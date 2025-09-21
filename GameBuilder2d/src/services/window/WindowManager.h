#pragma once
#include <string>
#include <vector>
#include <optional>
#include <unordered_set>
#include <unordered_map>
#include "raylib.h"
#include "Window.h"
#include "DockRegion.h"
#include "Layout.h"

namespace gb2d {

class WindowManager {
public:
    WindowManager();
    ~WindowManager();

    struct ManagedWindow {
        std::string id;
        std::string title;
        bool open{true};
        std::optional<Size> minSize{}; // optional per-window min size
    };

    const Layout& getLayout() const;

    // Render ImGui-based UI elements (docking, tabs to be implemented later)
    void renderUI();

    std::string createWindow(const std::string& title, std::optional<Size> initialSize = std::nullopt);
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

    std::string makeLabel(const ManagedWindow& w) const;
    void renderDockTargetsOverlay();

    struct Toast {
        std::string text;
        float remaining;
    };
    void addToast(const std::string& text, float seconds = 2.0f);
    void updateToasts(float dt);
    void renderToasts();

    // Simple file preview support
    struct Preview {
        enum class Kind { None, Text, Image };
        Kind kind{Kind::None};
        std::string path;
        std::string text;
        int imgWidth{0};
        int imgHeight{0};
        unsigned int texId{0}; // raylib Texture2D.id
        bool loaded{false};
    };
    void openFilePreview(const std::string& path);
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
    std::unordered_map<std::string, Preview> previews_{}; // key: window id

    // DnD docking overlay + constraints
    std::optional<std::string> dragging_window_id_{}; // set while user drags our handle
    int min_dock_width_{200};
    int min_dock_height_{120};
};

} // namespace gb2d
