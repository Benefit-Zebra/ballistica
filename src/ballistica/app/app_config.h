// Released under the MIT License. See LICENSE for details.

#ifndef BALLISTICA_APP_APP_CONFIG_H_
#define BALLISTICA_APP_APP_CONFIG_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ballistica {

// This class wrangles user config values for the app.
// The underlying config data currently lives in the Python layer,
// so at the moment these calls are only usable from the game thread,
// but that may change in the future.
class AppConfig {
 public:
  // Our official config values:

  enum class FloatID {
    kScreenGamma,
    kScreenPixelScale,
    kTouchControlsScale,
    kTouchControlsScaleMovement,
    kTouchControlsScaleActions,
    kSoundVolume,
    kMusicVolume,
    kGoogleVRRenderTargetScale,
    kLast  // Sentinel.
  };

  enum class StringID {
    kResolutionAndroid,
    kTouchActionControlType,
    kTouchMovementControlType,
    kGraphicsQuality,
    kTextureQuality,
    kVerticalSync,
    kVRHeadRelativeAudio,
    kMacControllerSubsystem,
    kTelnetPassword,
    kLast  // Sentinel.
  };

  enum class IntID {
    kPort,
    kTelnetPort,
    kLast  // Sentinel.
  };

  enum class BoolID {
    kTouchControlsSwipeHidden,
    kFullscreen,
    kKickIdlePlayers,
    kAlwaysUseInternalKeyboard,
    kShowFPS,
    kTVBorder,
    kKeyboardP2Enabled,
    kEnablePackageMods,
    kChatMuted,
    kEnableRemoteApp,
    kEnableTelnet,
    kDisableCameraShake,
    kDisableCameraGyro,
    kLast  // Sentinel.
  };

  class Entry {
   public:
    enum class Type { kString, kInt, kFloat, kBool };
    Entry() = default;
    explicit Entry(const char* name) : name_(name) {}
    virtual auto GetType() const -> Type = 0;
    auto name() const -> const std::string& { return name_; }
    virtual auto FloatValue() const -> float;
    virtual auto StringValue() const -> std::string;
    virtual auto IntValue() const -> int;
    virtual auto BoolValue() const -> bool;
    virtual auto DefaultFloatValue() const -> float;
    virtual auto DefaultStringValue() const -> std::string;
    virtual auto DefaultIntValue() const -> int;
    virtual auto DefaultBoolValue() const -> bool;

   private:
    std::string name_;
  };

  static void Init();
  AppConfig();

  // Given specific ids, returns resolved values (fastest access).
  auto Resolve(FloatID id) -> float;
  auto Resolve(StringID id) -> std::string;
  auto Resolve(IntID id) -> int;
  auto Resolve(BoolID id) -> bool;

  // Given a name, returns an entry (or nullptr).
  // You should check the entry's type and request
  // the corresponding typed resolved value from it.
  auto GetEntry(const std::string& name) -> const Entry* {
    auto i = entries_by_name_.find(name);
    if (i == entries_by_name_.end()) {
      return nullptr;
    }
    return i->second;
  }

  auto entries_by_name() const -> const std::map<std::string, const Entry*>& {
    return entries_by_name_;
  }

 private:
  class StringEntry;
  class FloatEntry;
  class IntEntry;
  class BoolEntry;
  template <typename T>
  void CompleteMap(const T& entry_map);
  void SetupEntries();
  std::map<std::string, const Entry*> entries_by_name_;
  std::map<FloatID, FloatEntry> float_entries_;
  std::map<IntID, IntEntry> int_entries_;
  std::map<StringID, StringEntry> string_entries_;
  std::map<BoolID, BoolEntry> bool_entries_;
};

}  // namespace ballistica

#endif  // BALLISTICA_APP_APP_CONFIG_H_
