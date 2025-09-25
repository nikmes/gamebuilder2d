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
// ImGuiColorTextEdit editor
#include <TextEditor.h>

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
    std::optional<std::string> focus_request_window_id_{}; // focus a window after render
    int min_dock_width_{200};
    int min_dock_height_{120};

    // Console settings (persisted with layout)
    bool console_autoscroll_{true};
    int console_max_lines_{1000};
    size_t console_buffer_cap_{5000};
    uint32_t console_level_mask_{0x3F}; // bits: 0=trace,1=debug,2=info,3=warn,4=err,5=critical
    std::string console_text_filter_{};

    // Text Editor state (multi-tab)
    struct EditorTab {
        std::string path;      // absolute or relative on open
        std::string title;     // tab title (file name or "Untitled")
        std::unique_ptr<TextEditor> editor;
        bool dirty{false};
        std::string langName;  // for display/debug
    };
    struct EditorState {
        bool exists{false};
        bool open{true};
        std::string id;            // managed window id
        int current{-1};
        std::vector<EditorTab> tabs;
    };
    EditorState editor_{};

    // Editor helpers
    void ensureEditorWindow(bool focus = true);
    void renderEditorWindow();
    void openEditorFile(const std::string& path);
    bool saveEditorTab(int index, bool saveAs = false);
    static bool isTextLikeExtension(const std::string& ext);
    static const TextEditor::LanguageDefinition& languageForExtension(const std::string& ext, std::string& outName);
};

} // namespace gb2d
