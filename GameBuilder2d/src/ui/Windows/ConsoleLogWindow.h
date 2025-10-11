#pragma once
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include "ui/Window.h"
#include <TextEditor.h>
// For logging::LogLine
#include "services/logger/LogManager.h"

namespace gb2d {

class ConsoleLogWindow : public IWindow {
public:
    ConsoleLogWindow() = default;

    const char* typeId() const override { return "console-log"; }
    const char* displayName() const override { return "Console Log"; }

    std::string title() const override { return title_; }
    void setTitle(std::string t) override { title_ = std::move(t); }

    void render(WindowContext& ctx) override;
    void serialize(nlohmann::json& out) const override;
    void deserialize(const nlohmann::json& in) override;

private:
    std::string title_ = "Console";

    // Settings (persisted by manager layout for now; we also serialize here for future JSON)
    bool autoscroll_{true};
    int max_lines_{1000};
    size_t buffer_cap_{5000};
    uint32_t level_mask_{0x3F};
    std::string text_filter_{};
    float font_scale_{1.0f};
    float text_brightness_{1.0f};

    // TextEditor-backed console state
    TextEditor editor_{};
    bool editor_initialized_{false};
    size_t last_snapshot_size_{0};
    uint64_t last_hash_{0};
    bool user_was_at_bottom_{true};
    std::vector<::gb2d::logging::LogLine> prev_raw_{};
    std::string editor_text_cache_{};
    size_t prev_emitted_count_{0};
    size_t prev_char_count_{0};
    size_t text_version_{0};

    // Search state
    std::string search_query_{};
    std::string search_last_query_{};
    size_t search_last_version_{(size_t)-1};
    struct SearchMatch { int line; int start_col; int end_col; };
    std::vector<SearchMatch> search_matches_{};
    int search_current_index_{0};
    bool search_case_sensitive_{false};
    bool search_selection_dirty_{false};
    bool search_last_case_sensitive_{false};
    bool last_autoscroll_triggered_{false};

    // Helpers
    void initEditorIfNeeded();
    void rebuildEditorIfNeeded();
    void applyPalette();
};

} // namespace gb2d
