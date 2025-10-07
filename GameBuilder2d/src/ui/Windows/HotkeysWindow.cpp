#include "ui/Windows/HotkeysWindow.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "imgui.h"

#include "services/hotkey/ShortcutUtils.h"
#include "services/logger/LogManager.h"

namespace gb2d {

namespace {

using gb2d::hotkeys::ShortcutBinding;
using gb2d::hotkeys::kModifierAlt;
using gb2d::hotkeys::kModifierCtrl;
using gb2d::hotkeys::kModifierShift;
using gb2d::hotkeys::kModifierSuper;
using gb2d::hotkeys::equalsShortcut;

std::string canonicalTokenForLabel(const char* label) {
    if (label == nullptr) {
        return {};
    }

    const std::string name{label};
    if (name.empty()) {
        return {};
    }

    if (name == "Shift" || name == "Ctrl" || name == "Alt") {
        return {};
    }

    if (name == "~") {
        return "`";
    }
    if (name == "+") {
        return "=";
    }
    if (name == "|") {
        return "\\";
    }
    if (name == "Ret") {
        return "Enter";
    }
    if (name == "Caps Lock") {
        return "CapsLock";
    }
    if (name == "PgU") {
        return "PageUp";
    }
    if (name == "PgD") {
        return "PageDown";
    }
    if (name == "Hom") {
        return "Home";
    }
    if (name == "Del") {
        return "Delete";
    }
    if (name == "Ins") {
        return "Insert";
    }
    if (name == "PrSn") {
        return "PrintScreen";
    }
    if (name == "ScLk") {
        return "ScrollLock";
    }
    if (name == "Brk") {
        return "Pause";
    }

    return name;
}

const std::unordered_map<std::string, unsigned char>& tokenToScanMap() {
    static const std::unordered_map<std::string, unsigned char> map = [] {
        std::unordered_map<std::string, unsigned char> result;
        for (unsigned int row = 0; row < 6; ++row) {
            for (int col = 0; ImHotKey::Keys[row][col].lib != nullptr; ++col) {
                const ImHotKey::Key& key = ImHotKey::Keys[row][col];
                std::string token = canonicalTokenForLabel(key.lib);
                if (token.empty()) {
                    continue;
                }
                result[token] = static_cast<unsigned char>(key.scanCodePage1 & 0xFFu);
            }
        }

        // Provide a couple of aliases commonly used in serialized bindings.
        auto itEnter = result.find("Enter");
        if (itEnter != result.end()) {
            result["Return"] = itEnter->second;
        }

        return result;
    }();

    return map;
}

unsigned int bindingToFunctionKeys(const ShortcutBinding& binding) {
    if (!binding.valid) {
        return 0u;
    }

    std::array<unsigned char, 4> scanCodes{0xFF, 0xFF, 0xFF, 0xFF};
    std::array<unsigned char, 4> order{0xFF, 0xFF, 0xFF, 0xFF};
    std::size_t index = 0;

    auto addScanCode = [&](unsigned char scancode) {
        if (index >= scanCodes.size() || scancode == 0xFF) {
            return;
        }
        scanCodes[index] = scancode;
        order[index] = static_cast<unsigned char>(ImHotKey::GetKeyForScanCode(scancode).order & 0xFFu);
        ++index;
    };

    if ((binding.modifiers & kModifierCtrl) != 0u) {
        addScanCode(0x1D);
    }
    if ((binding.modifiers & kModifierShift) != 0u) {
        addScanCode(0x2A);
    }
    if ((binding.modifiers & kModifierAlt) != 0u) {
        addScanCode(0x38);
    }
    if ((binding.modifiers & kModifierSuper) != 0u) {
        addScanCode(0x5B);
    }

    if (!binding.keyToken.empty()) {
        const auto& mapping = tokenToScanMap();
        const auto it = mapping.find(binding.keyToken);
        if (it != mapping.end()) {
            addScanCode(it->second);
        } else {
            static std::unordered_set<std::string> warnedTokens;
            if (!warnedTokens.contains(binding.keyToken)) {
                warnedTokens.insert(binding.keyToken);
                gb2d::logging::LogManager::warn("HotkeysWindow missing scancode mapping for token '{}'.", binding.keyToken);
            }
        }
    }

    return ImHotKey::GetOrderedScanCodes(scanCodes.data(), order.data());
}

ShortcutBinding functionKeysToBinding(unsigned int encoded) {
    if (encoded == 0u) {
        return {};
    }

    char buffer[128] = {0};
    ImHotKey::GetHotKeyLib(encoded, buffer, sizeof(buffer), nullptr);
    if (buffer[0] == '\0') {
        return {};
    }

    ShortcutBinding parsed = gb2d::hotkeys::parseShortcut(buffer);
    if (!parsed.valid) {
        return {};
    }
    return parsed;
}

bool bindingsEqual(const ShortcutBinding& lhs, const ShortcutBinding& rhs) {
    if (!lhs.valid && !rhs.valid) {
        return true;
    }
    if (lhs.valid != rhs.valid) {
        return false;
    }
    return equalsShortcut(lhs, rhs);
}

std::string composeHotKeyLib(const std::string& description, const std::string& context) {
    if (!description.empty()) {
        return description;
    }
    if (!context.empty()) {
        return std::string{"Context: "} + context;
    }
    return std::string{};
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

} // namespace

HotkeysWindow::HotkeysWindow()
    : title_("Hotkeys") {
}

const char* HotkeysWindow::typeId() const {
    return "hotkeys";
}

const char* HotkeysWindow::displayName() const {
    return "Hotkeys";
}

std::string HotkeysWindow::title() const {
    return title_;
}

void HotkeysWindow::setTitle(std::string newTitle) {
    if (!newTitle.empty()) {
        title_ = std::move(newTitle);
    }
}

std::optional<Size> HotkeysWindow::minSize() const {
    return Size{720, 420};
}

void HotkeysWindow::render(WindowContext&) {
    ensureInitialized();

    if (!gb2d::hotkeys::HotKeyManager::isInitialized()) {
        ImGui::TextUnformatted("HotKeyManager hasn't been initialized yet.");
        ImGui::TextUnformatted("Ensure the service starts during application bootstrap.");
        if (ImGui::Button("Retry")) {
            refreshActions();
        }
        return;
    }

    ensureConflictState();

    const bool blockingIssues = hasBlockingIssues();

    if (!statusMessage_.empty()) {
        ImVec4 color{0.68f, 0.82f, 0.68f, 1.0f};
        switch (statusLevel_) {
        case StatusLevel::Info:
            color = ImVec4{0.68f, 0.82f, 0.68f, 1.0f};
            break;
        case StatusLevel::Warning:
            color = ImVec4{0.95f, 0.79f, 0.38f, 1.0f};
            break;
        case StatusLevel::Error:
            color = ImVec4{0.86f, 0.27f, 0.27f, 1.0f};
            break;
        }
        ImGui::TextColored(color, "%s", statusMessage_.c_str());
        ImGui::Spacing();
    }

    if (ImGui::Button("Reload from manager")) {
        refreshActions();
        setStatus(StatusLevel::Info, "Hotkeys reloaded from manager.");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu actions", actions_.size());

    ImGui::Spacing();
    ImGui::TextWrapped("Select an action and click Edit to open the ImHotKey capture widget. Use Apply to push changes to the running editor and Save to persist them to disk. Restore Defaults and Clear All operate on staged values until applied.");
    ImGui::Spacing();

    drawControls();

    if (blockingIssues) {
        std::size_t conflictCount = 0;
        std::size_t invalidCount = 0;
        for (const auto& action : actions_) {
            if (action.hasConflict) {
                ++conflictCount;
            }
            if (action.captureRejected) {
                ++invalidCount;
            }
        }

        const ImVec4 errorColor{0.86f, 0.27f, 0.27f, 1.0f};
        if (conflictCount > 0) {
            ImGui::TextColored(errorColor, "%zu action%s have conflicting shortcuts.", conflictCount, conflictCount == 1 ? "" : "s");
        }
        if (invalidCount > 0) {
            ImGui::TextColored(errorColor, "%zu action%s have invalid shortcut captures.", invalidCount, invalidCount == 1 ? "" : "s");
        }
        ImGui::TextColored(errorColor, "Resolve the issues above to enable apply/save controls.");
        ImGui::Spacing();
    }

    drawActions();
}

void HotkeysWindow::ensureInitialized() {
    if (initialized_) {
        return;
    }
    if (!gb2d::hotkeys::HotKeyManager::isInitialized()) {
        return;
    }
    refreshActions();
}

void HotkeysWindow::refreshActions() {
    actions_.clear();

    if (!gb2d::hotkeys::HotKeyManager::isInitialized()) {
        initialized_ = false;
        return;
    }

    const auto& registered = gb2d::hotkeys::HotKeyManager::actions();
    actions_.reserve(registered.size());

    for (const auto& action : registered) {
        ActionEntry entry;
        entry.id = action.id;
        entry.label = action.label.empty() ? action.id : action.label;
        entry.category = action.category.empty() ? std::string{"Misc"} : action.category;
        entry.context = action.context;
        entry.description = action.description;
        entry.defaultBinding = action.defaultBinding;

        if (const ShortcutBinding* runtime = gb2d::hotkeys::HotKeyManager::binding(action.id)) {
            entry.originalBinding = *runtime;
            entry.currentBinding = *runtime;
        } else {
            entry.originalBinding = {};
            entry.currentBinding = {};
        }

        entry.hotKeyName = entry.label;
        entry.hotKeyLib = composeHotKeyLib(entry.description, entry.context);
        entry.hotKey.functionName = entry.hotKeyName.c_str();
        entry.hotKey.functionLib = entry.hotKeyLib.empty() ? nullptr : entry.hotKeyLib.c_str();
        entry.hotKey.functionKeys = bindingToFunctionKeys(entry.currentBinding);
        entry.originalFunctionKeys = entry.hotKey.functionKeys;
        entry.lastFunctionKeys = entry.hotKey.functionKeys;

        actions_.push_back(std::move(entry));
    }

    std::sort(actions_.begin(), actions_.end(), [](const ActionEntry& lhs, const ActionEntry& rhs) {
        if (lhs.category == rhs.category) {
            return lhs.label < rhs.label;
        }
        return lhs.category < rhs.category;
    });

    initialized_ = true;
    conflictsDirty_ = true;
}

void HotkeysWindow::drawActions() {
    if (actions_.empty()) {
        ImGui::TextDisabled("No actions registered with the HotKeyManager.");
        return;
    }

    // Build grouped view by category while preserving sorted order.
    std::size_t index = 0;
    while (index < actions_.size()) {
        std::string currentCategory = actions_[index].category;
        std::vector<ActionEntry*> group;
        while (index < actions_.size() && actions_[index].category == currentCategory) {
            group.push_back(&actions_[index]);
            ++index;
        }
        drawCategory(currentCategory, group);
    }
}

void HotkeysWindow::drawControls() {
    const bool managerReady = gb2d::hotkeys::HotKeyManager::isInitialized();
    const bool dirty = anyDirty();
    const bool blockingIssues = hasBlockingIssues();

    ImGui::BeginGroup();
    ImGui::BeginDisabled(!managerReady);
    if (ImGui::Button("Restore All Defaults")) {
        restoreAllDefaults();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All Bindings")) {
        clearAllBindings();
    }
    ImGui::EndDisabled();
    ImGui::EndGroup();

    ImGui::BeginGroup();
    ImGui::BeginDisabled(!managerReady || !dirty);
    if (ImGui::Button("Discard Changes")) {
        discardChanges();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!managerReady || blockingIssues || !dirty);
    if (ImGui::Button("Apply Changes")) {
        applyChanges(false);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!managerReady || blockingIssues);
    if (ImGui::Button("Save to Config")) {
        applyChanges(true);
    }
    ImGui::EndDisabled();
    ImGui::EndGroup();

    ImGui::Spacing();
}

void HotkeysWindow::drawCategory(const std::string& category, std::vector<ActionEntry*>& items) {
    if (items.empty()) {
        return;
    }

    ImGui::PushID(category.c_str());
    const std::string headerLabel = category.empty() ? std::string{"Misc"} : category;
    const bool open = ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

    if (open) {
        if (ImGui::BeginTable("hotkey_table", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 220.0f);
            ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("Context", ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (auto* entry : items) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(entry->label.c_str());
                ImGui::SameLine();
                const std::string editLabel = std::string{"Edit##"} + entry->id;
                if (ImGui::SmallButton(editLabel.c_str())) {
                    beginEdit(*entry);
                }
                ImGui::SameLine();
                const bool canClear = entry->currentBinding.valid;
                const std::string clearLabel = std::string{"Clear##"} + entry->id;
                ImGui::BeginDisabled(!canClear);
                if (ImGui::SmallButton(clearLabel.c_str())) {
                    clearActionBinding(*entry);
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                const bool canRestoreDefault = !bindingsEqual(entry->currentBinding, entry->defaultBinding);
                const std::string defaultLabel = std::string{"Default##"} + entry->id;
                ImGui::BeginDisabled(!canRestoreDefault);
                if (ImGui::SmallButton(defaultLabel.c_str())) {
                    restoreActionDefault(*entry);
                }
                ImGui::EndDisabled();
                if (isDirty(*entry)) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.95f, 0.72f, 0.18f, 1.0f), "modified");
                }
                if (entry->hasConflict) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.86f, 0.27f, 0.27f, 1.0f), "conflict");
                }
                if (entry->captureRejected) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.86f, 0.27f, 0.27f, 1.0f), "invalid");
                }

                ImGui::TableSetColumnIndex(1);
                if (entry->currentBinding.valid) {
                    ImGui::TextUnformatted(entry->currentBinding.humanReadable.c_str());
                } else {
                    ImGui::TextDisabled("Unassigned");
                }
                if (entry->hasConflict) {
                    const std::string conflicts = joinCommaSeparated(entry->conflictLabels);
                    if (!conflicts.empty()) {
                        ImGui::TextColored(ImVec4(0.86f, 0.27f, 0.27f, 1.0f), "Conflicts with: %s", conflicts.c_str());
                    } else {
                        ImGui::TextColored(ImVec4(0.86f, 0.27f, 0.27f, 1.0f), "Conflicts with other actions.");
                    }
                }
                if (entry->captureRejected) {
                    ImGui::TextColored(ImVec4(0.86f, 0.27f, 0.27f, 1.0f), "Last capture was invalid; shortcut unchanged.");
                }

                ImGui::TableSetColumnIndex(2);
                if (!entry->context.empty()) {
                    ImGui::TextUnformatted(entry->context.c_str());
                } else {
                    ImGui::TextUnformatted("Global");
                }

                ImGui::TableSetColumnIndex(3);
                if (!entry->description.empty()) {
                    ImGui::TextWrapped("%s", entry->description.c_str());
                } else {
                    ImGui::TextDisabled("No description provided.");
                }
            }

            ImGui::EndTable();
        }
    }

    ImGui::PopID();

    for (auto* entry : items) {
        handleEditModal(*entry);
    }
}

void HotkeysWindow::beginEdit(ActionEntry& entry) {
    entry.modalRequested = true;
    entry.captureRejected = false;
}

void HotkeysWindow::handleEditModal(ActionEntry& entry) {
    const std::string popupId = std::string{"HotkeyEditor##"} + entry.id;

    if (entry.modalRequested) {
        entry.hotKeyName = entry.label;
        entry.hotKeyLib = composeHotKeyLib(entry.description, entry.context);
        entry.hotKey.functionName = entry.hotKeyName.c_str();
        entry.hotKey.functionLib = entry.hotKeyLib.empty() ? nullptr : entry.hotKeyLib.c_str();
        entry.hotKey.functionKeys = bindingToFunctionKeys(entry.currentBinding);
        entry.lastFunctionKeys = entry.hotKey.functionKeys;
        entry.modalRequested = false;
        entry.modalOpen = true;
        ImGui::OpenPopup(popupId.c_str());
    }

    if (!entry.modalOpen) {
        return;
    }

    entry.hotKey.functionName = entry.hotKeyName.c_str();
    entry.hotKey.functionLib = entry.hotKeyLib.empty() ? nullptr : entry.hotKeyLib.c_str();

    ImHotKey::Edit(&entry.hotKey, 1, popupId.c_str());

    if (!ImGui::IsPopupOpen(popupId.c_str())) {
        entry.modalOpen = false;
        bool appliedChange = false;

        if (entry.hotKey.functionKeys == 0u) {
            entry.currentBinding = {};
            entry.captureRejected = false;
            appliedChange = true;
        } else {
            ShortcutBinding parsed = functionKeysToBinding(entry.hotKey.functionKeys);
            if (parsed.valid) {
                entry.currentBinding = parsed;
                entry.captureRejected = false;
                appliedChange = true;
            } else {
                // Revert to previous binding if the captured combo was not valid (e.g., only modifiers).
                entry.captureRejected = true;
                entry.hotKey.functionKeys = entry.lastFunctionKeys;
            }
        }

        entry.hotKey.functionName = entry.hotKeyName.c_str();
        entry.hotKey.functionLib = entry.hotKeyLib.empty() ? nullptr : entry.hotKeyLib.c_str();
        entry.hotKey.functionKeys = bindingToFunctionKeys(entry.currentBinding);
        if (appliedChange) {
            entry.lastFunctionKeys = entry.hotKey.functionKeys;
        }
        conflictsDirty_ = true;
    }
}

bool HotkeysWindow::isDirty(const ActionEntry& entry) const {
    return !bindingsEqual(entry.currentBinding, entry.originalBinding);
}

void HotkeysWindow::ensureConflictState() {
    if (!initialized_) {
        return;
    }
    if (!conflictsDirty_) {
        return;
    }
    recomputeConflicts();
    conflictsDirty_ = false;
}

void HotkeysWindow::recomputeConflicts() {
    std::unordered_map<std::uint64_t, std::vector<ActionEntry*>> groups;
    groups.reserve(actions_.size());

    for (auto& entry : actions_) {
        entry.hasConflict = false;
        entry.conflictLabels.clear();
    }

    for (auto& entry : actions_) {
        if (!entry.currentBinding.valid) {
            continue;
        }
        const std::uint64_t key = (static_cast<std::uint64_t>(entry.currentBinding.keyCode) << 32u) |
                                  static_cast<std::uint64_t>(entry.currentBinding.modifiers);
        groups[key].push_back(&entry);
    }

    for (auto& pair : groups) {
        auto& entries = pair.second;
        if (entries.size() <= 1) {
            continue;
        }

        for (auto* entry : entries) {
            entry->hasConflict = true;
            entry->conflictLabels.clear();
            entry->conflictLabels.reserve(entries.size() - 1);
            for (auto* other : entries) {
                if (other == entry) {
                    continue;
                }
                entry->conflictLabels.push_back(other->label.empty() ? other->id : other->label);
            }
            std::sort(entry->conflictLabels.begin(), entry->conflictLabels.end());
            entry->conflictLabels.erase(std::unique(entry->conflictLabels.begin(), entry->conflictLabels.end()), entry->conflictLabels.end());
        }
    }
}

bool HotkeysWindow::hasBlockingIssues() const {
    for (const auto& entry : actions_) {
        if (entry.hasConflict || entry.captureRejected) {
            return true;
        }
    }
    return false;
}

bool HotkeysWindow::anyDirty() const {
    return std::any_of(actions_.begin(), actions_.end(), [this](const ActionEntry& entry) {
        return isDirty(entry);
    });
}

void HotkeysWindow::clearActionBinding(ActionEntry& entry) {
    if (!entry.currentBinding.valid) {
        setStatus(StatusLevel::Info, "Shortcut already cleared.");
        return;
    }

    entry.currentBinding = {};
    entry.hotKey.functionKeys = 0u;
    entry.lastFunctionKeys = 0u;
    entry.captureRejected = false;
    entry.hotKey.functionName = entry.hotKeyName.c_str();
    entry.hotKey.functionLib = entry.hotKeyLib.empty() ? nullptr : entry.hotKeyLib.c_str();

    conflictsDirty_ = true;
    ensureConflictState();

    setStatus(StatusLevel::Info, std::string{"Cleared shortcut for '"} + entry.label + "'. Apply to commit.");
}

void HotkeysWindow::restoreActionDefault(ActionEntry& entry) {
    if (bindingsEqual(entry.currentBinding, entry.defaultBinding)) {
        setStatus(StatusLevel::Info, "Shortcut already at default.");
        return;
    }

    entry.currentBinding = entry.defaultBinding;
    entry.hotKey.functionKeys = bindingToFunctionKeys(entry.currentBinding);
    entry.lastFunctionKeys = entry.hotKey.functionKeys;
    entry.captureRejected = false;
    entry.hotKey.functionName = entry.hotKeyName.c_str();
    entry.hotKey.functionLib = entry.hotKeyLib.empty() ? nullptr : entry.hotKeyLib.c_str();

    conflictsDirty_ = true;
    ensureConflictState();

    setStatus(StatusLevel::Info, std::string{"Restored default shortcut for '"} + entry.label + "'. Apply to commit.");
}

void HotkeysWindow::restoreAllDefaults() {
    bool changed = false;
    for (auto& entry : actions_) {
        if (!bindingsEqual(entry.currentBinding, entry.defaultBinding) || entry.captureRejected) {
            entry.currentBinding = entry.defaultBinding;
            entry.hotKey.functionKeys = bindingToFunctionKeys(entry.currentBinding);
            entry.lastFunctionKeys = entry.hotKey.functionKeys;
            entry.captureRejected = false;
            entry.hotKey.functionName = entry.hotKeyName.c_str();
            entry.hotKey.functionLib = entry.hotKeyLib.empty() ? nullptr : entry.hotKeyLib.c_str();
            changed = true;
        }
    }

    if (changed) {
        conflictsDirty_ = true;
        ensureConflictState();
        setStatus(StatusLevel::Info, "All shortcuts reset to defaults. Apply to commit.");
    } else {
        setStatus(StatusLevel::Info, "All shortcuts already match their defaults.");
    }
}

void HotkeysWindow::clearAllBindings() {
    bool changed = false;
    for (auto& entry : actions_) {
        if (entry.currentBinding.valid || entry.captureRejected) {
            entry.currentBinding = {};
            entry.hotKey.functionKeys = 0u;
            entry.lastFunctionKeys = 0u;
            entry.captureRejected = false;
            entry.hotKey.functionName = entry.hotKeyName.c_str();
            entry.hotKey.functionLib = entry.hotKeyLib.empty() ? nullptr : entry.hotKeyLib.c_str();
            changed = true;
        }
    }

    if (changed) {
        conflictsDirty_ = true;
        ensureConflictState();
        setStatus(StatusLevel::Info, "All shortcuts cleared. Apply to commit.");
    } else {
        setStatus(StatusLevel::Info, "All shortcuts are already unassigned.");
    }
}

void HotkeysWindow::discardChanges() {
    bool changed = false;
    for (auto& entry : actions_) {
        if (!bindingsEqual(entry.currentBinding, entry.originalBinding) || entry.captureRejected) {
            entry.currentBinding = entry.originalBinding;
            entry.hotKey.functionKeys = bindingToFunctionKeys(entry.currentBinding);
            entry.lastFunctionKeys = entry.hotKey.functionKeys;
            entry.captureRejected = false;
            entry.hotKey.functionName = entry.hotKeyName.c_str();
            entry.hotKey.functionLib = entry.hotKeyLib.empty() ? nullptr : entry.hotKeyLib.c_str();
            changed = true;
        }
    }

    if (changed) {
        conflictsDirty_ = true;
        ensureConflictState();
        setStatus(StatusLevel::Info, "Staged changes discarded.");
    } else {
        setStatus(StatusLevel::Info, "No staged changes to discard.");
    }
}

bool HotkeysWindow::applyChanges(bool persistToDisk) {
    using gb2d::hotkeys::HotKeyManager;
    using gb2d::hotkeys::HotKeyUpdateResult;
    using gb2d::hotkeys::HotKeyUpdateStatus;

    if (!HotKeyManager::isInitialized()) {
        setStatus(StatusLevel::Error, "HotKeyManager is not initialized.");
        return false;
    }

    bool dirty = false;
    bool hadErrors = false;
    std::size_t appliedCount = 0;
    std::size_t clearedCount = 0;
    std::size_t defaultCount = 0;
    std::size_t unchangedCount = 0;

    for (auto& entry : actions_) {
        if (!isDirty(entry)) {
            continue;
        }
        dirty = true;

        HotKeyUpdateResult result;
        if (!entry.currentBinding.valid && !entry.defaultBinding.valid) {
            result = HotKeyManager::restoreDefaultBinding(entry.id);
        } else if (!entry.currentBinding.valid) {
            result = HotKeyManager::clearBinding(entry.id);
        } else if (bindingsEqual(entry.currentBinding, entry.defaultBinding)) {
            result = HotKeyManager::restoreDefaultBinding(entry.id);
        } else {
            result = HotKeyManager::setBinding(entry.id, entry.currentBinding);
        }

        switch (result.status) {
        case HotKeyUpdateStatus::Applied:
            ++appliedCount;
            break;
        case HotKeyUpdateStatus::Cleared:
            ++clearedCount;
            break;
        case HotKeyUpdateStatus::RestoredDefault:
            ++defaultCount;
            break;
        case HotKeyUpdateStatus::NoChange:
            ++unchangedCount;
            break;
        case HotKeyUpdateStatus::ActionNotFound:
        case HotKeyUpdateStatus::InvalidBinding:
            hadErrors = true;
            setStatus(StatusLevel::Error, std::string{"Failed to update '"} + entry.label + "': " + result.message);
            break;
        }

        if (hadErrors) {
            break;
        }

        entry.originalBinding = result.binding;
        entry.currentBinding = result.binding;
        entry.hotKey.functionKeys = bindingToFunctionKeys(entry.currentBinding);
        entry.lastFunctionKeys = entry.hotKey.functionKeys;
        entry.originalFunctionKeys = entry.hotKey.functionKeys;
        entry.captureRejected = false;
        entry.hotKey.functionName = entry.hotKeyName.c_str();
        entry.hotKey.functionLib = entry.hotKeyLib.empty() ? nullptr : entry.hotKeyLib.c_str();
    }

    if (hadErrors) {
        conflictsDirty_ = true;
        refreshActions();
        ensureConflictState();
        return false;
    }

    if (dirty) {
        conflictsDirty_ = true;
        ensureConflictState();
    }

    if (persistToDisk) {
        if (!gb2d::hotkeys::HotKeyManager::persistBindings()) {
            setStatus(StatusLevel::Error, "Failed to save hotkeys to configuration.");
            return false;
        }
    }

    if (!dirty) {
        if (persistToDisk) {
            setStatus(StatusLevel::Info, "Hotkey configuration saved. No staged changes were pending.");
        } else {
            setStatus(StatusLevel::Info, "No staged changes to apply.");
        }
        return true;
    }

    std::vector<std::string> summaryParts;
    if (appliedCount > 0) {
        summaryParts.push_back(std::to_string(appliedCount) + (appliedCount == 1 ? " update" : " updates"));
    }
    if (clearedCount > 0) {
        summaryParts.push_back(std::to_string(clearedCount) + (clearedCount == 1 ? " clear" : " clears"));
    }
    if (defaultCount > 0) {
        summaryParts.push_back(std::to_string(defaultCount) + (defaultCount == 1 ? " default restore" : " default restores"));
    }
    if (summaryParts.empty()) {
        summaryParts.push_back("no effective changes");
    }

    std::string message = "Applied " + joinCommaSeparated(summaryParts) + '.';
    if (persistToDisk) {
        message += " Configuration saved.";
    } else {
        message += " Use Save to persist to disk.";
    }

    const bool stillBlocking = hasBlockingIssues();
    if (stillBlocking) {
        message += " Some shortcuts still conflict.";
        setStatus(StatusLevel::Warning, std::move(message));
    } else {
        setStatus(StatusLevel::Info, std::move(message));
    }

    return true;
}

void HotkeysWindow::setStatus(StatusLevel level, std::string message) {
    statusLevel_ = level;
    statusMessage_ = std::move(message);
}

} // namespace gb2d
