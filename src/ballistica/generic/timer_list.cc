// Released under the MIT License. See LICENSE for details.

#include "ballistica/generic/timer_list.h"

#include "ballistica/generic/runnable.h"
#include "ballistica/generic/timer.h"

namespace ballistica {

TimerList::TimerList() = default;

TimerList::~TimerList() {
  Clear();

  // Don't delete the client timer if one exists; just inform it that the list
  // is dead.
  if (client_timer_) {
    client_timer_->list_died_ = true;
  }

  if (g_buildconfig.debug_build()) {
    if (timer_count_active_ != 0) {
      Log("Error: Invalid timerlist state on teardown.");
    }
    if (timer_count_inactive_ != 0) {
      Log("Error: Invalid timerlist state on teardown.");
    }
    if (!((timer_count_total_ == 0)
          || (client_timer_ != nullptr && timer_count_total_ == 1))) {
      Log("Error: Invalid timerlist state on teardown.");
    }
  }
}

void TimerList::Clear() {
  assert(!are_clearing_);
  are_clearing_ = true;
  while (timers_) {
    Timer* t = timers_;
    t->on_list_ = false;
    timer_count_active_--;
    timers_ = t->next_;
    delete t;
  }
  while (timers_inactive_) {
    Timer* t = timers_inactive_;
    t->on_list_ = false;
    timer_count_inactive_--;
    timers_inactive_ = t->next_;
    delete t;
  }
  are_clearing_ = false;
}

// Pull a timer out of the list.
auto TimerList::PullTimer(int timer_id, bool remove) -> Timer* {
  Timer* t = timers_;
  Timer* p = nullptr;
  while (t) {
    if (t->id_ == timer_id) {
      if (remove) {
        if (p) {
          p->next_ = t->next_;
        } else {
          timers_ = t->next_;
        }
        t->on_list_ = false;
        timer_count_active_--;
      }
      return t;
    }
    p = t;
    t = t->next_;
  }

  // Didn't find it. check the inactive list.
  t = timers_inactive_;
  p = nullptr;
  while (t) {
    if (t->id_ == timer_id) {
      if (remove) {
        if (p) {
          p->next_ = t->next_;
        } else {
          timers_inactive_ = t->next_;
        }
        t->on_list_ = false;
        timer_count_inactive_--;
      }
      return t;
    }
    p = t;
    t = t->next_;
  }

  // Not on either list; only other possibility is the current client timer.
  if (client_timer_ && client_timer_->id_ == timer_id) {
    return client_timer_;
  }
  return nullptr;
}

void TimerList::Run(TimerMedium target_time) {
  assert(!are_clearing_);

  // Limit our runs to whats initially on the list so we don't spin all day if
  // a timer resets itself to run immediately.
  // FIXME - what if this timer kills one or more of the initially-expired ones
  //  ..that means it could potentially run more than once..  does it matter?
  int expired_count = GetExpiredCount(target_time);
  for (int timers_to_run = expired_count; timers_to_run > 0; timers_to_run--) {
    Timer* t = GetExpiredTimer(target_time);
    if (t) {
      assert(!t->dead_);
      try {
        t->runnable_->Run();
      } catch (const std::exception&) {
        // If something went wrong, put our list back in order and propagate.
        if (t->list_died_) {
          delete t;  // nothing is left but this timer
        } else {
          SubmitTimer(t);
        }
        throw;
      }
      // If this timer killed the list, stop; otherwise put it back and keep on
      // trucking.
      if (t->list_died_) {
        delete t;  // nothing is left but this timer
        return;
      } else {
        SubmitTimer(t);
      }
    }
  }
}
auto TimerList::GetExpiredCount(TimerMedium target_time) -> int {
  assert(!are_clearing_);

  Timer* t = timers_;
  int count = 0;
  while (t && t->expire_time_ <= target_time) {
    count++;
    t = t->next_;
  }
  return count;
}

// Returns the next expired timer.  When done with the timer,
// return it to the list with Timer::submit()
// (this will either put it back in line or delete it)
auto TimerList::GetExpiredTimer(TimerMedium target_time) -> Timer* {
  assert(!are_clearing_);

  Timer* t;
  if (timers_ != nullptr && timers_->expire_time_ <= target_time) {
    t = timers_;
    t->last_run_time_ = target_time;
    timers_ = timers_->next_;
    timer_count_active_--;
    t->on_list_ = false;

    // Exactly one timer at a time can be out in userland and not on
    // any list - this is now that one.
    assert(client_timer_ == nullptr);
    client_timer_ = t;
    return t;
  }
  return nullptr;
}

auto TimerList::NewTimer(TimerMedium current_time, TimerMedium length,
                         TimerMedium offset, int repeat_count,
                         const Object::Ref<Runnable>& runnable) -> Timer* {
  assert(!are_clearing_);
  auto* t = new Timer(this, next_timer_id_++, current_time, length, offset,
                      repeat_count);
  t->runnable_ = runnable;
  t = SubmitTimer(t);
  return t;
}

auto TimerList::GetTimeToNextExpire(TimerMedium current_time) -> TimerMedium {
  assert(!are_clearing_);
  if (!timers_) {
    return (TimerMedium)-1;
  }
  TimerMedium diff = timers_->expire_time_ - current_time;
  return (diff < 0) ? 0 : diff;
}

auto TimerList::GetTimer(int id) -> Timer* {
  assert(!are_clearing_);

  assert(id != 0);  // Zero denotes "no-id".
  Timer* t = PullTimer(id, false);
  return t->dead_ ? nullptr : t;
}

void TimerList::DeleteTimer(int timer_id) {
  assert(timer_id != 0);  // zero denotes "no-id"
  Timer* t = PullTimer(timer_id);
  if (t) {
    // If its the client timer, just mark it as dead, so the client can still
    // resubmit it without crashing.
    if (client_timer_ == t) {
      t->dead_ = true;
    } else {
      // Not in the client domain; kill it now.
      delete t;
    }
  }
}

auto TimerList::SubmitTimer(Timer* t) -> Timer* {
  assert(t->list_ == this);
  assert(t->initial_ || t == client_timer_ || t->dead_);

  // Aside from initial timer submissions, only the one client timer should be
  // coming thru here.
  if (!t->initial_) {
    assert(client_timer_ == t);
    client_timer_ = nullptr;
  }

  // If its a one-shot timer or is dead, kill it.
  if ((t->repeat_count_ == 0 && !t->initial_) || t->dead_) {
    delete t;
    return nullptr;
  } else {
    // Its still alive. Shove it back in line and tell it to keep working.
    if (!t->initial_ && t->repeat_count_ > 0) t->repeat_count_--;
    t->initial_ = false;

    // No drift.
    if (explicit_bool(false)) {
      t->expire_time_ = t->expire_time_ + t->length_;
    } else {
      // Drift.
      t->expire_time_ = t->last_run_time_ + t->length_;
    }
    AddTimer(t);
    return t;
  }
}

void TimerList::AddTimer(Timer* t) {
  assert(t && !t->on_list_);

  // If its set to never go off, throw it on the inactive list.
  if (t->length_ == -1) {
    t->next_ = timers_inactive_;
    timers_inactive_ = t;
    timer_count_inactive_++;
  } else {
    Timer** list = &timers_;

    // Go along till we find an expire time later than ourself.
    while (*list != nullptr) {
      if ((*list)->expire_time_ > t->expire_time_) break;
      list = &((*list)->next_);
    }
    Timer* tmp = (*list);
    (*list) = t;
    t->next_ = tmp;
    timer_count_active_++;
  }
  t->on_list_ = true;
}

}  // namespace ballistica
