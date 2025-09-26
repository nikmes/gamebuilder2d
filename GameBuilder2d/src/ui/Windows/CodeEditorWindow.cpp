#include "ui/Windows/CodeEditorWindow.h"
#include "ui/WindowContext.h"
#include <nlohmann/json.hpp>
#include <imgui.h>
#include "ImGuiFileDialog.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
    inline std::string toLower(std::string s){ for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
}

namespace gb2d {

bool CodeEditorWindow::isTextLikeExtension(const std::string& ext) {
    static const char* exts[] = { 
        ".txt", ".md", ".log", ".cmake", ".ini", ".json", ".yaml", ".yml",
        ".h", ".hpp", ".c", ".cpp", ".cc", ".cxx", ".glsl", ".vert", ".frag", ".hlsl", ".lua", ".sql"
    };
    for (auto* e : exts) if (ext == e) return true; return false;
}

const TextEditor::LanguageDefinition& CodeEditorWindow::languageForExtension(const std::string& ext, std::string& outName) {
    std::string e = toLower(ext);
    if (e == ".h" || e == ".hpp" || e == ".c" || e == ".cpp" || e == ".cc" || e == ".cxx") { outName = "C/C++"; return TextEditor::LanguageDefinition::CPlusPlus(); }
    if (e == ".glsl" || e == ".vert" || e == ".frag") { outName = "GLSL"; return TextEditor::LanguageDefinition::GLSL(); }
    if (e == ".hlsl") { outName = "HLSL"; return TextEditor::LanguageDefinition::HLSL(); }
    if (e == ".c") { outName = "C"; return TextEditor::LanguageDefinition::C(); }
    if (e == ".sql") { outName = "SQL"; return TextEditor::LanguageDefinition::SQL(); }
    if (e == ".lua") { outName = "Lua"; return TextEditor::LanguageDefinition::Lua(); }
    outName = "Plain"; return TextEditor::LanguageDefinition::CPlusPlus();
}

void CodeEditorWindow::newUntitled() {
    Tab tab; tab.title = "Untitled"; tab.editor = std::make_unique<TextEditor>();
    tab.editor->SetPalette(TextEditor::GetDarkPalette()); tab.editor->SetShowWhitespaces(false);
    tab.editor->SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
    tab.editor->SetText(""); tab.dirty = false;
    tabs_.push_back(std::move(tab)); current_ = (int)tabs_.size() - 1;
}

void CodeEditorWindow::openFile(const std::string& path) {
    // Focus existing
    for (int i = 0; i < (int)tabs_.size(); ++i) { if (tabs_[i].path == path) { current_ = i; return; } }
    Tab tab;
    tab.path = path;
    tab.title = std::filesystem::path(path).filename().string();
    tab.editor = std::make_unique<TextEditor>();
    tab.editor->SetShowWhitespaces(false);
    tab.editor->SetPalette(TextEditor::GetDarkPalette());
    std::string ext; try { ext = std::filesystem::path(path).extension().string(); } catch (...) { ext.clear(); }
    tab.editor->SetLanguageDefinition(languageForExtension(ext, tab.langName));
    try {
        std::ifstream ifs(path, std::ios::binary);
        std::ostringstream oss; oss << ifs.rdbuf();
        tab.editor->SetText(oss.str());
        tab.dirty = false;
    } catch (...) {
        tab.editor->SetText("");
        tab.dirty = false;
    }
    tabs_.push_back(std::move(tab));
    current_ = (int)tabs_.size() - 1;
}

bool CodeEditorWindow::saveCurrent(bool saveAs) {
    if (current_ < 0 || current_ >= (int)tabs_.size()) return false;
    auto& t = tabs_[current_];
    std::string savePath = t.path;
    if (saveAs || savePath.empty()) {
        IGFD::FileDialogConfig cfg; // optional: could set path from context's recent folder service
        ImGuiFileDialog::Instance()->OpenDialog("EditorSaveAsDlg_Modular", "Save File As", ".*", cfg);
        pending_save_as_index_ = current_;
        return false; // actual save will happen when dialog result is processed in render
    }
    try {
        std::ofstream ofs(savePath, std::ios::binary | std::ios::trunc);
        auto content = t.editor->GetText();
        ofs.write(content.data(), (std::streamsize)content.size());
        t.dirty = false;
        return true;
    } catch (...) {
        return false;
    }
}

void CodeEditorWindow::render(WindowContext& /*ctx*/) {
    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) newUntitled();
            if (ImGui::MenuItem("Open...")) {
                IGFD::FileDialogConfig cfg; // path could be provided by context later
                ImGuiFileDialog::Instance()->OpenDialog("EditorOpenDlg_Modular", "Open File", ".*", cfg);
            }
            bool canSave = current_ >= 0 && current_ < (int)tabs_.size();
            if (!canSave) ImGui::BeginDisabled();
            if (ImGui::MenuItem("Save")) { saveCurrent(false); }
            if (ImGui::MenuItem("Save As...")) { saveCurrent(true); }
            if (ImGui::MenuItem("Save All")) { saveAll(); }
            if (!canSave) ImGui::EndDisabled();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Theme")) {
            bool canApply = current_ >= 0 && current_ < (int)tabs_.size();
            if (!canApply) ImGui::BeginDisabled();
            if (ImGui::MenuItem("Dark") && canApply) tabs_[current_].editor->SetPalette(TextEditor::GetDarkPalette());
            if (ImGui::MenuItem("Light") && canApply) tabs_[current_].editor->SetPalette(TextEditor::GetLightPalette());
            if (ImGui::MenuItem("Retro Blue") && canApply) tabs_[current_].editor->SetPalette(TextEditor::GetRetroBluePalette());
            if (!canApply) ImGui::EndDisabled();
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Toolbar above tabs
    {
        if (ImGui::Button("New")) newUntitled();
        ImGui::SameLine();
        if (ImGui::Button("Open")) {
            IGFD::FileDialogConfig cfg; 
            ImGuiFileDialog::Instance()->OpenDialog("EditorOpenDlg_Modular", "Open File", ".*", cfg);
        }
        ImGui::SameLine();
        bool canSave = current_ >= 0 && current_ < (int)tabs_.size();
        if (!canSave) ImGui::BeginDisabled();
        if (ImGui::Button("Save")) { saveCurrent(false); }
        ImGui::SameLine();
        if (ImGui::Button("Save All")) { saveAll(); }
        ImGui::SameLine();
        if (ImGui::Button("Close Tab")) { closeCurrent(); }
        ImGui::SameLine();
        if (ImGui::Button("Close All")) { closeAll(); }
        if (!canSave) ImGui::EndDisabled();
        ImGui::Separator();
    }

    // Tab bar
    if (ImGui::BeginTabBar("EditorTabs_Modular", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
        for (int i = 0; i < (int)tabs_.size(); ++i) {
            auto& t = tabs_[i];
            std::string label = t.title; if (t.dirty) label += " *";
            if (ImGui::BeginTabItem(label.c_str(), nullptr)) {
                current_ = i;
                if (t.editor->IsTextChanged()) t.dirty = true;
                ImGui::TextUnformatted(t.path.empty() ? "(unsaved)" : t.path.c_str());
                if (!t.langName.empty()) { ImGui::SameLine(); ImGui::TextDisabled("[%s]", t.langName.c_str()); }
                t.editor->Render("##text");
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    // Open dialog results
    if (ImGuiFileDialog::Instance()->Display("EditorOpenDlg_Modular")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            auto path = ImGuiFileDialog::Instance()->GetFilePathName();
            openFile(path);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // Save As dialog results
    if (ImGuiFileDialog::Instance()->Display("EditorSaveAsDlg_Modular")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            auto savePath = ImGuiFileDialog::Instance()->GetFilePathName();
            int idx = current_;
            if (pending_save_as_index_.has_value() && *pending_save_as_index_ >= 0 && *pending_save_as_index_ < (int)tabs_.size()) idx = *pending_save_as_index_;
            if (idx >= 0 && idx < (int)tabs_.size()) {
                auto& t = tabs_[idx];
                try {
                    std::ofstream ofs(savePath, std::ios::binary | std::ios::trunc);
                    auto content = t.editor->GetText();
                    ofs.write(content.data(), (std::streamsize)content.size());
                    t.dirty = false;
                    t.path = savePath;
                    t.title = std::filesystem::path(savePath).filename().string();
                    std::string ext; try { ext = std::filesystem::path(savePath).extension().string(); } catch (...) { ext.clear(); }
                    t.editor->SetLanguageDefinition(languageForExtension(ext, t.langName));
                } catch (...) {
                    // swallow for now
                }
            }
        }
        pending_save_as_index_.reset();
        ImGuiFileDialog::Instance()->Close();
    }
}

bool CodeEditorWindow::saveAll() {
    bool anySaved = false;
    for (int i = 0; i < (int)tabs_.size(); ++i) {
        auto& t = tabs_[i];
        if (!t.dirty) continue;
        if (t.path.empty()) {
            // open Save As for this index (non-blocking), first one at a time
            if (!pending_save_as_index_.has_value()) {
                pending_save_as_index_ = i;
                IGFD::FileDialogConfig cfg; 
                ImGuiFileDialog::Instance()->OpenDialog("EditorSaveAsDlg_Modular", "Save File As", ".*", cfg);
            }
        } else {
            try {
                std::ofstream ofs(t.path, std::ios::binary | std::ios::trunc);
                auto content = t.editor->GetText();
                ofs.write(content.data(), (std::streamsize)content.size());
                t.dirty = false;
                anySaved = true;
            } catch (...) {
                // ignore error for now
            }
        }
    }
    return anySaved;
}

void CodeEditorWindow::closeCurrent() {
    if (current_ < 0 || current_ >= (int)tabs_.size()) return;
    tabs_.erase(tabs_.begin() + current_);
    if (tabs_.empty()) current_ = -1; else if (current_ >= (int)tabs_.size()) current_ = (int)tabs_.size() - 1;
}

void CodeEditorWindow::closeAll() {
    tabs_.clear();
    current_ = -1;
}

void CodeEditorWindow::serialize(nlohmann::json& out) const {
    out["title"] = title_;
    // minimal: save open files and current index
    out["current"] = current_;
    nlohmann::json jt = nlohmann::json::array();
    for (const auto& t : tabs_) {
        nlohmann::json jtab;
        jtab["path"] = t.path;
        jtab["title"] = t.title;
        jtab["dirty"] = t.dirty;
        jtab["lang"] = t.langName;
        jt.push_back(std::move(jtab));
    }
    out["tabs"] = std::move(jt);
}

void CodeEditorWindow::deserialize(const nlohmann::json& in) {
    if (auto it = in.find("title"); it != in.end() && it->is_string()) title_ = *it;
    if (auto it = in.find("tabs"); it != in.end() && it->is_array()) {
        tabs_.clear();
        for (const auto& jtab : *it) {
            Tab t;
            if (auto p = jtab.find("path"); p != jtab.end() && p->is_string()) t.path = *p;
            if (auto tt = jtab.find("title"); tt != jtab.end() && tt->is_string()) t.title = *tt; else t.title = t.path.empty() ? std::string("Untitled") : std::filesystem::path(t.path).filename().string();
            if (auto d = jtab.find("dirty"); d != jtab.end() && d->is_boolean()) t.dirty = *d; else t.dirty = false;
            if (auto ln = jtab.find("lang"); ln != jtab.end() && ln->is_string()) t.langName = *ln; else t.langName.clear();
            t.editor = std::make_unique<TextEditor>();
            t.editor->SetShowWhitespaces(false);
            t.editor->SetPalette(TextEditor::GetDarkPalette());
            if (!t.path.empty()) {
                try {
                    std::ifstream ifs(t.path, std::ios::binary);
                    std::ostringstream oss; oss << ifs.rdbuf();
                    t.editor->SetText(oss.str());
                } catch (...) { t.editor->SetText(""); }
                std::string ext; try { ext = std::filesystem::path(t.path).extension().string(); } catch (...) { ext.clear(); }
                t.editor->SetLanguageDefinition(languageForExtension(ext, t.langName));
            } else {
                t.editor->SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
                t.editor->SetText("");
            }
            tabs_.push_back(std::move(t));
        }
    }
    if (auto it = in.find("current"); it != in.end() && it->is_number_integer()) current_ = (int)*it; else current_ = tabs_.empty() ? -1 : 0;
}

} // namespace gb2d
