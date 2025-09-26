#include "ui/Windows/FilePreviewWindow.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace gb2d {

void FilePreviewWindow::unload() {
    if (kind_ == Kind::Image && loaded_ && texId_ != 0) {
        Texture2D tex; tex.id = texId_; tex.width = imgW_; tex.height = imgH_; tex.mipmaps = 1; tex.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        UnloadTexture(tex);
        texId_ = 0; loaded_ = false; imgW_ = imgH_ = 0;
    }
}

void FilePreviewWindow::open(const std::string& path) {
    unload();
    path_ = path;
    title_ = std::string("Preview: ") + std::filesystem::path(path).filename().string();
    // detect type by extension
    std::string ext;
    try { ext = std::filesystem::path(path).extension().string(); } catch (...) { ext.clear(); }
    for (auto& c : ext) c = (char)tolower((unsigned char)c);
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif") {
        kind_ = Kind::Image;
        Image img = LoadImage(path.c_str());
        if (img.data) {
            Texture2D tex = LoadTextureFromImage(img);
            texId_ = tex.id; imgW_ = tex.width; imgH_ = tex.height; loaded_ = true;
            UnloadImage(img);
        } else {
            kind_ = Kind::None; loaded_ = false;
        }
    } else {
        kind_ = Kind::Text;
        try { std::ifstream ifs(path); std::ostringstream oss; oss << ifs.rdbuf(); text_ = oss.str(); loaded_ = true; }
        catch (...) { text_.clear(); loaded_ = true; }
    }
}

void FilePreviewWindow::render(WindowContext& /*ctx*/) {
    if (path_.empty() && !loaded_) {
        ImGui::TextUnformatted("(no file)");
        return;
    }
    ImGui::TextUnformatted(path_.c_str());
    if (kind_ == Kind::Text) {
        ImGui::Separator();
        ImGui::BeginChild("text", ImVec2(0,0), false, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::TextUnformatted(text_.c_str());
        ImGui::EndChild();
    } else if (kind_ == Kind::Image && loaded_ && texId_ != 0) {
        float availW = ImGui::GetContentRegionAvail().x;
        float scale = (float) imgW_ > 0 ? availW / (float)imgW_ : 1.0f;
        ImGui::Image((ImTextureID)(intptr_t)texId_, ImVec2(imgW_ * scale, imgH_ * scale));
    } else {
        ImGui::TextUnformatted("(no preview)");
    }
}

void FilePreviewWindow::serialize(nlohmann::json& out) const {
    out["title"] = title_;
    out["path"] = path_;
}

void FilePreviewWindow::deserialize(const nlohmann::json& in) {
    if (auto it = in.find("title"); it != in.end() && it->is_string()) title_ = *it;
    if (auto it = in.find("path"); it != in.end() && it->is_string()) {
        open(*it);
    }
}

} // namespace gb2d
