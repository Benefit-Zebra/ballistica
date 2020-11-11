// Released under the MIT License. See LICENSE for details.

#ifndef BALLISTICA_UI_WIDGET_WIDGET_H_
#define BALLISTICA_UI_WIDGET_WIDGET_H_

#include <string>
#include <vector>

#include "ballistica/ballistica.h"
#include "ballistica/core/object.h"
#include "ballistica/platform/min_sdl.h"

namespace ballistica {

// Messages descriptions sent to widgets.
struct WidgetMessage {
  enum class Type {
    kEmptyMessage,
    kMoveUp,
    kMoveDown,
    kMoveLeft,
    kMoveRight,
    kActivate,
    kStart,
    kCancel,
    kShow,
    // In order to work in all-joystick environments,
    // don't rely on the following to be available (they're just a luxury).
    kKey,
    kTabNext,
    kTabPrev,
    kMouseDown,
    kMouseUp,
    kMouseWheel,
    kMouseWheelH,
    kMouseWheelVelocity,
    kMouseWheelVelocityH,
    kMouseMove,
    kScrollMouseDown,
    kTextInput
  };

  Type type{};
  bool has_keysym{};
  SDL_Keysym keysym{};
  float fval1{};
  float fval2{};
  float fval3{};
  float fval4{};
  std::string* sval{};

  explicit WidgetMessage(Type t = Type::kEmptyMessage,
                         const SDL_Keysym* k = nullptr, float f1 = 0,
                         float f2 = 0, float f3 = 0, float f4 = 0,
                         const char* s = nullptr)
      : type(t), has_keysym(false), fval1(f1), fval2(f2), fval3(f3), fval4(f4) {
    if (k) {
      keysym = *k;
      has_keysym = true;
    }
    if (s) {
      sval = new std::string();
      *sval = s;
    } else {
      sval = nullptr;
    }
  }
  ~WidgetMessage() { delete sval; }
};

// Base class for interface widgets.
class Widget : public Object {
 public:
  // Only relevant for direct children of the main stack widget.
  enum class ToolbarVisibility {
    kInherit = 0,            // for popups and whatnot - leave toolbar as-is
    kMenuMinimal = 1,        // menu, party, and back buttons
    kMenuMinimalNoBack = 2,  // menu and party buttons
    kMenuCurrency = 4,       // only menu, party, and currency
    kInGame = 8,             // only menu and party buttons
    kMenuFull = 16,          // everything
    kMenuFullRoot = 32       // everything minus back button plus a backing for
                             // visibility over scenes (obsolete?..)
  };

  Widget();
  ~Widget() override;

  // Activate the widget.
  virtual void Activate();

  // Draw the widget.
  // Widgets are drawn in 2 passes. The first is a front-to-back pass where
  // opaque parts should be drawn and the second is back-to-front where
  // transparent stuff should be drawn.
  virtual void Draw(RenderPass* pass, bool transparent);

  // Send a message to the widget; returns whether it was handled.
  virtual auto HandleMessage(const WidgetMessage& m) -> bool;

  // Whether the widget (or its children) is selectable in any way.
  virtual auto IsSelectable() -> bool;

  // Whether the widget can be selected by default with direction/tab presses.
  virtual auto IsSelectableViaKeys() -> bool;

  // Is the widget currently accepting input?
  // (containers transitioning out may return false here, etc).
  virtual auto IsAcceptingInput() const -> bool;

  void SetOnSelectCall(PyObject* call_obj);

  void AddOnDeleteCall(PyObject* call_obj);

  // Globally select this widget.
  void GlobalSelect();

  // Show this widget if possible (by scrolling to it, etc).
  void Show();

  // Returns true if the widget is the currently selected child of its parent
  // this does not mean that the parent is selected, however.
  auto selected() const -> bool { return selected_; }

  // Returns true if the widget hierarchy is selected (all of its parents).
  auto IsHierarchySelected() const -> bool;

  // Only really applicable to container widgets.
  void SetToolbarVisibility(ToolbarVisibility v);
  auto toolbar_visibility() const -> ToolbarVisibility {
    return toolbar_visibility_;
  }

  // FIXME: Replace this with GetBounds so we can do different alignments/etc.
  virtual auto GetWidth() -> float { return 0.0f; }
  virtual auto GetHeight() -> float { return 0.0f; }

  // If this widget is in a container, return it.
  auto parent_widget() const -> ContainerWidget* { return parent_widget_; }

  // Return the container_widget containing this widget, or the owner-widget if
  // there is no parent.
  auto GetOwnerWidget() const -> Widget*;

  auto down_widget() const -> Widget* { return down_widget_.get(); }
  void set_down_widget(Widget* w) {
    BA_PRECONDITION(!neighbors_locked_);
    down_widget_ = w;
  }
  auto up_widget() const -> Widget* { return up_widget_.get(); }
  void set_up_widget(Widget* w) {
    BA_PRECONDITION(!neighbors_locked_);
    up_widget_ = w;
  }
  auto left_widget() const -> Widget* { return left_widget_.get(); }
  void set_left_widget(Widget* w) {
    BA_PRECONDITION(!neighbors_locked_);
    left_widget_ = w;
  }
  auto right_widget() const -> Widget* { return right_widget_.get(); }
  void set_right_widget(Widget* w) {
    BA_PRECONDITION(!neighbors_locked_);
    right_widget_ = w;
  }

  void set_auto_select(bool enable) { auto_select_ = enable; }
  auto auto_select() const -> bool { return auto_select_; }

  // If neighbors are locked, calls to set the up/down/left/right widget
  // will fail. (useful for global toolbar widgets where we don't want users
  // redirecting them to transient per-window stuff).
  void set_neighbors_locked(bool locked) { neighbors_locked_ = locked; }

  // Widgets normally draw with a local depth range of 0-1.
  // It can be useful to limit drawing to a subsection of that region however
  // (for manually resolving overlap issues with widgets at the same depth,
  // etc).
  void SetDepthRange(float minDepth, float maxDepth);

  auto depth_range_min() const -> float { return depth_range_min_; }
  auto depth_range_max() const -> float { return depth_range_max_; }

  // For use by ContainerWidgets.
  // (we probably should just this functionality to all widgets)
  void set_parent_widget(ContainerWidget* c) { parent_widget_ = c; }

  auto IsInMainStack() const -> bool;
  auto IsInOverlayStack() const -> bool;

  // For use when embedding widgets inside others manually.
  // This will allow proper selection states/etc to trickle down to the
  // lowest-level child.
  void set_owner_widget(Widget* o) { owner_widget_ = o; }
  virtual auto GetWidgetTypeName() -> std::string { return "widget"; }
  virtual auto HasChildren() const -> bool { return false; }

  enum class SelectionCause { NEXT_SELECTED, PREV_SELECTED, NONE };

  void set_translate(float x, float y) {
    tx_ = x;
    ty_ = y;
  }
  void set_stack_offset(float x, float y) {
    stack_offset_x_ = x;
    stack_offset_y_ = y;
  }
  auto tx() const -> float { return tx_; }
  auto ty() const -> float { return ty_; }

  // Positional offset used when this widget is part of a stack.
  auto stack_offset_x() const -> float { return stack_offset_x_; }
  auto stack_offset_y() const -> float { return stack_offset_y_; }

  // Overall scale of the widget.
  auto scale() const -> float { return scale_; }
  void set_scale(float s) { scale_ = s; }

  // Return the widget's center in its parent's space.
  virtual void GetCenter(float* x, float* y);

  // Translates a point from screen space to widget space.
  void ScreenPointToWidget(float* x, float* y) const;
  void WidgetPointToScreen(float* x, float* y) const;

  // Draw-control parents are used to give one widget some basic visual control
  // over others, allowing them to inherit things like draw-brightness and tilt
  // shift (for cases such as images drawn over buttons).
  // Ideally we'd probably want to extend the parent mechanism for this, but
  // this works for now.
  auto draw_control_parent() const -> Widget* {
    return draw_control_parent_.get();
  }
  void set_draw_control_parent(Widget* w) { draw_control_parent_ = w; }

  // Can be used to ask link-parents how bright to draw.
  // Note: make sure the value returned here does not get changed when draw()
  // is run, since parts of draw-controlled children may query this before
  // draw() and parts after. (and they need to line up visually)
  virtual auto GetDrawBrightness(millisecs_t current_time) const -> float;

  // Extra buffer added around widgets when they are centered-on.
  void set_show_buffer_top(float b) { show_buffer_top_ = b; }
  void set_show_buffer_bottom(float b) { show_buffer_bottom_ = b; }
  void set_show_buffer_left(float b) { show_buffer_left_ = b; }
  void set_show_buffer_right(float b) { show_buffer_right_ = b; }

  auto show_buffer_top() const -> float { return show_buffer_top_; }
  auto show_buffer_bottom() const -> float { return show_buffer_bottom_; }
  auto show_buffer_left() const -> float { return show_buffer_left_; }
  auto show_buffer_right() const -> float { return show_buffer_right_; }

  auto NewPyRef() -> PyObject* { return GetPyWidget(true); }
  auto BorrowPyRef() -> PyObject* { return GetPyWidget(false); }

  auto has_py_ref() -> bool { return (py_ref_ != nullptr); }

  // For use by containers to flag widgets as invisible (for drawing
  // efficiency).
  void set_visible_in_container(bool val) { visible_in_container_ = val; }
  auto visible_in_container() const -> bool { return visible_in_container_; }

  virtual void OnLanguageChange() {}

  // Primitive janktastic child culling for use by containers.
  // (should really implement something more proper...)
  auto simple_culling_v() const -> float { return simple_culling_v_; }
  auto simple_culling_h() const -> float { return simple_culling_h_; }
  auto simple_culling_bottom() const -> float { return simple_culling_bottom_; }
  auto simple_culling_top() const -> float { return simple_culling_top_; }
  auto simple_culling_left() const -> float { return simple_culling_left_; }
  auto simple_culling_right() const -> float { return simple_culling_right_; }
  void set_simple_culling_h(float val) { simple_culling_h_ = val; }
  void set_simple_culling_v(float val) { simple_culling_v_ = val; }
  void set_simple_culling_left(float val) { simple_culling_left_ = val; }
  void set_simple_culling_right(float val) { simple_culling_right_ = val; }
  void set_simple_culling_bottom(float val) { simple_culling_bottom_ = val; }
  void set_simple_culling_top(float val) { simple_culling_top_ = val; }

 private:
  auto GetPyWidget(bool new_ref) -> PyObject*;
  virtual void SetSelected(bool s, SelectionCause cause);
  float simple_culling_h_{-1.0f};
  float simple_culling_v_{-1.0f};
  float simple_culling_left_{};
  float simple_culling_right_{};
  float simple_culling_bottom_{};
  float simple_culling_top_{};
  ToolbarVisibility toolbar_visibility_{ToolbarVisibility::kMenuMinimalNoBack};
  PyObject* py_ref_{};
  float show_buffer_top_{20.0f};
  float show_buffer_bottom_{20.0f};
  float show_buffer_left_{20.0f};
  float show_buffer_right_{20.0f};
  Object::WeakRef<Widget> draw_control_parent_;
  Object::WeakRef<Widget> down_widget_;
  Object::WeakRef<Widget> up_widget_;
  Object::WeakRef<Widget> left_widget_;
  Object::WeakRef<Widget> right_widget_;
  bool neighbors_locked_{};
  bool auto_select_{};
  ContainerWidget* parent_widget_{};
  Widget* owner_widget_{};
  bool selected_{};
  bool visible_in_container_{true};
  float tx_{};
  float ty_{};
  float stack_offset_x_{};
  float stack_offset_y_{};
  float scale_{1.0f};
  float depth_range_min_{};
  float depth_range_max_{1.0f};
  Object::Ref<PythonContextCall> on_select_call_;
  std::vector<Object::Ref<PythonContextCall> > on_delete_calls_;
  // FIXME: Should move container widget's functionality into ourself.
  friend class ContainerWidget;
};

}  // namespace ballistica

#endif  // BALLISTICA_UI_WIDGET_WIDGET_H_
