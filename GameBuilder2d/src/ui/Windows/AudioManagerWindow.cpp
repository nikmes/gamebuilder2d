#include "ui/Windows/AudioManagerWindow.h"
#include "ui/WindowContext.h"
#include "services/audio/AudioManager.h"
#include "services/logger/LogManager.h"

#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace {

std::string formatPlaybackTime(float seconds) {
    if (!std::isfinite(seconds) || seconds < 0.0f) {
        seconds = 0.0f;
    }
    int totalSeconds = static_cast<int>(seconds + 0.5f);
    int minutes = totalSeconds / 60;
    int remSeconds = totalSeconds % 60;
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", minutes, remSeconds);
    return std::string(buffer);
}

} // namespace

namespace gb2d {

AudioManagerWindow::AudioManagerWindow() {
    gb2d::logging::LogManager::info("AudioManagerWindow created, subscribing to audio events");
    // Subscribe to audio events
    eventSubscription_ = audio::AudioManager::subscribeToAudioEvents(this);
    gb2d::logging::LogManager::info("AudioManagerWindow subscribed to audio events, subscription ID: {}", eventSubscription_.id);
    refreshInventorySnapshots();
    gb2d::logging::LogManager::info("AudioManagerWindow inventory snapshots refreshed: {} sounds, {} music", soundInventory_.size(), musicInventory_.size());
}

AudioManagerWindow::~AudioManagerWindow() {
    // Stop any active preview
    stopPreview();
    
    // Unsubscribe from audio events
    if (eventSubscription_.active) {
        audio::AudioManager::unsubscribeFromAudioEvents(eventSubscription_);
    }
}

void AudioManagerWindow::render(WindowContext& ctx) {
    ImGui::PushID(this);

    // Basic split-pane layout
    ImGui::BeginChild("audio-window-content", ImVec2(0.0f, 0.0f), false);

    // Left panel: Asset list
    ImGui::BeginChild("audio-window-assets", ImVec2(300.0f, 0.0f), true);
    renderAssetList();
    ImGui::EndChild();

    ImGui::SameLine();

    // Right panel: Details
    ImGui::BeginChild("audio-window-details", ImVec2(0.0f, 0.0f), true);
    ImGui::BeginTabBar("audio-details-tabs");
    
    if (ImGui::BeginTabItem("Preview")) {
        renderPreviewPanel();
        ImGui::EndTabItem();
    }
    
    if (ImGui::BeginTabItem("Config")) {
        renderConfigPanel();
        ImGui::EndTabItem();
    }
    
    if (ImGui::BeginTabItem("Diagnostics")) {
        renderDiagnosticsPanel();
        ImGui::EndTabItem();
    }
    
    ImGui::EndTabBar();
    ImGui::EndChild();

    ImGui::EndChild();

    ImGui::PopID();
}

bool AudioManagerWindow::handleCloseRequest(WindowContext& ctx) {
    // For now, always allow close
    return true;
}

void AudioManagerWindow::serialize(nlohmann::json& out) const {
    // TODO: Serialize window state
}

void AudioManagerWindow::deserialize(const nlohmann::json& in) {
    // TODO: Deserialize window state
}

void AudioManagerWindow::renderAssetList() {
    if (inventoryDirty_) {
        refreshInventorySnapshots();
    }
    
    ImGui::Text("Audio Assets");
    ImGui::Separator();
    
    if (ImGui::Button("Refresh")) {
        refreshInventorySnapshots();
    }
    
    ImGui::BeginChild("asset-list", ImVec2(0, 0), true);
    
    // Sounds section
    if (!soundInventory_.empty()) {
        ImGui::Text("Sounds (%zu)", soundInventory_.size());
        ImGui::Separator();
        
        for (const auto& sound : soundInventory_) {
            ImGui::PushID(sound.key.c_str());
            
            bool isPlaceholder = sound.placeholder;
            bool isSelected = (selectedAssetKey_ == sound.key);
            bool isPreviewingThis = (previewType_ == PreviewType::Sound && previewKey_ == sound.key && isPlayingPreview_);
            
            if (isPlaceholder) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            } else if (isPreviewingThis) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
            }
            
            // Play/Stop button
            if (!isPlaceholder) {
                if (isPreviewingThis) {
                    if (ImGui::Button("■")) {
                        stopPreview();
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Stop preview");
                    }
                } else {
                    if (ImGui::Button("▶")) {
                        startSoundPreview(sound.key);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Preview sound");
                    }
                }
                ImGui::SameLine();
            }
            
            if (ImGui::Selectable(sound.key.c_str(), isSelected)) {
                selectedAssetKey_ = sound.key;
            }
            
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Key: %s", sound.key.c_str());
                ImGui::Text("Path: %s", sound.path.c_str());
                ImGui::Text("Duration: %.2fs", sound.durationSeconds);
                ImGui::Text("Ref Count: %zu", sound.refCount);
                ImGui::Text("Sample Rate: %u Hz", sound.sampleRate);
                ImGui::Text("Channels: %u", sound.channels);
                if (isPlaceholder) {
                    ImGui::Text("Status: Placeholder (not loaded)");
                }
                ImGui::EndTooltip();
            }
            
            if (isPlaceholder || isPreviewingThis) {
                ImGui::PopStyleColor();
            }
            
            ImGui::PopID();
        }
    }
    
    // Music section
    if (!musicInventory_.empty()) {
        if (!soundInventory_.empty()) {
            ImGui::Separator();
        }
        
        ImGui::Text("Music (%zu)", musicInventory_.size());
        ImGui::Separator();
        
        for (const auto& music : musicInventory_) {
            ImGui::PushID(music.key.c_str());
            
            bool isPlaceholder = music.placeholder;
            bool isSelected = (selectedAssetKey_ == music.key);
            bool isPreviewingThis = (previewType_ == PreviewType::Music && previewKey_ == music.key && isPlayingPreview_);
            
            if (isPlaceholder) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            } else if (isPreviewingThis) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
            }
            
            // Play/Stop button
            if (!isPlaceholder) {
                if (isPreviewingThis) {
                    if (ImGui::Button("■")) {
                        stopPreview();
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Stop preview");
                    }
                } else {
                    if (ImGui::Button("▶")) {
                        startMusicPreview(music.key);
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Preview music");
                    }
                }
                ImGui::SameLine();
            }
            
            if (ImGui::Selectable(music.key.c_str(), isSelected)) {
                selectedAssetKey_ = music.key;
            }
            
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Key: %s", music.key.c_str());
                ImGui::Text("Path: %s", music.path.c_str());
                ImGui::Text("Duration: %.2fs", music.durationSeconds);
                ImGui::Text("Ref Count: %zu", music.refCount);
                ImGui::Text("Sample Rate: %u Hz", music.sampleRate);
                ImGui::Text("Channels: %u", music.channels);
                if (isPlaceholder) {
                    ImGui::Text("Status: Placeholder (not loaded)");
                }
                ImGui::EndTooltip();
            }
            
            if (isPlaceholder || isPreviewingThis) {
                ImGui::PopStyleColor();
            }
            
            ImGui::PopID();
        }
    }
    
    if (soundInventory_.empty() && musicInventory_.empty()) {
        ImGui::Text("No audio assets loaded");
    }
    
    ImGui::EndChild();
}

void AudioManagerWindow::renderPreviewPanel() {
    ImGui::Text("Audio Preview");
    ImGui::Separator();
    
    if (selectedAssetKey_.empty()) {
        ImGui::TextWrapped("Select an audio asset from the list to preview it.");
        return;
    }
    
    // Find the selected asset info
    bool foundSound = false;
    bool foundMusic = false;
    audio::SoundInventoryRecord soundInfo;
    audio::MusicInventoryRecord musicInfo;
    
    for (const auto& sound : soundInventory_) {
        if (sound.key == selectedAssetKey_) {
            foundSound = true;
            soundInfo = sound;
            break;
        }
    }
    
    if (!foundSound) {
        for (const auto& music : musicInventory_) {
            if (music.key == selectedAssetKey_) {
                foundMusic = true;
                musicInfo = music;
                break;
            }
        }
    }
    
    if (!foundSound && !foundMusic) {
        ImGui::Text("Selected asset not found in inventory.");
        return;
    }
    
    // Display asset info
    ImGui::BeginGroup();
    ImGui::Text("Asset: %s", selectedAssetKey_.c_str());
    
    if (foundSound) {
        ImGui::Text("Type: Sound");
        ImGui::Text("Duration: %.2fs", soundInfo.durationSeconds);
        ImGui::Text("Sample Rate: %u Hz", soundInfo.sampleRate);
        ImGui::Text("Channels: %u", soundInfo.channels);
        ImGui::Text("Path: %s", soundInfo.path.c_str());
        
        if (soundInfo.placeholder) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Warning: Placeholder (not loaded)");
        }
    } else if (foundMusic) {
        ImGui::Text("Type: Music");
        ImGui::Text("Duration: %.2fs", musicInfo.durationSeconds);
        ImGui::Text("Sample Rate: %u Hz", musicInfo.sampleRate);
        ImGui::Text("Channels: %u", musicInfo.channels);
        ImGui::Text("Path: %s", musicInfo.path.c_str());
        
        if (musicInfo.placeholder) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Warning: Placeholder (not loaded)");
        }
    }
    ImGui::EndGroup();
    
    ImGui::Separator();
    
    // Preview controls
    audio::MusicPlaybackStatus musicStatus{};
    if (foundMusic) {
        musicStatus = audio::AudioManager::musicPlaybackStatus(selectedAssetKey_);
    }

    bool isPreviewingThis = (previewKey_ == selectedAssetKey_ && isPlayingPreview_);
    if (previewType_ == PreviewType::Music && previewKey_ == selectedAssetKey_ && musicStatus.valid && musicStatus.paused) {
        isPreviewingThis = true;
    }
    bool canPreview = (foundSound && !soundInfo.placeholder) || (foundMusic && !musicInfo.placeholder);
    
    if (!canPreview) {
        ImGui::TextWrapped("This asset cannot be previewed (placeholder or not loaded).");
        return;
    }
    
    // Status & progress
    std::string statusLabel = "Stopped";
    if (previewType_ == PreviewType::Sound && isPreviewingThis && previewKey_ == selectedAssetKey_) {
        statusLabel = "Playing";
    } else if (previewType_ == PreviewType::Music && previewKey_ == selectedAssetKey_) {
        if (musicStatus.paused) {
            statusLabel = "Paused";
        } else if (musicStatus.playing || isPreviewingThis) {
            statusLabel = "Playing";
        }
    }

    ImVec4 statusColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    if (statusLabel == "Playing") {
        statusColor = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
    } else if (statusLabel == "Paused") {
        statusColor = ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
    }
    ImGui::TextColored(statusColor, "Status: %s", statusLabel.c_str());

    if (foundMusic && isPreviewingThis) {
        float duration = musicStatus.durationSeconds > 0.0f ? musicStatus.durationSeconds : musicInfo.durationSeconds;
        float position = musicStatus.positionSeconds;
        if (duration > 0.0f) {
            position = std::clamp(position, 0.0f, duration);
            float progress = std::clamp(duration > 0.0f ? position / duration : 0.0f, 0.0f, 1.0f);
            ImGui::ProgressBar(progress, ImVec2(200.0f, 0.0f));
            ImGui::SameLine();
            ImGui::Text("%s / %s", formatPlaybackTime(position).c_str(), formatPlaybackTime(duration).c_str());
        } else {
            ImGui::Text("Time: %s", formatPlaybackTime(position).c_str());
        }
        if (musicStatus.paused) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Playback paused");
        }
    }

    bool volumeChanged = false;
    bool panChanged = false;
    bool pitchChanged = false;

    // Volume control
    ImGui::Text("Preview Volume:");
    ImGui::SetNextItemWidth(200.0f);
    volumeChanged = ImGui::SliderFloat("##preview_volume", &previewVolume_, 0.0f, 1.0f, "%.2f");

    // Pan control (sounds only)
    ImGui::Text("Preview Pan:");
    ImGui::SetNextItemWidth(200.0f);
    const bool disablePanPitch = !foundSound;
    if (disablePanPitch) ImGui::BeginDisabled();
    panChanged = ImGui::SliderFloat("##preview_pan", &previewPan_, 0.0f, 1.0f, "%.2f");
    if (disablePanPitch) {
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::SetTooltip("Pan adjustment is available for sound previews only.");
        }
    }

    // Pitch control (sounds only)
    ImGui::Text("Preview Pitch:");
    ImGui::SetNextItemWidth(200.0f);
    if (disablePanPitch) ImGui::BeginDisabled();
    pitchChanged = ImGui::SliderFloat("##preview_pitch", &previewPitch_, 0.5f, 2.0f, "%.2f");
    if (disablePanPitch) {
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::SetTooltip("Pitch adjustment is available for sound previews only.");
        }
    }

    if (isPreviewingThis) {
        if (previewType_ == PreviewType::Sound && previewSoundHandle_.valid()) {
            if (volumeChanged || panChanged || pitchChanged) {
                audio::PlaybackParams params;
                params.volume = previewVolume_;
                params.pan = previewPan_;
                params.pitch = previewPitch_;
                if (!audio::AudioManager::updateSoundPlayback(previewSoundHandle_, params)) {
                    // Handle invalidation gracefully
                    previewSoundHandle_ = {};
                    isPlayingPreview_ = false;
                    previewKey_.clear();
                    previewType_ = PreviewType::None;
                }
            }
        } else if (previewType_ == PreviewType::Music && volumeChanged) {
            audio::AudioManager::setMusicVolume(previewKey_, previewVolume_);
        }
    }
    
    // Play/Stop buttons
    if (isPreviewingThis) {
        if (ImGui::Button("Stop Preview", ImVec2(120, 0))) {
            stopPreview();
        }
    } else {
        if (ImGui::Button("Play Preview", ImVec2(120, 0))) {
            if (foundSound) {
                startSoundPreview(selectedAssetKey_);
            } else if (foundMusic) {
                startMusicPreview(selectedAssetKey_);
            }
        }
    }
    
    // For music, show additional controls
    if (foundMusic && isPreviewingThis) {
        ImGui::Separator();
        ImGui::Text("Music Playback:");
        
        // Show pause/resume button
        ImGui::SameLine(0, 10);
        bool canPause = musicStatus.valid ? (musicStatus.playing && !musicStatus.paused) : true;
        if (!canPause) ImGui::BeginDisabled();
        if (ImGui::Button("Pause")) {
            audio::AudioManager::pauseMusic(selectedAssetKey_);
        }
        if (!canPause) ImGui::EndDisabled();
        ImGui::SameLine();
        bool canResume = musicStatus.valid ? musicStatus.paused : false;
        if (!canResume) ImGui::BeginDisabled();
        if (ImGui::Button("Resume")) {
            audio::AudioManager::resumeMusic(selectedAssetKey_);
        }
        if (!canResume) ImGui::EndDisabled();
    }
}

void AudioManagerWindow::renderConfigPanel() {
    ImGui::Text("Audio Configuration");
    ImGui::Separator();
    
    // Placeholder for config
    ImGui::Text("(Config panel coming soon)");
}

void AudioManagerWindow::renderDiagnosticsPanel() {
    ImGui::Text("Audio Diagnostics");
    ImGui::Separator();
    
    // Show metrics
    auto metrics = audio::AudioManager::metrics();
    ImGui::Text("Initialized: %s", metrics.initialized ? "Yes" : "No");
    ImGui::Text("Device Ready: %s", metrics.deviceReady ? "Yes" : "No");
    ImGui::Text("Silent Mode: %s", metrics.silentMode ? "Yes" : "No");
    ImGui::Text("Loaded Sounds: %zu", metrics.loadedSounds);
    ImGui::Text("Loaded Music: %zu", metrics.loadedMusic);
    ImGui::Text("Active Instances: %zu / %zu", metrics.activeSoundInstances, metrics.maxSoundSlots);
    
    ImGui::Separator();
    ImGui::Text("Event Log");
    
    if (ImGui::Button("Clear Log")) {
        eventLog_.clear();
    }
    
    ImGui::BeginChild("event-log", ImVec2(0, 0), true);
    
    for (const auto& entry : eventLog_) {
        const char* typeStr = "";
        switch (entry.event.type) {
            case audio::AudioEventType::SoundLoaded: typeStr = "SOUND_LOAD"; break;
            case audio::AudioEventType::SoundUnloaded: typeStr = "SOUND_UNLOAD"; break;
            case audio::AudioEventType::MusicLoaded: typeStr = "MUSIC_LOAD"; break;
            case audio::AudioEventType::MusicUnloaded: typeStr = "MUSIC_UNLOAD"; break;
            case audio::AudioEventType::SoundPlaybackStarted: typeStr = "SOUND_START"; break;
            case audio::AudioEventType::SoundPlaybackStopped: typeStr = "SOUND_STOP"; break;
            case audio::AudioEventType::MusicPlaybackStarted: typeStr = "MUSIC_START"; break;
            case audio::AudioEventType::MusicPlaybackPaused: typeStr = "MUSIC_PAUSE"; break;
            case audio::AudioEventType::MusicPlaybackResumed: typeStr = "MUSIC_RESUME"; break;
            case audio::AudioEventType::MusicPlaybackStopped: typeStr = "MUSIC_STOP"; break;
            case audio::AudioEventType::PreviewStarted: typeStr = "PREVIEW_START"; break;
            case audio::AudioEventType::PreviewStopped: typeStr = "PREVIEW_STOP"; break;
            case audio::AudioEventType::ConfigChanged: typeStr = "CONFIG_CHANGE"; break;
            case audio::AudioEventType::DeviceError: typeStr = "DEVICE_ERROR"; break;
        }
        
        ImGui::Text("[%s] %s", entry.formattedTime.c_str(), typeStr);
        if (!entry.event.key.empty()) {
            ImGui::SameLine();
            ImGui::Text("'%s'", entry.event.key.c_str());
        }
        if (!entry.event.details.empty()) {
            ImGui::SameLine();
            ImGui::Text("(%s)", entry.event.details.c_str());
        }
    }
    
    if (eventLog_.empty()) {
        ImGui::Text("No events recorded");
    }
    
    ImGui::EndChild();
}

void AudioManagerWindow::onAudioEvent(const audio::AudioEvent& event) {
    gb2d::logging::LogManager::info("AudioManagerWindow received audio event: type={}, key='{}', details='{}'", 
                 static_cast<int>(event.type), event.key, event.details);
    handleEvent(event);
}

void AudioManagerWindow::refreshInventorySnapshots() {
    soundInventory_ = audio::AudioManager::captureSoundInventorySnapshot();
    musicInventory_ = audio::AudioManager::captureMusicInventorySnapshot();
    gb2d::logging::LogManager::info("AudioManagerWindow refreshed inventory: {} sounds, {} music tracks", 
                 soundInventory_.size(), musicInventory_.size());
    inventoryDirty_ = false;
}

void AudioManagerWindow::handleEvent(const audio::AudioEvent& event) {
    gb2d::logging::LogManager::info("AudioManagerWindow::handleEvent called for event type {}", static_cast<int>(event.type));
    // Add to event log
    EventLogEntry entry{event, ""};
    
    // Format timestamp
    auto time_t = std::chrono::system_clock::from_time_t(event.timestampMs / 1000);
    auto time = std::chrono::system_clock::to_time_t(time_t);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%H:%M:%S");
    entry.formattedTime = ss.str();
    
    eventLog_.push_back(entry);
    
    // Keep log size limited
    if (eventLog_.size() > maxEventLogSize_) {
        eventLog_.erase(eventLog_.begin());
    }
    
    // Mark inventory as potentially dirty for certain events
    if (event.type == audio::AudioEventType::SoundLoaded ||
        event.type == audio::AudioEventType::SoundUnloaded ||
        event.type == audio::AudioEventType::MusicLoaded ||
        event.type == audio::AudioEventType::MusicUnloaded) {
        inventoryDirty_ = true;
    }
    
    // Update preview state based on events
    if (event.type == audio::AudioEventType::PreviewStopped) {
        if (event.key == previewKey_) {
            isPlayingPreview_ = false;
            previewKey_.clear();
            previewType_ = PreviewType::None;
            previewSoundHandle_ = {};
        }
    }
    
    // Handle sound playback stopped (when sound finishes naturally)
    if (event.type == audio::AudioEventType::SoundPlaybackStopped) {
        if (previewType_ == PreviewType::Sound && event.key == previewKey_) {
            isPlayingPreview_ = false;
            previewKey_.clear();
            previewType_ = PreviewType::None;
            previewSoundHandle_ = {};
            gb2d::logging::LogManager::info("Sound preview finished for '{}'", event.key);
        }
    }
    
    // Handle music playback stopped (when music finishes or is stopped)
    if (event.type == audio::AudioEventType::MusicPlaybackStopped) {
        if (previewType_ == PreviewType::Music && event.key == previewKey_) {
            isPlayingPreview_ = false;
            previewKey_.clear();
            previewType_ = PreviewType::None;
            previewSoundHandle_ = {};
            gb2d::logging::LogManager::info("Music preview stopped for '{}'", event.key);
        }
    }
}

void AudioManagerWindow::startSoundPreview(const std::string& key) {
    // Stop any existing preview
    stopPreview();
    
    // Start new sound preview using playSound
    audio::PlaybackParams params;
    params.volume = previewVolume_;
    params.pan = previewPan_;
    params.pitch = previewPitch_;
    auto handle = audio::AudioManager::playSound(key, params);
    
    if (handle.valid()) {
        previewKey_ = key;
        previewType_ = PreviewType::Sound;
        isPlayingPreview_ = true;
        previewSoundHandle_ = handle;
        gb2d::logging::LogManager::info("Started sound preview for '{}'", key);
    } else {
        gb2d::logging::LogManager::warn("Failed to start sound preview for '{}'", key);
    }
}

void AudioManagerWindow::startMusicPreview(const std::string& key) {
    // Stop any existing preview
    stopPreview();
    
    // Start new music preview using playMusic
    bool success = audio::AudioManager::playMusic(key);
    
    if (success) {
        // Set volume for music
        audio::AudioManager::setMusicVolume(key, previewVolume_);
        previewKey_ = key;
        previewType_ = PreviewType::Music;
        isPlayingPreview_ = true;
        previewSoundHandle_ = {};
        gb2d::logging::LogManager::info("Started music preview for '{}'", key);
    } else {
        gb2d::logging::LogManager::warn("Failed to start music preview for '{}'", key);
    }
}

void AudioManagerWindow::stopPreview() {
    if (!isPlayingPreview_ || previewKey_.empty()) {
        return;
    }
    
    // Stop based on preview type
    if (previewType_ == PreviewType::Music) {
        audio::AudioManager::stopMusic(previewKey_);
        gb2d::logging::LogManager::info("Stopped music preview for '{}'", previewKey_);
    } else if (previewType_ == PreviewType::Sound) {
        bool stopped = false;
        if (previewSoundHandle_.valid()) {
            stopped = audio::AudioManager::stopSound(previewSoundHandle_);
        }
        if (!stopped) {
            audio::AudioManager::stopAllSounds();
        }
        gb2d::logging::LogManager::info("Stopped sound preview for '{}'", previewKey_);
    }
    
    // Clear preview state
    isPlayingPreview_ = false;
    previewKey_.clear();
    previewType_ = PreviewType::None;
    previewSoundHandle_ = {};
}

} // namespace gb2d