#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace gb2d::hotkeys {

struct ShortcutBinding {
    std::string humanReadable;   // Canonical textual representation (e.g., "Ctrl+O")
    std::string keyToken;        // Canonical key token (e.g., "O", "F5", "Enter")
    std::uint32_t keyCode{0};    // Normalized key code (raylib KeyboardKey or synthesized ID)
    std::uint32_t modifiers{0};  // Bitmask of modifier flags
    bool valid{false};
};

struct HotKeyAction {
    std::string id;             // e.g., "global.openFileDialog"
    std::string label;          // Human-friendly label for UI display
    std::string category;       // Grouping (Global, Code Editor, etc.)
    std::string context;        // Activation scope (e.g., "Global", "Code Editor")
    ShortcutBinding defaultBinding;
    std::string description;    // Optional tooltip or help text
};

struct HotKeyRegistration {
    std::vector<HotKeyAction> actions;
};

enum class HotKeyUpdateStatus {
    Applied,
    Cleared,
    RestoredDefault,
    NoChange,
    ActionNotFound,
    InvalidBinding,
};

struct HotKeyUpdateResult {
    HotKeyUpdateStatus status{HotKeyUpdateStatus::NoChange};
    ShortcutBinding binding{};
    bool isCustom{false};
    bool hasConflict{false};
    std::vector<std::string> conflictingActions;
    std::string message;
};

struct HotKeyConflictInfo {
    std::string actionId;
    ShortcutBinding binding;
    std::vector<std::string> conflictingActions;
};

enum class HotKeySuppressionReason : std::uint8_t {
    TextInput = 0,
    ModalDialog,
    ExplicitPause,
    Count,
};

class ScopedHotKeySuppression;

class HotKeyManager {
public:
    static bool initialize();
    static void shutdown();
    static bool isInitialized();

    static void tick();

    static void registerActions(const HotKeyRegistration& registration);
    static void clearRegistrations();

    static const std::vector<HotKeyAction>& actions();
    static const HotKeyAction* findAction(const std::string& actionId);

    static const ShortcutBinding* binding(const std::string& actionId);
    static bool isPressed(const std::string& actionId);
    static bool consumeTriggered(const std::string& actionId);
    static std::vector<const HotKeyAction*> consumeTriggeredActions();

    static HotKeyUpdateResult setBinding(const std::string& actionId, const ShortcutBinding& binding);
    static HotKeyUpdateResult clearBinding(const std::string& actionId);
    static HotKeyUpdateResult restoreDefaultBinding(const std::string& actionId);
    static void restoreAllDefaults();

    static bool hasConflicts();
    static std::vector<HotKeyConflictInfo> conflicts();

    static bool isCustomBinding(const std::string& actionId);
    static bool actionHasConflict(const std::string& actionId);

    [[nodiscard]] static nlohmann::json exportBindingsJson();
    static bool persistBindings();

    static void pushSuppression(HotKeySuppressionReason reason);
    static void popSuppression(HotKeySuppressionReason reason);
    static bool isSuppressed();
    static bool isSuppressed(HotKeySuppressionReason reason);

    // Additional persistence, configuration integration, and context suppression APIs will
    // follow in subsequent tasks.

private:
    static bool applyOverridesFromConfigForReload();
};

class ScopedHotKeySuppression {
public:
    explicit ScopedHotKeySuppression(HotKeySuppressionReason reason) noexcept;
    ScopedHotKeySuppression(const ScopedHotKeySuppression&) = delete;
    ScopedHotKeySuppression& operator=(const ScopedHotKeySuppression&) = delete;
    ScopedHotKeySuppression(ScopedHotKeySuppression&& other) noexcept;
    ScopedHotKeySuppression& operator=(ScopedHotKeySuppression&& other) noexcept;
    ~ScopedHotKeySuppression();

    void release() noexcept;

private:
    HotKeySuppressionReason reason_;
    bool active_;
};

} // namespace gb2d::hotkeys
