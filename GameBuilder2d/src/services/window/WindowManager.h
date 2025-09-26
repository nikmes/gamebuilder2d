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

// Include logging definitions (LogLine) needed for incremental log console state.
#include "services/logger/LogManager.h"

namespace gb2d {

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

    // New TextEditor-backed log console state (Phase 1 replacement)
    TextEditor log_editor_{};              // Read-only view of filtered log
    bool log_editor_initialized_{false};   // Lazy init flag
    size_t log_last_snapshot_size_{0};     // Last snapshot size used to build editor text
    uint64_t log_last_hash_{0};            // Hash of (snapshot_size, level_mask, filter_text)
    bool log_user_was_at_bottom_{true};    // Track if user was at bottom before rebuild for refined autoscroll
    // Use alias to guard against any macro/name collision on 'logging'
    using LoggingLine = ::gb2d::logging::LogLine;
    std::vector<LoggingLine> log_prev_raw_; // Last raw snapshot for incremental append decision
    std::string log_editor_text_cache_;    // Cached full text currently loaded in log_editor_
    size_t log_prev_emitted_count_{0};      // How many raw lines contributed to current editor text (post-filter)
    size_t log_prev_char_count_{0};          // Cached character count of editor text to detect no-op rebuilds
    size_t log_text_version_{0};             // Monotonic version incremented whenever log_editor_text_cache_ mutates

    // Console search (T2.5) state
    std::string console_search_query_{};              // current search query (independent of filter)
    std::string console_search_last_query_{};         // last query we processed
    size_t console_search_last_version_{(size_t)-1};  // last log_text_version_ we processed
    struct ConsoleSearchMatch { int line; int start_col; int end_col; }; // [start_col,end_col)
    std::vector<ConsoleSearchMatch> console_search_matches_{};
    int console_search_current_index_{0};
    bool console_search_case_sensitive_{false};
    bool console_search_selection_dirty_{false};       // trigger selection update in editor
    bool console_search_last_case_sensitive_{false};    // detect toggles

    // Phase 2 instrumentation (T2.1): performance & behavior counters (debug builds only)
#ifdef GB2D_LOG_CONSOLE_INSTRUMENT
    struct LogConsoleMetrics {
        uint64_t total_full_rebuilds{0};
        uint64_t total_incremental_appends{0};
        uint64_t total_noop_skips{0};          // hash stable early-return
        uint64_t total_settext_calls{0};       // actual SetText invocations
        uint64_t total_frames{0};              // frames rebuild evaluated
        uint64_t total_truncation_fallbacks{0}; // times incremental disabled due to prefix mismatch (ring eviction or history change)
        double   accum_full_rebuild_ms{0.0};
        double   accum_incremental_ms{0.0};
        double   last_op_ms{0.0};
        bool     last_was_incremental{false};
    } log_metrics_{};
#endif

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

    // Log console helpers
    void initLogEditorIfNeeded();
    void rebuildLogEditorIfNeeded();

    // Shutdown state guard
    bool shutting_down_{false};
};

} // namespace gb2d
