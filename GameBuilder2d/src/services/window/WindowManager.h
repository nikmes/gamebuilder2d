#pragma once
#include <string>
#include <vector>
#include <optional>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include "raylib.h"
#include "Window.h"
#include "DockRegion.h"
#include "Layout.h"
// ImGuiColorTextEdit usage moved to modular windows
// Modular window registry scaffolding
#include "ui/WindowRegistry.h"

// Include logging definitions (LogLine) needed for incremental log console state.
#include "services/logger/LogManager.h"

struct ImGuiIO;

namespace gb2d {

class FullscreenSession;

class WindowManager {
public:
    WindowManager();
    ~WindowManager();

    // Explicit, idempotent teardown. Safe to call multiple times. Ensures
    // resources that depend on ImGui / raylib are released while those
    // subsystems are still alive (before global/static shutdown).
    void shutdown();

    struct ManagedWindow {
        std::string id;
        std::string title;
        bool open{true};
        std::optional<Size> minSize{}; // optional per-window min size
        // Optional modular implementation (new window system). When present,
        // title/minSize should be sourced from impl at render-time.
        std::unique_ptr<IWindow> impl{};
    };

    const Layout& getLayout() const;

    // Render ImGui-based UI elements (docking, tabs to be implemented later)
    void renderUI();

    void syncHotkeySuppression(const ImGuiIO* imguiIO, bool imguiFrameActive);

    void setFullscreenSession(FullscreenSession* session);

    std::string createWindow(const std::string& title, std::optional<Size> initialSize = std::nullopt);
    // New: spawn a window by registered type. Returns empty string on failure.
    std::string spawnWindowByType(const std::string& typeId,
                                  std::optional<std::string> desiredTitle = std::nullopt,
                                  std::optional<Size> initialSize = std::nullopt);
    bool dockWindow(const std::string& windowId, const std::string& targetRegionId, DockPosition position);
    bool undockWindow(const std::string& windowId);
    bool closeWindow(const std::string& windowId);

    bool reorderTabs(const std::string& regionId, const std::vector<std::string>& newOrder);
    bool resizeRegion(const std::string& regionId, int deltaWidth, int deltaHeight);

    bool saveLayout(const std::optional<std::string>& name = std::nullopt);
    bool loadLayout(const std::string& name);

private:
    void buildDefaultLayoutIfNeeded();
    const ManagedWindow* findByTitle(const std::string& title) const;
    ManagedWindow* findByTitle(const std::string& title);
    ManagedWindow* findByTypeId(const std::string& typeId) {
        auto it = std::find_if(windows_.begin(), windows_.end(), [&](const ManagedWindow& w){
            return w.impl && std::string(w.impl->typeId()) == typeId;
        });
        if (it == windows_.end()) return nullptr;
        return &(*it);
    }

    std::string makeLabel(const ManagedWindow& w) const;
    void renderDockTargetsOverlay();
    void toggleEditorFullscreen();
    void setEditorFullscreen(bool enable);
    void processGlobalHotkeys();
    void openFileDialog(const char* dialogTitle, const char* filters);

    struct Toast {
        std::string text;
        float remaining;
    };
    void addToast(const std::string& text, float seconds = 2.0f);
    void updateToasts(float dt);
    void renderToasts();

    // Legacy file preview support migrated to FilePreviewWindow (modular)
    void cleanupPreview(const std::string& windowId);

    Layout layout_{};
    std::vector<ManagedWindow> windows_{};
    std::unordered_set<std::string> undock_requests_{};
    unsigned int dockspace_id_{0};
    int next_id_{1};
    bool layout_built_{false};
    std::vector<Toast> toasts_{};
    std::string last_folder_{};
    std::vector<std::string> recent_files_{}; // MRU list, most-recent first
    void addRecentFile(const std::string& path, size_t cap = 10);
    // previews_ removed; handled by FilePreviewWindow instances

    // DnD docking overlay + constraints
    std::optional<std::string> dragging_window_id_{}; // set while user drags our handle
    std::optional<std::string> focus_request_window_id_{}; // focus a window after render
    int min_dock_width_{200};
    int min_dock_height_{120};

    // Console window migrated to ConsoleLogWindow (modular); no console-specific state here.

    // Editor and preview logic migrated to modular windows

    // No console helpers; handled by ConsoleLogWindow.

    // Shutdown state guard
    bool shutting_down_{false};

    // Modular windows registry (scaffolding; will be used as windows are migrated)
    WindowRegistry window_registry_{};

    FullscreenSession* fullscreen_session_{nullptr};
    int editor_window_restore_width_{0};
    int editor_window_restore_height_{0};

    bool hotkey_suppressed_text_input_{false};
    bool hotkey_suppressed_modal_{false};
};

} // namespace gb2d
