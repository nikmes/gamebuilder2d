#include "services/hotkey/HotKeyManager.h"

#include <algorithm>
#include <array>
#include <exception>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "raylib.h"

#include <nlohmann/json.hpp>

#include "services/configuration/ConfigurationManager.h"
#include "services/hotkey/HotKeyCatalog.h"
#include "services/hotkey/ShortcutUtils.h"
#include "services/logger/LogManager.h"

namespace gb2d::hotkeys {

using json = nlohmann::json;

namespace {

    struct ConfigLoadStats;

    std::string trim(std::string_view text) {
        const std::size_t begin = text.find_first_not_of(" \t\r\n");
        if (begin == std::string_view::npos) {
            return {};
        }
        const std::size_t end = text.find_last_not_of(" \t\r\n");
        return std::string{text.substr(begin, end - begin + 1)};
    }

    std::string joinCommaSeparated(const std::vector<std::string>& items) {
        std::string result;
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (i > 0) {
                result += ", ";
            }
            result += items[i];
        }
        return result;
    }
    void logConfigLoadAnomalies(const ConfigLoadStats& stats, std::string_view context);
    void logConflictsIfAny();
    ConfigLoadStats applyConfigOverridesFromConfig();
    ConfigLoadStats reloadHotkeysFromConfig();

    bool g_initialized = false;
    std::vector<HotKeyAction> g_actions;
    std::unordered_map<std::string, std::size_t> g_actionIndex;

    struct ActionRuntimeState {
        ShortcutBinding binding;
        bool isActive{false};
        bool pendingTrigger{false};
        bool hasConflict{false};
        bool isCustom{false};
    };

    std::vector<ActionRuntimeState> g_actionRuntime;
    constexpr std::size_t suppressionIndex(HotKeySuppressionReason reason) {
        return static_cast<std::size_t>(reason);
    }

    constexpr std::size_t kSuppressionReasonCount = suppressionIndex(HotKeySuppressionReason::Count);

    std::array<std::uint32_t, kSuppressionReasonCount> g_suppressionCounts{};

    bool isSuppressedInternal() {
        return std::any_of(g_suppressionCounts.begin(), g_suppressionCounts.end(), [](std::uint32_t count) {
            return count > 0u;
        });
    }

    void clearSuppressedRuntimeState() {
        for (auto& runtime : g_actionRuntime) {
            runtime.isActive = false;
            runtime.pendingTrigger = false;
        }
    }

    std::vector<std::string> collectConflictsForIndex(std::size_t index) {
        std::vector<std::string> conflicts;
        if (index >= g_actionRuntime.size()) {
            return conflicts;
        }

        const auto& runtime = g_actionRuntime[index];
        if (!runtime.binding.valid) {
            return conflicts;
        }

        for (std::size_t i = 0; i < g_actionRuntime.size(); ++i) {
            if (i == index) {
                continue;
            }
            const auto& other = g_actionRuntime[i];
            if (!other.binding.valid) {
                continue;
            }
            if (!equalsShortcut(runtime.binding, other.binding)) {
                continue;
            }
            conflicts.push_back(g_actions[i].id);
        }

        std::sort(conflicts.begin(), conflicts.end());
        conflicts.erase(std::unique(conflicts.begin(), conflicts.end()), conflicts.end());
        return conflicts;
    }

    void recomputeConflicts() {
        for (auto& runtime : g_actionRuntime) {
            runtime.hasConflict = false;
        }

        std::unordered_map<std::uint64_t, std::vector<std::size_t>> groups;
        groups.reserve(g_actionRuntime.size());
        for (std::size_t i = 0; i < g_actionRuntime.size(); ++i) {
            const auto& runtime = g_actionRuntime[i];
            if (!runtime.binding.valid) {
                continue;
            }
            const std::uint64_t key = (static_cast<std::uint64_t>(runtime.binding.keyCode) << 32u) |
                                      static_cast<std::uint64_t>(runtime.binding.modifiers);
            groups[key].push_back(i);
        }

        for (const auto& entry : groups) {
            if (entry.second.size() <= 1) {
                continue;
            }
            for (const auto idx : entry.second) {
                if (idx >= g_actionRuntime.size()) {
                    continue;
                }
                auto& runtime = g_actionRuntime[idx];
                runtime.hasConflict = true;
                runtime.isActive = false;
                runtime.pendingTrigger = false;
            }
        }
    }

    HotKeyUpdateResult buildResult(HotKeyUpdateStatus status, std::size_t index, std::string message) {
        HotKeyUpdateResult result;
        result.status = status;
        result.message = std::move(message);
        if (index < g_actionRuntime.size()) {
            const auto& runtime = g_actionRuntime[index];
            result.binding = runtime.binding;
            result.isCustom = runtime.isCustom;
            result.hasConflict = runtime.hasConflict;
            result.conflictingActions = collectConflictsForIndex(index);
        }
        return result;
    }

    bool isEitherKeyDown(int leftKey, int rightKey) {
        return IsKeyDown(leftKey) || IsKeyDown(rightKey);
    }

    bool modifiersMatch(std::uint32_t modifiers) {
        const bool ctrlDown = isEitherKeyDown(KEY_LEFT_CONTROL, KEY_RIGHT_CONTROL);
        if (((modifiers & kModifierCtrl) != 0u) != ctrlDown) {
            return false;
        }

        const bool shiftDown = isEitherKeyDown(KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT);
        if (((modifiers & kModifierShift) != 0u) != shiftDown) {
            return false;
        }

        const bool altDown = isEitherKeyDown(KEY_LEFT_ALT, KEY_RIGHT_ALT);
        if (((modifiers & kModifierAlt) != 0u) != altDown) {
            return false;
        }

        const bool superDown = isEitherKeyDown(KEY_LEFT_SUPER, KEY_RIGHT_SUPER);
        if (((modifiers & kModifierSuper) != 0u) != superDown) {
            return false;
        }

        return true;
    }

    struct ConfigOverride {
        ShortcutBinding binding{};
        bool hasBinding{false};
        bool clearing{false};
    };

    struct ConfigLoadStats {
        std::size_t totalEntries{0};
        std::size_t appliedOverrides{0};
        std::size_t clearedOverrides{0};
        std::size_t unknownActions{0};
        std::size_t invalidEntries{0};
        std::size_t duplicateActions{0};
    };

    struct PreservedConfigArtifacts {
        std::vector<std::pair<std::size_t, json>> nonActionEntries;
        std::unordered_map<std::string, json> actionExtras;
    };

    PreservedConfigArtifacts capturePreservedArtifacts() {
        PreservedConfigArtifacts artifacts;

        const json& root = ConfigurationManager::raw();
        if (!root.is_object()) {
            return artifacts;
        }

        const auto inputIt = root.find("input");
        if (inputIt == root.end() || !inputIt->is_object()) {
            return artifacts;
        }

        const auto hotkeysIt = inputIt->find("hotkeys");
        if (hotkeysIt == inputIt->end() || !hotkeysIt->is_array()) {
            return artifacts;
        }

        std::size_t index = 0;
        for (const auto& entry : *hotkeysIt) {
            if (!entry.is_object()) {
                ++index;
                continue;
            }

            const auto actionIt = entry.find("action");
            if (actionIt != entry.end() && actionIt->is_string()) {
                std::string actionId = actionIt->get<std::string>();
                json extras = entry;
                extras.erase("action");
                extras.erase("shortcut");
                if (!extras.empty()) {
                    artifacts.actionExtras.emplace(std::move(actionId), std::move(extras));
                }
            } else {
                artifacts.nonActionEntries.emplace_back(index, entry);
            }

            ++index;
        }

        std::sort(artifacts.nonActionEntries.begin(), artifacts.nonActionEntries.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.first < rhs.first;
        });

        return artifacts;
    }

    ConfigLoadStats applyConfigOverridesFromConfig() {
        using gb2d::logging::LogManager;

        ConfigLoadStats stats{};

        const json& root = ConfigurationManager::raw();
        if (!root.is_object()) {
            LogManager::warn("Configuration root is not an object; hotkey defaults will be used.");
            return stats;
        }

        auto inputIt = root.find("input");
        if (inputIt == root.end()) {
            return stats;
        }
        if (!inputIt->is_object()) {
            LogManager::warn("Configuration section 'input' is not an object; hotkey defaults will be used.");
            return stats;
        }

        auto hotkeysIt = inputIt->find("hotkeys");
        if (hotkeysIt == inputIt->end()) {
            return stats;
        }
        if (!hotkeysIt->is_array()) {
            LogManager::warn("Configuration key 'input.hotkeys' is not an array; hotkey defaults will be used.");
            return stats;
        }

        std::unordered_map<std::string, ConfigOverride> overrides;
        overrides.reserve(hotkeysIt->size());

        std::size_t index = 0;
        for (const auto& item : *hotkeysIt) {
            ++stats.totalEntries;
            if (!item.is_object()) {
                LogManager::warn("Hotkeys config entry #{} ignored (expected object).", index);
                ++stats.invalidEntries;
                ++index;
                continue;
            }

            const auto actionIt = item.find("action");
            if (actionIt == item.end() || !actionIt->is_string()) {
                LogManager::warn("Hotkeys config entry #{} missing string 'action'; entry ignored.", index);
                ++stats.invalidEntries;
                ++index;
                continue;
            }

            std::string actionId = actionIt->get<std::string>();
            if (actionId.empty()) {
                LogManager::warn("Hotkeys config entry #{} has empty action id; entry ignored.", index);
                ++stats.invalidEntries;
                ++index;
                continue;
            }

            const auto knownIt = g_actionIndex.find(actionId);
            if (knownIt == g_actionIndex.end()) {
                LogManager::warn("Hotkeys config references unknown action '{}'; entry ignored.", actionId);
                ++stats.unknownActions;
                ++index;
                continue;
            }

            const auto shortcutIt = item.find("shortcut");
            if (shortcutIt == item.end()) {
                LogManager::warn("Hotkeys config entry for '{}' missing 'shortcut'; default binding kept.", actionId);
                ++stats.invalidEntries;
                ++index;
                continue;
            }

            ConfigOverride override{};
            if (shortcutIt->is_null()) {
                override.clearing = true;
            } else if (shortcutIt->is_string()) {
                const std::string shortcutText = shortcutIt->get<std::string>();
                const std::string trimmedShortcut = trim(shortcutText);
                if (trimmedShortcut.empty()) {
                    override.clearing = true;
                } else {
                    ShortcutBinding binding = parseShortcut(trimmedShortcut);
                    if (!binding.valid) {
                        LogManager::warn("Hotkeys config shortcut '{}' for '{}' is invalid; default binding kept.", shortcutText, actionId);
                        ++stats.invalidEntries;
                        ++index;
                        continue;
                    }
                    override.binding = binding;
                    override.hasBinding = true;
                }
            } else {
                LogManager::warn("Hotkeys config shortcut for '{}' has unsupported type; default binding kept.", actionId);
                ++stats.invalidEntries;
                ++index;
                continue;
            }

            if (overrides.contains(actionId)) {
                LogManager::warn("Hotkeys config contains duplicate entry for '{}'; later value overrides earlier.", actionId);
                ++stats.duplicateActions;
            }

            overrides[actionId] = override;
            ++index;
        }

        for (std::size_t i = 0; i < g_actions.size() && i < g_actionRuntime.size(); ++i) {
            auto& runtime = g_actionRuntime[i];
            runtime.binding = g_actions[i].defaultBinding;
            runtime.isCustom = false;
            runtime.isActive = false;
            runtime.pendingTrigger = false;
            runtime.hasConflict = false;

            const auto it = overrides.find(g_actions[i].id);
            if (it == overrides.end()) {
                continue;
            }

            if (it->second.clearing) {
                runtime.binding = ShortcutBinding{};
                runtime.isCustom = true;
                ++stats.clearedOverrides;
                continue;
            }

            if (it->second.hasBinding) {
                runtime.binding = it->second.binding;
                runtime.isCustom = !equalsShortcut(runtime.binding, g_actions[i].defaultBinding);
                if (runtime.isCustom) {
                    ++stats.appliedOverrides;
                }
            }
        }

        recomputeConflicts();
        return stats;
    }

    void logConfigLoadAnomalies(const ConfigLoadStats& stats, std::string_view context) {
        using gb2d::logging::LogManager;
        if (stats.invalidEntries == 0 && stats.unknownActions == 0 && stats.duplicateActions == 0) {
            return;
        }

        LogManager::warn("Hotkeys config {} encountered {} invalid entries, {} unknown actions, {} duplicates.",
                         context,
                         stats.invalidEntries,
                         stats.unknownActions,
                         stats.duplicateActions);
    }

    void logConflictsIfAny() {
        if (!HotKeyManager::hasConflicts()) {
            return;
        }

        const auto conflictsList = HotKeyManager::conflicts();
        for (const auto& conflict : conflictsList) {
            const std::string shortcutLabel = conflict.binding.valid ? toString(conflict.binding) : std::string{"Unassigned"};
            gb2d::logging::LogManager::warn("Hotkey '{}' conflicts with [{}] on shortcut '{}'.",
                                            conflict.actionId,
                                            joinCommaSeparated(conflict.conflictingActions),
                                            shortcutLabel);
        }
    }

    ConfigLoadStats reloadHotkeysFromConfig() {
        ConfigLoadStats stats = applyConfigOverridesFromConfig();
        logConfigLoadAnomalies(stats, "reload");
        logConflictsIfAny();
        return stats;
    }
}

nlohmann::json HotKeyManager::exportBindingsJson() {
    PreservedConfigArtifacts artifacts = capturePreservedArtifacts();

    std::vector<json> actionEntries;
    actionEntries.reserve(g_actions.size());

    const std::size_t managedCount = std::min(g_actions.size(), g_actionRuntime.size());
    for (std::size_t i = 0; i < managedCount; ++i) {
        json entry = json::object();
        entry["action"] = g_actions[i].id;

        const auto& runtime = g_actionRuntime[i];
        if (runtime.binding.valid) {
            entry["shortcut"] = toString(runtime.binding);
        } else {
            entry["shortcut"] = nullptr;
        }

        const auto extrasIt = artifacts.actionExtras.find(g_actions[i].id);
        if (extrasIt != artifacts.actionExtras.end()) {
            for (auto it = extrasIt->second.begin(); it != extrasIt->second.end(); ++it) {
                entry[it.key()] = it.value();
            }
        }

        actionEntries.push_back(std::move(entry));
    }

    // In the unlikely event runtime contains additional anonymous entries, emit them for visibility.
    for (std::size_t i = g_actions.size(); i < g_actionRuntime.size(); ++i) {
        json entry = json::object();
        entry["action"] = std::string{"__runtime."} + std::to_string(i);
        const auto& runtime = g_actionRuntime[i];
        if (runtime.binding.valid) {
            entry["shortcut"] = toString(runtime.binding);
        } else {
            entry["shortcut"] = nullptr;
        }
        actionEntries.push_back(std::move(entry));
    }

    json serialized = json::array();
    std::size_t position = 0;
    std::size_t commentCursor = 0;

    for (auto& actionEntry : actionEntries) {
        while (commentCursor < artifacts.nonActionEntries.size() && artifacts.nonActionEntries[commentCursor].first <= position) {
            serialized.push_back(artifacts.nonActionEntries[commentCursor].second);
            ++commentCursor;
            ++position;
        }

        serialized.push_back(std::move(actionEntry));
        ++position;
    }

    while (commentCursor < artifacts.nonActionEntries.size()) {
        serialized.push_back(artifacts.nonActionEntries[commentCursor].second);
        ++commentCursor;
    }

    return serialized;
}

bool HotKeyManager::persistBindings() {
    using gb2d::logging::LogManager;

    const std::size_t customCount = std::count_if(g_actionRuntime.begin(), g_actionRuntime.end(), [](const ActionRuntimeState& state) {
        return state.isCustom;
    });
    const std::size_t clearedCount = std::count_if(g_actionRuntime.begin(), g_actionRuntime.end(), [](const ActionRuntimeState& state) {
        return state.isCustom && !state.binding.valid;
    });

    json payload = exportBindingsJson();

    try {
        ConfigurationManager::setJson("input.hotkeys", payload);
    } catch (const std::exception& ex) {
        LogManager::error("Failed to stage hotkey configuration: {}", ex.what());
        return false;
    } catch (...) {
        LogManager::error("Failed to stage hotkey configuration due to an unknown error.");
        return false;
    }

    if (ConfigurationManager::save()) {
        LogManager::info("Hotkeys saved ({} actions, {} custom overrides, {} cleared).",
                         g_actions.size(),
                         customCount,
                         clearedCount);
        return true;
    }

    LogManager::error("Hotkey configuration save failed while writing disk file.");
    return false;
}

bool HotKeyManager::applyOverridesFromConfigForReload() {
    using gb2d::logging::LogManager;

    if (!g_initialized) {
        LogManager::warn("HotKeyManager reload requested before initialization; ignoring.");
        return false;
    }

    const ConfigLoadStats stats = reloadHotkeysFromConfig();

    const std::size_t customCount = std::count_if(g_actionRuntime.begin(), g_actionRuntime.end(), [](const ActionRuntimeState& state) {
        return state.isCustom;
    });
    const std::size_t clearedCount = std::count_if(g_actionRuntime.begin(), g_actionRuntime.end(), [](const ActionRuntimeState& state) {
        return state.isCustom && !state.binding.valid;
    });

    LogManager::info("Hotkeys reloaded ({} actions, {} custom overrides, {} cleared).",
                     g_actions.size(),
                     customCount,
                     clearedCount);

    return true;
}

bool HotKeyManager::initialize() {
    if (g_initialized) {
        return true;
    }
    using gb2d::logging::LogManager;

    g_suppressionCounts.fill(0u);

    clearRegistrations();
    registerActions(buildDefaultCatalog());

    ConfigLoadStats stats = applyConfigOverridesFromConfig();

    const std::size_t customCount = std::count_if(g_actionRuntime.begin(), g_actionRuntime.end(), [](const ActionRuntimeState& state) {
        return state.isCustom;
    });
    const std::size_t clearedCount = std::count_if(g_actionRuntime.begin(), g_actionRuntime.end(), [](const ActionRuntimeState& state) {
        return state.isCustom && !state.binding.valid;
    });

    logConfigLoadAnomalies(stats, "load");

    LogManager::info("HotKeyManager initialized with {} actions ({} custom overrides, {} cleared).",
                     g_actions.size(),
                     customCount,
                     clearedCount);

    logConflictsIfAny();

    ConfigurationManager::pushReloadHook({
        .name = "HotKeyManager::reload",
        .callback = []() {
            const bool success = HotKeyManager::applyOverridesFromConfigForReload();
            if (!success) {
                gb2d::logging::LogManager::error("HotKeyManager failed to reload configuration overrides.");
            }
        }
    });

    g_initialized = true;
    return true;
}

void HotKeyManager::shutdown() {
    if (!g_initialized) {
        return;
    }
    clearRegistrations();
    g_suppressionCounts.fill(0u);
    g_initialized = false;
}

bool HotKeyManager::isInitialized() {
    return g_initialized;
}

void HotKeyManager::tick() {
    if (!g_initialized) {
        return;
    }

    if (isSuppressedInternal()) {
        clearSuppressedRuntimeState();
        return;
    }

    for (std::size_t i = 0; i < g_actionRuntime.size(); ++i) {
        auto& runtime = g_actionRuntime[i];
        runtime.isActive = false;

        const ShortcutBinding& binding = runtime.binding;
        if (!binding.valid || binding.keyCode == 0u) {
            continue;
        }

        if (runtime.hasConflict) {
            continue;
        }

        if (!modifiersMatch(binding.modifiers)) {
            continue;
        }

        const int keyCode = static_cast<int>(binding.keyCode);
        const bool keyDown = IsKeyDown(keyCode);
        const bool keyPressed = IsKeyPressed(keyCode);

        runtime.isActive = keyDown;
        if (keyPressed) {
            runtime.pendingTrigger = true;
        }
    }
}

void HotKeyManager::registerActions(const HotKeyRegistration& registration) {
    for (const auto& action : registration.actions) {
        if (action.id.empty()) {
            continue;
        }
        const auto it = g_actionIndex.find(action.id);
        if (it == g_actionIndex.end()) {
            g_actionIndex.emplace(action.id, g_actions.size());
            g_actions.push_back(action);
            ActionRuntimeState state{};
            state.binding = action.defaultBinding;
            g_actionRuntime.push_back(state);
        } else {
            g_actions[it->second] = action;
            if (it->second < g_actionRuntime.size()) {
                g_actionRuntime[it->second].binding = action.defaultBinding;
                g_actionRuntime[it->second].isActive = false;
                g_actionRuntime[it->second].pendingTrigger = false;
                g_actionRuntime[it->second].hasConflict = false;
                g_actionRuntime[it->second].isCustom = false;
            }
        }
    }

    recomputeConflicts();
}

void HotKeyManager::clearRegistrations() {
    g_actions.clear();
    g_actionIndex.clear();
    g_actionRuntime.clear();
}

const std::vector<HotKeyAction>& HotKeyManager::actions() {
    return g_actions;
}

const HotKeyAction* HotKeyManager::findAction(const std::string& actionId) {
    const auto it = g_actionIndex.find(actionId);
    if (it == g_actionIndex.end()) {
        return nullptr;
    }
    return &g_actions[it->second];
}

const ShortcutBinding* HotKeyManager::binding(const std::string& actionId) {
    const auto it = g_actionIndex.find(actionId);
    if (it == g_actionIndex.end()) {
        return nullptr;
    }
    const std::size_t index = it->second;
    if (index >= g_actionRuntime.size()) {
        return nullptr;
    }
    return &g_actionRuntime[index].binding;
}

bool HotKeyManager::isPressed(const std::string& actionId) {
    const auto it = g_actionIndex.find(actionId);
    if (it == g_actionIndex.end()) {
        return false;
    }
    const std::size_t index = it->second;
    if (index >= g_actionRuntime.size()) {
        return false;
    }
    const auto& runtime = g_actionRuntime[index];
    if (runtime.hasConflict) {
        return false;
    }
    return runtime.isActive;
}

bool HotKeyManager::consumeTriggered(const std::string& actionId) {
    const auto it = g_actionIndex.find(actionId);
    if (it == g_actionIndex.end()) {
        return false;
    }
    const std::size_t index = it->second;
    if (index >= g_actionRuntime.size()) {
        return false;
    }

    auto& runtime = g_actionRuntime[index];
    if (runtime.hasConflict) {
        runtime.pendingTrigger = false;
        return false;
    }
    if (!runtime.pendingTrigger) {
        return false;
    }

    runtime.pendingTrigger = false;
    return true;
}

std::vector<const HotKeyAction*> HotKeyManager::consumeTriggeredActions() {
    std::vector<const HotKeyAction*> triggered;
    triggered.reserve(g_actions.size());

    for (std::size_t i = 0; i < g_actionRuntime.size(); ++i) {
        auto& runtime = g_actionRuntime[i];
        if (runtime.hasConflict) {
            runtime.pendingTrigger = false;
            continue;
        }
        if (!runtime.pendingTrigger) {
            continue;
        }
        runtime.pendingTrigger = false;
        triggered.push_back(&g_actions[i]);
    }

    return triggered;
}

HotKeyUpdateResult HotKeyManager::setBinding(const std::string& actionId, const ShortcutBinding& binding) {
    const auto it = g_actionIndex.find(actionId);
    if (it == g_actionIndex.end()) {
        HotKeyUpdateResult result;
        result.status = HotKeyUpdateStatus::ActionNotFound;
        result.message = "Action not found.";
        return result;
    }

    const std::size_t index = it->second;
    if (index >= g_actionRuntime.size()) {
        HotKeyUpdateResult result;
        result.status = HotKeyUpdateStatus::ActionNotFound;
        result.message = "Action runtime state missing.";
        return result;
    }

    ShortcutBinding normalized = binding;
    if (binding.valid) {
        normalized = buildShortcut(binding.keyCode, binding.modifiers, binding.keyToken);
    } else {
        normalized = buildShortcut(binding.keyCode, binding.modifiers, binding.keyToken);
    }

    if (!normalized.valid) {
        return buildResult(HotKeyUpdateStatus::InvalidBinding, index, "Shortcut binding is invalid.");
    }

    auto& runtime = g_actionRuntime[index];
    if (runtime.binding.valid && equalsShortcut(runtime.binding, normalized)) {
        return buildResult(HotKeyUpdateStatus::NoChange, index, "Shortcut unchanged.");
    }

    runtime.binding = normalized;
    runtime.isCustom = true;
    runtime.isActive = false;
    runtime.pendingTrigger = false;

    recomputeConflicts();

    auto result = buildResult(HotKeyUpdateStatus::Applied, index, "Shortcut applied.");
    if (result.hasConflict) {
        result.message = "Shortcut applied but conflicts with other actions.";
    }
    return result;
}

HotKeyUpdateResult HotKeyManager::clearBinding(const std::string& actionId) {
    const auto it = g_actionIndex.find(actionId);
    if (it == g_actionIndex.end()) {
        HotKeyUpdateResult result;
        result.status = HotKeyUpdateStatus::ActionNotFound;
        result.message = "Action not found.";
        return result;
    }

    const std::size_t index = it->second;
    if (index >= g_actionRuntime.size()) {
        HotKeyUpdateResult result;
        result.status = HotKeyUpdateStatus::ActionNotFound;
        result.message = "Action runtime state missing.";
        return result;
    }

    auto& runtime = g_actionRuntime[index];
    if (!runtime.binding.valid && runtime.isCustom) {
        return buildResult(HotKeyUpdateStatus::NoChange, index, "Shortcut already cleared.");
    }

    runtime.binding = ShortcutBinding{};
    runtime.isCustom = true;
    runtime.isActive = false;
    runtime.pendingTrigger = false;

    recomputeConflicts();

    return buildResult(HotKeyUpdateStatus::Cleared, index, "Shortcut cleared.");
}

HotKeyUpdateResult HotKeyManager::restoreDefaultBinding(const std::string& actionId) {
    const auto it = g_actionIndex.find(actionId);
    if (it == g_actionIndex.end()) {
        HotKeyUpdateResult result;
        result.status = HotKeyUpdateStatus::ActionNotFound;
        result.message = "Action not found.";
        return result;
    }

    const std::size_t index = it->second;
    if (index >= g_actionRuntime.size()) {
        HotKeyUpdateResult result;
        result.status = HotKeyUpdateStatus::ActionNotFound;
        result.message = "Action runtime state missing.";
        return result;
    }

    auto& runtime = g_actionRuntime[index];
    const auto& defaults = g_actions[index].defaultBinding;

    if (runtime.binding.valid && equalsShortcut(runtime.binding, defaults) && !runtime.isCustom) {
        return buildResult(HotKeyUpdateStatus::NoChange, index, "Shortcut already at default.");
    }

    runtime.binding = defaults;
    runtime.isCustom = false;
    runtime.isActive = false;
    runtime.pendingTrigger = false;

    recomputeConflicts();

    return buildResult(HotKeyUpdateStatus::RestoredDefault, index, "Shortcut restored to default.");
}

void HotKeyManager::restoreAllDefaults() {
    for (std::size_t i = 0; i < g_actionRuntime.size(); ++i) {
        auto& runtime = g_actionRuntime[i];
        if (i < g_actions.size()) {
            runtime.binding = g_actions[i].defaultBinding;
        } else {
            runtime.binding = ShortcutBinding{};
        }
        runtime.isCustom = false;
        runtime.isActive = false;
        runtime.pendingTrigger = false;
        runtime.hasConflict = false;
    }

    recomputeConflicts();
}

bool HotKeyManager::hasConflicts() {
    return std::any_of(g_actionRuntime.begin(), g_actionRuntime.end(), [](const ActionRuntimeState& state) {
        return state.hasConflict;
    });
}

std::vector<HotKeyConflictInfo> HotKeyManager::conflicts() {
    std::vector<HotKeyConflictInfo> info;
    info.reserve(g_actionRuntime.size());

    for (std::size_t i = 0; i < g_actionRuntime.size(); ++i) {
        const auto& runtime = g_actionRuntime[i];
        if (!runtime.hasConflict) {
            continue;
        }
        HotKeyConflictInfo entry;
        entry.binding = runtime.binding;
        entry.actionId = g_actions[i].id;
        entry.conflictingActions = collectConflictsForIndex(i);
        info.push_back(std::move(entry));
    }

    return info;
}

bool HotKeyManager::isCustomBinding(const std::string& actionId) {
    const auto it = g_actionIndex.find(actionId);
    if (it == g_actionIndex.end()) {
        return false;
    }
    const std::size_t index = it->second;
    if (index >= g_actionRuntime.size()) {
        return false;
    }
    return g_actionRuntime[index].isCustom;
}

bool HotKeyManager::actionHasConflict(const std::string& actionId) {
    const auto it = g_actionIndex.find(actionId);
    if (it == g_actionIndex.end()) {
        return false;
    }
    const std::size_t index = it->second;
    if (index >= g_actionRuntime.size()) {
        return false;
    }
    return g_actionRuntime[index].hasConflict;
}

void HotKeyManager::pushSuppression(HotKeySuppressionReason reason) {
    const auto idx = suppressionIndex(reason);
    if (idx >= g_suppressionCounts.size()) {
        return;
    }
    ++g_suppressionCounts[idx];
}

void HotKeyManager::popSuppression(HotKeySuppressionReason reason) {
    const auto idx = suppressionIndex(reason);
    if (idx >= g_suppressionCounts.size()) {
        return;
    }
    auto& count = g_suppressionCounts[idx];
    if (count == 0u) {
        return;
    }
    --count;
    if (!isSuppressedInternal()) {
        clearSuppressedRuntimeState();
    }
}

bool HotKeyManager::isSuppressed() {
    return isSuppressedInternal();
}

bool HotKeyManager::isSuppressed(HotKeySuppressionReason reason) {
    const auto idx = suppressionIndex(reason);
    if (idx >= g_suppressionCounts.size()) {
        return false;
    }
    return g_suppressionCounts[idx] > 0u;
}

ScopedHotKeySuppression::ScopedHotKeySuppression(HotKeySuppressionReason reason) noexcept
    : reason_(reason), active_(true) {
    HotKeyManager::pushSuppression(reason_);
}

ScopedHotKeySuppression::ScopedHotKeySuppression(ScopedHotKeySuppression&& other) noexcept
    : reason_(other.reason_), active_(other.active_) {
    other.active_ = false;
}

ScopedHotKeySuppression& ScopedHotKeySuppression::operator=(ScopedHotKeySuppression&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (active_) {
        HotKeyManager::popSuppression(reason_);
    }

    reason_ = other.reason_;
    active_ = other.active_;
    other.active_ = false;

    return *this;
}

ScopedHotKeySuppression::~ScopedHotKeySuppression() {
    if (active_) {
        HotKeyManager::popSuppression(reason_);
    }
}

void ScopedHotKeySuppression::release() noexcept {
    if (!active_) {
        return;
    }
    HotKeyManager::popSuppression(reason_);
    active_ = false;
}

} // namespace gb2d::hotkeys
