#include "ui/Windows/ConsoleLogWindow.h"
#include "ui/WindowContext.h"
// logging snapshot API
#include "services/logger/LogManager.h"
// imgui & editor
#include <nlohmann/json.hpp>
#include <imgui.h>
#include <imgui_internal.h>
// ImGuiColorTextEdit
//#include <TextEditor.h> // already included by header
// std helpers
#include <cctype>
#include <algorithm>

namespace {
    constexpr float kConsoleFontScaleMin = 0.7f;
    constexpr float kConsoleFontScaleMax = 2.5f;
    constexpr float kConsoleTextBrightnessMin = 0.6f;
    constexpr float kConsoleTextBrightnessMax = 1.8f;
    // Simple 64-bit FNV-1a hash utility for rebuild change detection
    uint64_t fnv1a64(const void* data, size_t len, uint64_t seed = 1469598103934665603ull) {
        uint64_t h = seed;
        const unsigned char* p = (const unsigned char*)data;
        for (size_t i = 0; i < len; ++i) {
            h ^= (uint64_t)p[i];
            h *= 1099511628211ull;
        }
        return h;
    }

    ImU32 scaleColor(ImU32 color, float factor) {
        ImVec4 c = ImGui::ColorConvertU32ToFloat4(color);
        factor = std::clamp(factor, 0.1f, 2.5f);
        c.x = std::clamp(c.x * factor, 0.0f, 1.0f);
        c.y = std::clamp(c.y * factor, 0.0f, 1.0f);
        c.z = std::clamp(c.z * factor, 0.0f, 1.0f);
        return ImGui::ColorConvertFloat4ToU32(c);
    }

    // Custom tokenizer to color only the log level word (inside brackets) with a unique color per level.
    bool LogTokenize(const char* in_begin, const char* in_end,
                     const char*& out_begin, const char*& out_end,
                     TextEditor::PaletteIndex& paletteIndex) {
        // Skip whitespace first
        const char* p = in_begin;
        while (p < in_end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) {
            out_begin = p;
            out_end = p + 1;
            paletteIndex = TextEditor::PaletteIndex::Default;
            return true;
        }
        if (p >= in_end) return false;

        if (*p == '[') {
            const char* token_start = p;
            ++p;
            const char* word_start = p;
            while (p < in_end && *p != ']' && *p != '\n' && *p != '\r') ++p;
            const char* word_end = p;
            if (p < in_end && *p == ']') {
                std::string inner(word_start, word_end);
                std::string lower;
                lower.reserve(inner.size());
                for (char c : inner) lower.push_back((char)std::tolower((unsigned char)c));

                if (lower == "trace")      paletteIndex = TextEditor::PaletteIndex::Comment;
                else if (lower == "debug") paletteIndex = TextEditor::PaletteIndex::Identifier;
                else if (lower == "info")  paletteIndex = TextEditor::PaletteIndex::KnownIdentifier;
                else if (lower == "warn" || lower == "warning")  paletteIndex = TextEditor::PaletteIndex::PreprocIdentifier;
                else if (lower == "error" || lower == "err") paletteIndex = TextEditor::PaletteIndex::Keyword;
                else if (lower == "crit" || lower == "critical") paletteIndex = TextEditor::PaletteIndex::Preprocessor;
                else paletteIndex = TextEditor::PaletteIndex::Punctuation;

                ++p; // consume closing bracket
                out_begin = token_start;
                out_end = p;
                return true;
            }
            // unmatched '[', revert pointer to treat as punctuation below
            p = token_start;
        }

        if (std::ispunct((unsigned char)*p) && *p != '_' && *p != '-') {
            out_begin = p;
            out_end = p + 1;
            paletteIndex = TextEditor::PaletteIndex::Punctuation;
            return true;
        }

        if (std::isalnum((unsigned char)*p)) {
            const char* start = p;
            ++p;
            while (p < in_end && (std::isalnum((unsigned char)*p) || *p == '_' || *p=='-')) ++p;
            std::string word(start, p);
            std::string lower;
            lower.reserve(word.size());
            for (char c : word) lower.push_back((char)std::tolower((unsigned char)c));
            paletteIndex = TextEditor::PaletteIndex::Default;
            out_begin = start;
            out_end = p;
            return true;
        }

        out_begin = p;
        out_end = p + 1;
        paletteIndex = TextEditor::PaletteIndex::Default;
        return true;
    }

    TextEditor::LanguageDefinition CreateLogLanguageDefinition() {
        static bool initialized = false;
        static TextEditor::LanguageDefinition lang;
        if (!initialized) {
            lang.mName = "GB2DLog";
            lang.mKeywords.clear();
            lang.mTokenRegexStrings.clear();
            lang.mCommentStart = "";
            lang.mCommentEnd = "";
            lang.mSingleLineComment = "";
            lang.mCaseSensitive = true;
            lang.mIdentifiers.clear();
            lang.mPreprocIdentifiers.clear();
            lang.mAutoIndentation = false;
            lang.mTokenize = &LogTokenize;
            initialized = true;
        }
        return lang;
    }
}

namespace gb2d {

void ConsoleLogWindow::initEditorIfNeeded() {
    if (editor_initialized_) return;
    editor_initialized_ = true;
    // Apply default buffer capacity on first use
    gb2d::logging::set_log_buffer_capacity(buffer_cap_);
    editor_.SetReadOnly(true);
    editor_.SetShowWhitespaces(false);
    editor_.SetImGuiChildIgnored(true);
    applyPalette();
    editor_.SetLanguageDefinition(CreateLogLanguageDefinition());
}

void ConsoleLogWindow::applyPalette() {
    float tone = std::clamp(text_brightness_, kConsoleTextBrightnessMin, kConsoleTextBrightnessMax);
    text_brightness_ = tone;

    auto palette = TextEditor::GetDarkPalette();
    auto toneColor = [&](ImU32 base, float multiplier = 1.0f) {
        return scaleColor(base, tone * multiplier);
    };

    palette[(int)TextEditor::PaletteIndex::Default] = toneColor(IM_COL32(220, 220, 220, 255));
    palette[(int)TextEditor::PaletteIndex::Number] = palette[(int)TextEditor::PaletteIndex::Default];
    palette[(int)TextEditor::PaletteIndex::String] = palette[(int)TextEditor::PaletteIndex::Default];
    palette[(int)TextEditor::PaletteIndex::CharLiteral] = palette[(int)TextEditor::PaletteIndex::Default];

    palette[(int)TextEditor::PaletteIndex::Identifier] = IM_COL32(110, 190, 255, 255);       // DEBUG (sky blue)
    palette[(int)TextEditor::PaletteIndex::KnownIdentifier] = IM_COL32(120, 230, 150, 255);  // INFO (bright green)
    palette[(int)TextEditor::PaletteIndex::PreprocIdentifier] = IM_COL32(255, 200, 80, 255); // WARN (amber)
    palette[(int)TextEditor::PaletteIndex::Keyword] = IM_COL32(255, 110, 110, 255);          // ERROR (red)
    palette[(int)TextEditor::PaletteIndex::Preprocessor] = IM_COL32(230, 120, 255, 255);     // CRIT (magenta)

    palette[(int)TextEditor::PaletteIndex::Comment] = toneColor(IM_COL32(160, 160, 160, 255), 0.85f);
    palette[(int)TextEditor::PaletteIndex::MultiLineComment] = palette[(int)TextEditor::PaletteIndex::Comment];
    palette[(int)TextEditor::PaletteIndex::Punctuation] = toneColor(IM_COL32(190, 190, 190, 255));

    palette[(int)TextEditor::PaletteIndex::Background] = IM_COL32(26, 26, 28, 255);
    palette[(int)TextEditor::PaletteIndex::LineNumber] = toneColor(IM_COL32(130, 130, 130, 255), 0.9f);
    palette[(int)TextEditor::PaletteIndex::Cursor] = toneColor(IM_COL32(255, 255, 255, 255), 1.1f);
    palette[(int)TextEditor::PaletteIndex::Selection] = toneColor(IM_COL32(80, 120, 180, 160), 0.95f);
    palette[(int)TextEditor::PaletteIndex::CurrentLineFill] = IM_COL32(50, 50, 50, 60);
    palette[(int)TextEditor::PaletteIndex::CurrentLineFillInactive] = IM_COL32(40, 40, 40, 40);
    palette[(int)TextEditor::PaletteIndex::CurrentLineEdge] = IM_COL32(60, 60, 60, 120);

    editor_.SetPalette(palette);
}

void ConsoleLogWindow::rebuildEditorIfNeeded() {
    last_autoscroll_triggered_ = false;
    // Snapshot current log lines (bounded by max_lines_)
    auto lines = gb2d::logging::read_log_lines_snapshot((size_t)max_lines_);
    size_t snapshotSize = lines.size();

    // Compute hash of inputs
    uint64_t h = 1469598103934665603ull;
    h = fnv1a64(&snapshotSize, sizeof(snapshotSize), h);
    h = fnv1a64(&level_mask_, sizeof(level_mask_), h);
    h = fnv1a64(text_filter_.data(), text_filter_.size(), h);

    if (snapshotSize == last_snapshot_size_ && h == last_hash_) {
        return; // nothing changed that affects filtered view
    }

    // Determine if user is at bottom
    bool should_autoscroll = false;
    if (autoscroll_) {
        auto totalBefore = editor_.GetTotalLines();
        auto cursor = editor_.GetCursorPosition();
        if (totalBefore == 0) {
            user_was_at_bottom_ = true;
        } else {
            user_was_at_bottom_ = (cursor.mLine >= totalBefore - 2);
        }
        should_autoscroll = user_was_at_bottom_;
    }

    bool filters_simple = text_filter_.empty();
    bool size_non_decreasing = snapshotSize >= prev_raw_.size();
    bool can_incremental = size_non_decreasing && filters_simple && (level_mask_ == 0x3F);

    if (can_incremental && !prev_raw_.empty()) {
        size_t prevCount = prev_raw_.size();
        bool prefix_ok = true;
        if (prevCount <= lines.size()) {
            for (size_t i = 0; i < prevCount; ++i) {
                const auto& a = prev_raw_[i];
                const auto& b = lines[i];
                if (a.level != b.level || a.text != b.text) { prefix_ok = false; break; }
            }
        } else prefix_ok = false;
        if (!prefix_ok) {
            can_incremental = false;
        }
    }

    bool did_incremental = false;
    bool text_changed = false;
    std::string out;
    if (!can_incremental) out.reserve(snapshotSize * 64);

    size_t start_index = 0;
    if (can_incremental) start_index = prev_raw_.size();
    std::string appendBuf;
    if (can_incremental) appendBuf.reserve((snapshotSize - start_index) * 64);

    auto processLine = [&](const gb2d::logging::LogLine& ln, std::string& dest){
        uint32_t bit = 0;
        switch (ln.level) {
            case gb2d::logging::Level::trace: bit = 1u<<0; break;
            case gb2d::logging::Level::debug: bit = 1u<<1; break;
            case gb2d::logging::Level::info:  bit = 1u<<2; break;
            case gb2d::logging::Level::warn:  bit = 1u<<3; break;
            case gb2d::logging::Level::err:   bit = 1u<<4; break;
            case gb2d::logging::Level::critical: bit = 1u<<5; break;
            case gb2d::logging::Level::off: default: break;
        }
        if ((level_mask_ & bit) == 0) return false;
        if (!text_filter_.empty()) {
            std::string hay = ln.text;
            std::string needle = text_filter_;
            std::transform(hay.begin(), hay.end(), hay.begin(), [](unsigned char c){ return (char)tolower(c); });
            std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c){ return (char)tolower(c); });
            if (hay.find(needle) == std::string::npos) return false;
        }
        dest.append(ln.text);
        if (!dest.empty() && dest.back() != '\n') dest.push_back('\n');
        return true;
    };

    size_t emitted_count = 0;
    if (can_incremental && start_index < snapshotSize) {
        for (size_t i = start_index; i < snapshotSize; ++i) {
            if (processLine(lines[i], appendBuf)) ++emitted_count;
        }
        if (!appendBuf.empty()) {
            editor_text_cache_.append(appendBuf);
            editor_.SetText(editor_text_cache_);
            did_incremental = true;
            text_changed = true;
        } else {
            did_incremental = true;
        }
    }

    if (!did_incremental) {
        for (const auto& ln : lines) {
            if (processLine(ln, out)) ++emitted_count;
        }
        if (out.size() != prev_char_count_ || out != editor_text_cache_) {
            editor_text_cache_.assign(out.begin(), out.end());
            editor_.SetText(editor_text_cache_);
            text_changed = true;
        }
    }
    if (text_changed) {
        ++text_version_;
    }
    if (should_autoscroll && text_changed) {
        auto totalLines = editor_.GetTotalLines();
        if (totalLines > 0) {
            TextEditor::Coordinates c{ (int)totalLines - 1, 0 };
            editor_.SetCursorPosition(c);
        }
        last_autoscroll_triggered_ = true;
    }
    last_snapshot_size_ = snapshotSize;
    last_hash_ = h;
    if (!did_incremental) {
        prev_raw_ = lines;
    } else {
        if (!appendBuf.empty()) {
            prev_raw_.insert(prev_raw_.end(), lines.begin() + (long long)start_index, lines.end());
        } else {
            prev_raw_ = lines;
        }
    }
    if (text_changed) {
        prev_char_count_ = editor_text_cache_.size();
    }
}

void ConsoleLogWindow::render(WindowContext& /*ctx*/) {
    initEditorIfNeeded();
    font_scale_ = std::clamp(font_scale_, kConsoleFontScaleMin, kConsoleFontScaleMax);

    ImGuiContext* imguiCtx = ImGui::GetCurrentContext();
    ImGuiWindow* previousFocusWindow = imguiCtx ? imguiCtx->NavWindow : nullptr;
    const bool windowWasFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    // Settings / controls row
    ImGui::SetNextItemWidth(120);
    ImGui::InputInt("Max lines", &max_lines_);
    if (max_lines_ < 100) max_lines_ = 100;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    static int bufCapTmp2 = 0; bufCapTmp2 = (int)buffer_cap_;
    if (ImGui::InputInt("Buffer cap", &bufCapTmp2)) {
        if (bufCapTmp2 < 1000) bufCapTmp2 = 1000;
        buffer_cap_ = (size_t)bufCapTmp2;
        gb2d::logging::set_log_buffer_capacity(buffer_cap_);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Autoscroll", &autoscroll_);
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        gb2d::logging::clear_log_buffer();
        editor_.SetText("");
        last_snapshot_size_ = 0;
        last_hash_ = 0;
        prev_raw_.clear();
        editor_text_cache_.clear();
        prev_char_count_ = 0;
        ++text_version_;
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy")) {
        auto txt = editor_.GetText();
        ImGui::SetClipboardText(txt.c_str());
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("Font");
    ImGui::SameLine();
    if (ImGui::SmallButton("A-")) {
    font_scale_ = std::max(kConsoleFontScaleMin, font_scale_ - 0.1f);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("A+")) {
    font_scale_ = std::min(kConsoleFontScaleMax, font_scale_ + 0.1f);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::SliderFloat("##console_font_scale", &font_scale_, kConsoleFontScaleMin, kConsoleFontScaleMax, "%.2fx")) {
        font_scale_ = std::clamp(font_scale_, kConsoleFontScaleMin, kConsoleFontScaleMax);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##console_font")) {
        font_scale_ = 1.0f;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("x%.2f", font_scale_);
    ImGui::SameLine();
    ImGui::TextUnformatted("Tone");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::SliderFloat("##console_text_brightness", &text_brightness_, kConsoleTextBrightnessMin, kConsoleTextBrightnessMax, "%.2f")) {
        applyPalette();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##console_text_tone")) {
        text_brightness_ = 1.0f;
        applyPalette();
    }
    ImGui::NewLine();

    auto lvlBtn2 = [&](const char* label, uint32_t bit){
        bool on = (level_mask_ & bit) != 0;
        if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.6f,0.2f,1.0f));
        if (ImGui::SmallButton(label)) {
            level_mask_ ^= bit;
            if ((level_mask_ & 0x3F) == 0) level_mask_ = 0x3F;
        }
        if (on) ImGui::PopStyleColor();
        ImGui::SameLine();
    };
    lvlBtn2("Trace", 1u<<0);
    lvlBtn2("Debug", 1u<<1);
    lvlBtn2("Info",  1u<<2);
    lvlBtn2("Warn",  1u<<3);
    lvlBtn2("Error", 1u<<4);
    lvlBtn2("Crit",  1u<<5);
    ImGui::NewLine();

    ImGui::SetNextItemWidth(300);
    char filterBuf2[256];
    std::strncpy(filterBuf2, text_filter_.c_str(), sizeof(filterBuf2));
    filterBuf2[sizeof(filterBuf2)-1] = '\0';
    if (ImGui::InputText("##filter", filterBuf2, IM_ARRAYSIZE(filterBuf2))) {
        text_filter_ = filterBuf2;
    }

    // Search controls
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    char searchBuf[256];
    std::strncpy(searchBuf, search_query_.c_str(), sizeof(searchBuf));
    searchBuf[sizeof(searchBuf)-1] = '\0';
    bool searchEdited = false;
    
    if (ImGui::InputTextWithHint("##console_search", "Search", searchBuf, IM_ARRAYSIZE(searchBuf))) {
        search_query_ = searchBuf;
        searchEdited = true;
    }
    
    ImGui::SameLine();
    ImGui::Checkbox("Aa", &search_case_sensitive_); ImGui::SameLine();
    bool goPrev = ImGui::ArrowButton("##search_prev", ImGuiDir_Left); ImGui::SameLine();
    bool goNext = ImGui::ArrowButton("##search_next", ImGuiDir_Right); ImGui::SameLine();
    
    if (ImGui::Button("Clear Search")) {
        search_query_.clear();
        search_matches_.clear();
        search_current_index_ = 0;
    }
    if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        goNext = true;
    }

    if (searchEdited || search_last_query_ != search_query_ || search_last_version_ != text_version_ || search_last_case_sensitive_ != search_case_sensitive_) {
        search_matches_.clear();
        search_current_index_ = 0;
        search_last_query_ = search_query_;
        search_last_version_ = text_version_;
        search_last_case_sensitive_ = search_case_sensitive_;
        if (!search_query_.empty()) {
            auto lines = editor_.GetTextLines();
            std::string needle = search_query_;
            if (!search_case_sensitive_) {
                std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c){ return (char)tolower(c); });
            }
            for (int li = 0; li < (int)lines.size(); ++li) {
                const std::string& L = lines[li];
                std::string hay = L;
                if (!search_case_sensitive_) {
                    std::transform(hay.begin(), hay.end(), hay.begin(), [](unsigned char c){ return (char)tolower(c); });
                }
                size_t pos = hay.find(needle);
                while (!needle.empty() && pos != std::string::npos) {
                    search_matches_.push_back({ li, (int)pos, (int)(pos + needle.size()) });
                    pos = hay.find(needle, pos + (needle.size() ? needle.size() : 1));
                }
            }
        }
        search_selection_dirty_ = true;
    }

    if (!search_matches_.empty()) {
        if (goNext) { search_current_index_ = (search_current_index_ + 1) % (int)search_matches_.size(); search_selection_dirty_ = true; }
        if (goPrev) { search_current_index_ = (search_current_index_ - 1 + (int)search_matches_.size()) % (int)search_matches_.size(); search_selection_dirty_ = true; }
        ImGui::SameLine();
        ImGui::TextDisabled("%d/%d", search_current_index_ + 1, (int)search_matches_.size());
    } else if (!search_query_.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("0/0");
    }

    if (search_selection_dirty_ && !search_matches_.empty()) {
        search_selection_dirty_ = false;
        const auto& m = search_matches_[search_current_index_];
        TextEditor::Coordinates start{ m.line, m.start_col };
        TextEditor::Coordinates end{ m.line, m.end_col };
        editor_.SetSelection(start, end, TextEditor::SelectionMode::Normal);
        editor_.SetCursorPosition(end);
    }

    rebuildEditorIfNeeded();
    constexpr ImGuiWindowFlags editorFlags = ImGuiWindowFlags_HorizontalScrollbar |
                                            ImGuiWindowFlags_AlwaysHorizontalScrollbar |
                                            ImGuiWindowFlags_NoMove;
    ImGuiWindow* consoleChildWindow = nullptr;
    if (ImGui::BeginChild("##console_log_editor", ImVec2(0, 0), false, editorFlags)) {
        consoleChildWindow = ImGui::GetCurrentWindow();
        if (font_scale_ != 1.0f) {
            ImGui::SetWindowFontScale(font_scale_);
        }
        editor_.Render("##log_editor");
        if (font_scale_ != 1.0f) {
            ImGui::SetWindowFontScale(1.0f);
        }
    }
    ImGui::EndChild();

    if (last_autoscroll_triggered_ && !windowWasFocused && imguiCtx && consoleChildWindow) {
        ImGuiWindow* currentNav = imguiCtx->NavWindow;
        if (currentNav == consoleChildWindow && previousFocusWindow && previousFocusWindow != consoleChildWindow) {
            ImGui::FocusWindow(previousFocusWindow);
        }
    }
    last_autoscroll_triggered_ = false;
}

void ConsoleLogWindow::serialize(nlohmann::json& out) const {
    out["title"] = title_;
    out["autoscroll"] = autoscroll_;
    out["max_lines"] = max_lines_;
    out["buffer_cap"] = buffer_cap_;
    out["level_mask"] = level_mask_;
    out["text_filter"] = text_filter_;
    out["font_scale"] = font_scale_;
    out["text_brightness"] = text_brightness_;
}

void ConsoleLogWindow::deserialize(const nlohmann::json& in) {
    if (auto it = in.find("title"); it != in.end() && it->is_string()) title_ = *it;
    if (auto it = in.find("autoscroll"); it != in.end() && it->is_boolean()) autoscroll_ = *it;
    if (auto it = in.find("max_lines"); it != in.end() && it->is_number_integer()) max_lines_ = std::max(100, (int)*it);
    if (auto it = in.find("buffer_cap"); it != in.end() && it->is_number_integer()) {
        int v = (int)*it; if (v < 1000) v = 1000; buffer_cap_ = (size_t)v; gb2d::logging::set_log_buffer_capacity(buffer_cap_);
    }
    if (auto it = in.find("level_mask"); it != in.end() && it->is_number_unsigned()) level_mask_ = (uint32_t)*it;
    if (auto it = in.find("text_filter"); it != in.end() && it->is_string()) text_filter_ = *it;
    if (auto it = in.find("font_scale"); it != in.end() && (it->is_number_float() || it->is_number_integer())) {
        font_scale_ = std::clamp((float)it->get<double>(), kConsoleFontScaleMin, kConsoleFontScaleMax);
    }
    if (auto it = in.find("text_brightness"); it != in.end() && (it->is_number_float() || it->is_number_integer())) {
        text_brightness_ = std::clamp((float)it->get<double>(), kConsoleTextBrightnessMin, kConsoleTextBrightnessMax);
        if (editor_initialized_) applyPalette();
    }
}

} // namespace gb2d
