#include "ui/Windows/FilePreviewWindow.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <algorithm>
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
    if (kind_ == Kind::Atlas && !atlasKey_.empty()) {
        gb2d::textures::TextureManager::releaseAtlas(atlasKey_);
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
    atlasKey_.clear();
    atlasPlaceholder_ = false;
    atlasFrameCount_ = 0;
    atlasZoom_ = 1.0f;
}

void FilePreviewWindow::open(const std::string& path) {
    unload();
    path_ = path;
    title_ = std::string("Preview: ") + std::filesystem::path(path).filename().string();
    // detect type by extension
    std::string ext;
    try {
        ext = std::filesystem::path(path).extension().string();
    } catch (...) {
        ext.clear();
    }
    for (auto& c : ext) {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }

    auto loadTextFile = [&]() {
        kind_ = Kind::Text;
        imagePlaceholder_ = false;
        try {
            std::ifstream ifs(path);
            std::ostringstream oss;
            oss << ifs.rdbuf();
            text_ = oss.str();
            loaded_ = true;
        } catch (...) {
            text_.clear();
            loaded_ = true;
        }
    };

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
    } else if (ext == ".json") {
        std::string alias = "file-preview/atlas/" + path;
        auto handle = gb2d::textures::TextureManager::acquireAtlas(path, alias);
    if (!handle.key.empty() && handle.texture && (!handle.frames.empty() || !handle.placeholder)) {
            kind_ = Kind::Atlas;
            atlasKey_ = handle.key;
            atlasPlaceholder_ = handle.placeholder;
            atlasFrameCount_ = handle.frames.size();
            atlasZoom_ = 1.0f;
            loaded_ = true;
            if (handle.texture) {
                imgW_ = handle.texture->width;
                imgH_ = handle.texture->height;
            } else {
                imgW_ = imgH_ = 0;
            }
        } else {
            if (!handle.key.empty()) {
                gb2d::textures::TextureManager::releaseAtlas(handle.key);
            }
            loadTextFile();
        }
    } else {
        loadTextFile();
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
    } else if (kind_ == Kind::Atlas && loaded_) {
        ImGui::Separator();
        const auto* atlas = gb2d::textures::TextureManager::tryGetAtlas(atlasKey_);
        if (!atlas || !atlas->texture) {
            ImGui::TextUnformatted("(atlas texture unavailable)");
            if (atlasPlaceholder_) {
                ImGui::TextDisabled("Placeholder atlas (metadata or texture missing)");
            }
            return;
        }

    const std::size_t frameCount = atlas->frames.size();
    atlasFrameCount_ = frameCount;
    ImGui::Text("Frames: %zu", frameCount);
    if (frameCount == 0) {
            ImGui::TextDisabled("No frames defined in atlas metadata");
        }
        ImGui::Text("Size: %d x %d", atlas->texture->width, atlas->texture->height);
        if (atlasPlaceholder_) {
            ImGui::TextDisabled("Texture placeholder in use");
        }

        atlasZoom_ = std::clamp(atlasZoom_, 0.1f, 8.0f);
        ImGui::SliderFloat("Zoom", &atlasZoom_, 0.1f, 8.0f, "%.2fx");
        ImGui::SameLine();
        if (ImGui::Button("Fit Width")) {
            float avail = ImGui::GetContentRegionAvail().x;
            if (atlas->texture->width > 0 && avail > 0.0f) {
                atlasZoom_ = std::clamp(avail / static_cast<float>(atlas->texture->width), 0.1f, 8.0f);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            atlasZoom_ = 1.0f;
        }

        ImVec2 childSize = ImVec2(0, 0);
    ImGui::BeginChild("atlas_canvas", childSize, true, ImGuiWindowFlags_HorizontalScrollbar);
        ImTextureID texId = gb2d::ui::makeImTextureId<ImTextureID>(atlas->texture->id);
        ImVec2 previewSize(static_cast<float>(atlas->texture->width) * atlasZoom_,
                           static_cast<float>(atlas->texture->height) * atlasZoom_);
        ImGui::Image(texId, previewSize);

        const ImVec2 imageMin = ImGui::GetItemRectMin();
        const gb2d::textures::AtlasFrame* hoveredFrame = nullptr;
        auto* drawList = ImGui::GetWindowDrawList();
        for (const auto& frame : atlas->frames) {
            ImVec2 topLeft(imageMin.x + frame.frame.x * atlasZoom_,
                           imageMin.y + frame.frame.y * atlasZoom_);
            ImVec2 bottomRight(imageMin.x + (frame.frame.x + frame.frame.width) * atlasZoom_,
                               imageMin.y + (frame.frame.y + frame.frame.height) * atlasZoom_);
            bool hovered = ImGui::IsMouseHoveringRect(topLeft, bottomRight);
            ImU32 color = hovered ? IM_COL32(255, 128, 0, 255) : IM_COL32(0, 200, 255, 180);
            float thickness = hovered ? 2.0f : 1.0f;
            drawList->AddRect(topLeft, bottomRight, color, 0.0f, 0, thickness);
            if (hovered) {
                hoveredFrame = &frame;
            }
        }
        if (hoveredFrame) {
            const auto& rect = hoveredFrame->frame;
            ImGui::SetTooltip("%s\nPos: (%.0f, %.0f) Size: (%.0f, %.0f)",
                              hoveredFrame->originalName.c_str(),
                              rect.x,
                              rect.y,
                              rect.width,
                              rect.height);
        }
        ImGui::EndChild();
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
