#include "ui/Windows/FilePreviewWindow.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "ui/ImGuiTextureHelpers.h"
#include "services/audio/AudioManager.h"

namespace gb2d {

void FilePreviewWindow::unload() {
    if (kind_ == Kind::Image && !imageTexture_.key.empty()) {
        gb2d::textures::TextureManager::release(imageTexture_.key);
    }
    if (kind_ == Kind::Audio && !audioAsset_.key.empty()) {
        gb2d::audio::AudioManager::stopSound(audioHandle_);
        gb2d::audio::AudioManager::releaseSound(audioAsset_.key);
    }
    imageTexture_ = {};
    imagePlaceholder_ = false;
    imgW_ = imgH_ = 0;
    loaded_ = false;
    text_.clear();
    audioAsset_ = {};
    audioPlaying_ = false;
    audioHandle_ = {};
    audioVolume_ = 1.0f;
    audioPan_ = 0.5f;
    audioPitch_ = 1.0f;
    kind_ = Kind::None;
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
    std::string alias = "file-preview/" + path;
        auto acquired = gb2d::textures::TextureManager::acquire(path, alias);
        if (acquired.texture) {
            imageTexture_ = acquired;
            imgW_ = imageTexture_.texture->width;
            imgH_ = imageTexture_.texture->height;
            imagePlaceholder_ = imageTexture_.placeholder;
            loaded_ = true;
        } else {
            kind_ = Kind::None;
            loaded_ = false;
        }
    } else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
        kind_ = Kind::Audio;
        audioAlias_ = "file-preview/audio/" + path;
        audioAsset_ = gb2d::audio::AudioManager::acquireSound(path, audioAlias_);
        loaded_ = true;
        audioPlaying_ = false;
        audioHandle_ = {};
        if (audioAsset_.key.empty()) {
            kind_ = Kind::None;
        }
    } else {
        kind_ = Kind::Text;
        imagePlaceholder_ = false;
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
    } else if (kind_ == Kind::Image && loaded_ && imageTexture_.texture) {
        float availW = ImGui::GetContentRegionAvail().x;
        float scale = imgW_ > 0 ? availW / static_cast<float>(imgW_) : 1.0f;
        ImTextureID texId = gb2d::ui::makeImTextureId<ImTextureID>(imageTexture_.texture->id);
        ImGui::Image(texId, ImVec2(imgW_ * scale, imgH_ * scale));
        if (imagePlaceholder_ && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone)) {
            ImGui::SetTooltip("Placeholder texture (failed to load original asset)");
        }
    } else if (kind_ == Kind::Audio && loaded_ && !audioAsset_.key.empty()) {
        using gb2d::audio::AudioManager;
        if (audioPlaying_ && !AudioManager::isHandleActive(audioHandle_)) {
            audioPlaying_ = false;
            audioHandle_ = {};
        }
        ImGui::Separator();
        ImGui::TextUnformatted("Audio preview");
    ImGui::Text("Alias: %s", audioAlias_.c_str());
        ImGui::Text("Placeholder: %s", audioAsset_.placeholder ? "yes" : "no");

        bool volumeChanged = ImGui::SliderFloat("Volume", &audioVolume_, 0.0f, 1.0f, "%.2f");
        bool panChanged = ImGui::SliderFloat("Pan", &audioPan_, 0.0f, 1.0f, "%.2f");
        bool pitchChanged = ImGui::SliderFloat("Pitch", &audioPitch_, 0.5f, 2.0f, "%.2f");

        if (audioPlaying_ && (volumeChanged || panChanged || pitchChanged)) {
            gb2d::audio::PlaybackParams params;
            params.volume = audioVolume_;
            params.pan = audioPan_;
            params.pitch = audioPitch_;
            if (!AudioManager::updateSoundPlayback(audioHandle_, params)) {
                audioPlaying_ = false;
                audioHandle_ = {};
            }
        }

        auto stopPlayback = [&]() {
            if (audioPlaying_) {
                AudioManager::stopSound(audioHandle_);
                audioHandle_ = {};
                audioPlaying_ = false;
            }
        };

        if (ImGui::Button(audioPlaying_ ? "Stop" : "Play")) {
            if (audioPlaying_) {
                stopPlayback();
            } else {
                gb2d::audio::PlaybackParams params;
                params.volume = audioVolume_;
                params.pan = audioPan_;
                params.pitch = audioPitch_;
                audioHandle_ = AudioManager::playSound(audioAsset_.key, params);
                audioPlaying_ = audioHandle_.valid();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reload")) {
            stopPlayback();
            if (!audioAsset_.key.empty()) {
                AudioManager::releaseSound(audioAsset_.key);
            }
            audioAsset_ = AudioManager::acquireSound(path_, audioAlias_);
        }
        if (audioPlaying_) {
            ImGui::SameLine();
            ImGui::TextDisabled("Playing");
        }

        if (ImGui::Button("Stop All")) {
            AudioManager::stopAllSounds();
            audioPlaying_ = false;
            audioHandle_ = {};
        }
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
