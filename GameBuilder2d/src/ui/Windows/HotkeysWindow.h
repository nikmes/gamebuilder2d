#pragma once

#include <string>
#include <vector>
#include <optional>

#include "ui/Window.h"
#include "services/hotkey/HotKeyManager.h"
#ifdef _WIN32
extern "C" unsigned int __stdcall MapVirtualKeyA(unsigned int, unsigned int);
#ifndef MAPVK_VK_TO_VSC
#define MAPVK_VK_TO_VSC 0
#endif
#endif
#include "imHotKey.h"

namespace gb2d {

class HotkeysWindow : public IWindow {
public:
    HotkeysWindow();

    const char* typeId() const override;
    const char* displayName() const override;
    std::string title() const override;
    void setTitle(std::string newTitle) override;
    std::optional<Size> minSize() const override;

    void render(WindowContext& ctx) override;

private:
    struct ActionEntry {
        std::string id;
        std::string label;
        std::string category;
        std::string context;
        std::string description;

        gb2d::hotkeys::ShortcutBinding originalBinding{};
        gb2d::hotkeys::ShortcutBinding currentBinding{};
        gb2d::hotkeys::ShortcutBinding defaultBinding{};

        std::string hotKeyName;
        std::string hotKeyLib;
        ImHotKey::HotKey hotKey{};
        unsigned int lastFunctionKeys{0};
        unsigned int originalFunctionKeys{0};
        bool modalRequested{false};
        bool modalOpen{false};
        bool hasConflict{false};
        bool captureRejected{false};
        std::vector<std::string> conflictLabels;
    };

    enum class StatusLevel {
        Info,
        Warning,
        Error
    };

    void ensureInitialized();
    void ensureConflictState();
    void refreshActions();
    void drawActions();
    void drawCategory(const std::string& category, std::vector<ActionEntry*>& items);
    void beginEdit(ActionEntry& entry);
    void handleEditModal(ActionEntry& entry);
    bool isDirty(const ActionEntry& entry) const;
    bool anyDirty() const;
    void clearActionBinding(ActionEntry& entry);
    void restoreActionDefault(ActionEntry& entry);
    void restoreAllDefaults();
    void clearAllBindings();
    void discardChanges();
    bool applyChanges(bool persistToDisk);
    void setStatus(StatusLevel level, std::string message);
    void drawControls();
    void recomputeConflicts();
    bool hasBlockingIssues() const;

    std::string title_;
    bool initialized_{false};
    std::vector<ActionEntry> actions_;
    bool conflictsDirty_{true};
    StatusLevel statusLevel_{StatusLevel::Info};
    std::string statusMessage_;
};

} // namespace gb2d
