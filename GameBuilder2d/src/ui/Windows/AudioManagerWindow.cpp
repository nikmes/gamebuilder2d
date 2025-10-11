#include "ui/Windows/AudioManagerWindow.h"
#include "ui/WindowContext.h"
#include "services/audio/AudioManager.h"
#include "services/configuration/ConfigurationManager.h"
#include "services/logger/LogManager.h"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <utility>

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

std::string trimCopy(const std::string& value) {
    auto begin = value.begin();
    while (begin != value.end() && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    auto end = value.end();
    while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(begin, end);
}

} // namespace

namespace gb2d {

namespace {
constexpr const char* kClosePromptModalId = "AudioManagerWindowClosePrompt";
const ImVec4 kUnsavedColor{1.0f, 0.85f, 0.3f, 1.0f};
} // namespace

AudioManagerWindow::AudioManagerWindow() {
    gb2d::logging::LogManager::info("AudioManagerWindow created, subscribing to audio events");
    // Subscribe to audio events
    eventSubscription_ = audio::AudioManager::subscribeToAudioEvents(this);
    gb2d::logging::LogManager::info("AudioManagerWindow subscribed to audio events, subscription ID: {}", eventSubscription_.id);
    refreshInventorySnapshots();
    gb2d::logging::LogManager::info("AudioManagerWindow inventory snapshots refreshed: {} sounds, {} music", soundInventory_.size(), musicInventory_.size());
    refreshConfigState();
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
    
    const bool configDraft = hasConfigDraft();
    const char* configTabLabel = configDraft ? "Config *" : "Config";
    if (ImGui::BeginTabItem(configTabLabel)) {
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

    renderClosePromptModal();
    processPendingCloseAction();

    ImGui::PopID();
}

bool AudioManagerWindow::handleCloseRequest(WindowContext& ctx) {
    requestCloseCallback_ = ctx.requestClose;

    if (hasConfigDraft()) {
        openClosePrompt();
        return false;
    }

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
                clearPreviewStatus();
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
                clearPreviewStatus();
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

    auto metrics = audio::AudioManager::metrics();
    const bool deviceUnavailable = !metrics.initialized || !metrics.deviceReady;
    const bool silentMode = metrics.silentMode;
    const bool previewUnavailable = deviceUnavailable || silentMode;

    if (previewUnavailable && isPlayingPreview_) {
        stopPreview();
        reportPreviewStatus(deviceUnavailable
                                 ? "Preview stopped because the audio device became unavailable."
                                 : "Preview stopped because audio is running in silent mode.",
                             true);
    }

    if (previewUnavailable) {
        const ImVec4 warningColor = deviceUnavailable ? ImVec4(0.94f, 0.33f, 0.24f, 1.0f)
                                                      : ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
        const char* reasonText = deviceUnavailable
            ? "Audio device isn't ready. Previews are unavailable until the device is ready."
            : "Audio is running in silent mode. Enable audio to preview assets.";
        ImGui::TextColored(warningColor, "%s", reasonText);
        ImGui::Spacing();
    }

    if (!previewStatusMessage_.empty()) {
        const ImVec4 statusColor = previewStatusIsError_
            ? ImVec4(0.94f, 0.33f, 0.24f, 1.0f)
            : ImVec4(0.30f, 0.85f, 0.39f, 1.0f);
        ImGui::TextColored(statusColor, "%s", previewStatusMessage_.c_str());
        ImGui::Spacing();
    }
    
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
    
    const bool disableControls = previewUnavailable;

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
    if (disableControls) ImGui::BeginDisabled();
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
    if (disableControls) ImGui::EndDisabled();

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
                    reportPreviewStatus("Preview stopped because the audio playback ended unexpectedly.", true);
                }
            }
        } else if (previewType_ == PreviewType::Music && volumeChanged) {
            audio::AudioManager::setMusicVolume(previewKey_, previewVolume_);
        }
    }
    
    // Play/Stop buttons
    if (disableControls) ImGui::BeginDisabled();
    if (isPreviewingThis) {
        if (ImGui::Button("Stop Preview", ImVec2(120, 0))) {
            stopPreview();
            reportPreviewStatus("Preview stopped.", false);
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
    if (disableControls) ImGui::EndDisabled();
    
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

    auto metrics = audio::AudioManager::metrics();
    ImGui::Text("Initialized: %s", metrics.initialized ? "Yes" : "No");
    ImGui::Text("Device Ready: %s", metrics.deviceReady ? "Yes" : "No");
    ImGui::Text("Silent Mode: %s", metrics.silentMode ? "Yes" : "No");

    if (!configStatusMessage_.empty()) {
        const ImVec4 color = configStatusIsError_ ? ImVec4(0.94f, 0.33f, 0.24f, 1.0f)
                                                 : ImVec4(0.30f, 0.85f, 0.39f, 1.0f);
        ImGui::Spacing();
        ImGui::TextColored(color, "%s", configStatusMessage_.c_str());
        ImGui::Spacing();
    }

    bool changed = false;

    if (ImGui::Checkbox("Enable audio", &configWorking_.enabled)) {
        changed = true;
    }

    if (!configWorking_.enabled) {
        ImGui::BeginDisabled();
    }
    if (ImGui::SliderFloat("Master Volume", &configWorking_.masterVolume, 0.0f, 1.0f, "%.2f")) {
        changed = true;
    }
    if (ImGui::SliderFloat("Music Volume", &configWorking_.musicVolume, 0.0f, 1.0f, "%.2f")) {
        changed = true;
    }
    if (ImGui::SliderFloat("SFX Volume", &configWorking_.sfxVolume, 0.0f, 1.0f, "%.2f")) {
        changed = true;
    }
    if (!configWorking_.enabled) {
        ImGui::EndDisabled();
    }

    int maxSlots = configWorking_.maxConcurrentSounds;
    if (ImGui::SliderInt("Max Concurrent Sounds", &maxSlots, 0, 64, "%d")) {
        configWorking_.maxConcurrentSounds = std::clamp(maxSlots, 0, 256);
        changed = true;
    }

    ImGui::Separator();
    ImGui::Text("Search Paths");

    if (configWorking_.searchPaths.empty()) {
        ImGui::TextDisabled("No search paths configured.");
    }

    const ImGuiStyle& style = ImGui::GetStyle();

    for (std::size_t i = 0; i < configWorking_.searchPaths.size(); ++i) {
        ImGui::PushID(static_cast<int>(i));
        const float removeButtonWidth = ImGui::CalcTextSize("Remove").x + style.FramePadding.x * 2.0f;
        const float inputWidth = std::max(150.0f, ImGui::GetContentRegionAvail().x - removeButtonWidth - style.ItemInnerSpacing.x);
        ImGui::SetNextItemWidth(inputWidth);
        if (ImGui::InputText("##path", &configWorking_.searchPaths[i])) {
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove")) {
            configWorking_.searchPaths.erase(configWorking_.searchPaths.begin() + static_cast<std::ptrdiff_t>(i));
            changed = true;
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }

    const float addButtonWidth = ImGui::CalcTextSize("Add").x + style.FramePadding.x * 2.0f;
    const float addInputWidth = std::max(150.0f, ImGui::GetContentRegionAvail().x - addButtonWidth - style.ItemInnerSpacing.x);
    ImGui::SetNextItemWidth(addInputWidth);
    if (ImGui::InputTextWithHint("##new_path", "Add search path...", &configWorking_.newSearchPath)) {
        changed = true;
    }
    ImGui::SameLine();
    const std::string trimmedNewPath = trimCopy(configWorking_.newSearchPath);
    const bool canAddPath = !trimmedNewPath.empty();
    if (!canAddPath) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Add")) {
        configWorking_.searchPaths.push_back(trimmedNewPath);
        configWorking_.newSearchPath.clear();
        changed = true;
    }
    if (!canAddPath) {
        ImGui::EndDisabled();
    }

    if (changed) {
        configStatusMessage_.clear();
        configStatusIsError_ = false;
    }

    const bool dirty = isConfigDirty();
    const bool hasDraft = hasConfigDraft();

    if (hasDraft) {
        ImGui::Spacing();
        ImGui::TextColored(kUnsavedColor, "Unsaved changes. Apply or revert before closing.");
    }

    ImGui::Separator();
    ImGui::BeginDisabled(!dirty);
    if (ImGui::Button("Apply Changes", ImVec2(140, 0))) {
        applyConfigChanges();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    const bool canRevert = dirty || !trimCopy(configWorking_.newSearchPath).empty();
    if (!canRevert) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Revert", ImVec2(100, 0))) {
        configWorking_ = configBaseline_;
        configWorking_.newSearchPath.clear();
        configStatusMessage_.clear();
        configStatusIsError_ = false;
    }
    if (!canRevert) {
        ImGui::EndDisabled();
    }
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

void AudioManagerWindow::refreshConfigState() {
    auto cfg = audio::AudioManager::config();
    configBaseline_.enabled = cfg.enabled;
    configBaseline_.masterVolume = cfg.masterVolume;
    configBaseline_.musicVolume = cfg.musicVolume;
    configBaseline_.sfxVolume = cfg.sfxVolume;
    configBaseline_.maxConcurrentSounds = static_cast<int>(cfg.maxConcurrentSounds);
    configBaseline_.searchPaths = cfg.searchPaths;
    configBaseline_.newSearchPath.clear();

    configWorking_ = configBaseline_;
}

bool AudioManagerWindow::isConfigDirty() const {
    constexpr float kFloatTolerance = 1e-3f;
    if (configWorking_.enabled != configBaseline_.enabled) return true;
    if (std::abs(configWorking_.masterVolume - configBaseline_.masterVolume) > kFloatTolerance) return true;
    if (std::abs(configWorking_.musicVolume - configBaseline_.musicVolume) > kFloatTolerance) return true;
    if (std::abs(configWorking_.sfxVolume - configBaseline_.sfxVolume) > kFloatTolerance) return true;
    if (configWorking_.maxConcurrentSounds != configBaseline_.maxConcurrentSounds) return true;
    if (configWorking_.searchPaths != configBaseline_.searchPaths) return true;
    return false;
}

bool AudioManagerWindow::hasConfigDraft() const {
    if (isConfigDirty()) {
        return true;
    }
    return !trimCopy(configWorking_.newSearchPath).empty();
}

bool AudioManagerWindow::applyConfigChanges() {
    using gb2d::ConfigurationManager;

    std::vector<std::string> sanitizedPaths;
    sanitizedPaths.reserve(configWorking_.searchPaths.size());
    for (const auto& path : configWorking_.searchPaths) {
        std::string trimmed = trimCopy(path);
        if (!trimmed.empty()) {
            sanitizedPaths.push_back(trimmed);
        }
    }

    ConfigurationManager::set("audio::enabled", configWorking_.enabled);
    ConfigurationManager::set("audio::master_volume", static_cast<double>(std::clamp(configWorking_.masterVolume, 0.0f, 1.0f)));
    ConfigurationManager::set("audio::music_volume", static_cast<double>(std::clamp(configWorking_.musicVolume, 0.0f, 1.0f)));
    ConfigurationManager::set("audio::sfx_volume", static_cast<double>(std::clamp(configWorking_.sfxVolume, 0.0f, 1.0f)));
    ConfigurationManager::set("audio::max_concurrent_sounds", static_cast<int64_t>(std::max(configWorking_.maxConcurrentSounds, 0)));
    ConfigurationManager::set("audio::search_paths", sanitizedPaths);

    bool saved = ConfigurationManager::save();
    if (!saved) {
        configStatusMessage_ = "Failed to save audio configuration.";
        configStatusIsError_ = true;
        return false;
    }

    stopPreview();
    audio::AudioManager::shutdown();
    bool deviceReady = audio::AudioManager::init();
    if (deviceReady) {
        audio::AudioManager::reloadAll();
    }

    refreshConfigState();

    if (!configBaseline_.enabled) {
        configStatusMessage_ = "Audio disabled; manager running in silent mode.";
        configStatusIsError_ = false;
    } else if (!deviceReady) {
        configStatusMessage_ = "Audio settings applied, but audio device is unavailable (silent mode).";
        configStatusIsError_ = true;
    } else {
        configStatusMessage_ = "Audio settings applied.";
        configStatusIsError_ = false;
    }

    return true;
}

void AudioManagerWindow::renderClosePromptModal() {
    if (closePrompt_ != ClosePrompt::UnsavedChanges) {
        return;
    }

    if (ImGui::BeginPopupModal(kClosePromptModalId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("You have unapplied audio configuration changes. Apply them before closing?");
        ImGui::Spacing();
        if (ImGui::Button("Apply & Close", ImVec2(140.0f, 0.0f))) {
            pendingCloseAction_ = PendingCloseAction::ApplyAndClose;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard", ImVec2(110.0f, 0.0f))) {
            pendingCloseAction_ = PendingCloseAction::DiscardAndClose;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(110.0f, 0.0f))) {
            closePrompt_ = ClosePrompt::None;
            requestCloseCallback_ = {};
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void AudioManagerWindow::processPendingCloseAction() {
    if (pendingCloseAction_ == PendingCloseAction::None) {
        return;
    }

    const PendingCloseAction action = pendingCloseAction_;
    pendingCloseAction_ = PendingCloseAction::None;

    switch (action) {
    case PendingCloseAction::ApplyAndClose:
        if (applyConfigChanges()) {
            finalizeClose();
        } else {
            closePrompt_ = ClosePrompt::None;
            requestCloseCallback_ = {};
        }
        break;
    case PendingCloseAction::DiscardAndClose:
        configWorking_ = configBaseline_;
        configWorking_.newSearchPath.clear();
        configStatusMessage_.clear();
        configStatusIsError_ = false;
        finalizeClose();
        break;
    case PendingCloseAction::None:
        break;
    }
}

void AudioManagerWindow::openClosePrompt() {
    if (closePrompt_ == ClosePrompt::UnsavedChanges) {
        return;
    }
    closePrompt_ = ClosePrompt::UnsavedChanges;
    pendingCloseAction_ = PendingCloseAction::None;
    ImGui::OpenPopup(kClosePromptModalId);
}

void AudioManagerWindow::finalizeClose() {
    closePrompt_ = ClosePrompt::None;
    pendingCloseAction_ = PendingCloseAction::None;
    if (requestCloseCallback_) {
        auto callback = requestCloseCallback_;
        requestCloseCallback_ = {};
        callback();
    }
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
            reportPreviewStatus("Preview stopped.", false);
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
            reportPreviewStatus("Sound preview finished.", false);
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
            reportPreviewStatus("Music preview stopped.", false);
        }
    }

    if (event.type == audio::AudioEventType::DeviceError) {
        stopPreview();
        const std::string details = event.details.empty()
            ? "Audio device reported an error. Previews have been stopped."
            : event.details;
        reportPreviewStatus(details, true);
    }
}

void AudioManagerWindow::reportPreviewStatus(std::string message, bool isError) {
    previewStatusMessage_ = std::move(message);
    previewStatusIsError_ = isError;
}

void AudioManagerWindow::clearPreviewStatus() {
    previewStatusMessage_.clear();
    previewStatusIsError_ = false;
}

void AudioManagerWindow::startSoundPreview(const std::string& key) {
    auto metrics = audio::AudioManager::metrics();
    if (!metrics.initialized) {
        reportPreviewStatus("Audio system isn't initialized. Cannot preview sounds.", true);
        return;
    }
    if (!metrics.deviceReady) {
        reportPreviewStatus("Audio device isn't ready. Try again after the device initializes.", true);
        return;
    }
    if (metrics.silentMode) {
        reportPreviewStatus("Audio is running in silent mode. Enable audio to preview sounds.", true);
        return;
    }

    // Stop any existing preview
    stopPreview();
    clearPreviewStatus();
    
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
        reportPreviewStatus("Playing sound preview for '" + key + "'.", false);
    } else {
        gb2d::logging::LogManager::warn("Failed to start sound preview for '{}'", key);
        reportPreviewStatus("Failed to start sound preview for '" + key + "'.", true);
    }
}

void AudioManagerWindow::startMusicPreview(const std::string& key) {
    auto metrics = audio::AudioManager::metrics();
    if (!metrics.initialized) {
        reportPreviewStatus("Audio system isn't initialized. Cannot preview music.", true);
        return;
    }
    if (!metrics.deviceReady) {
        reportPreviewStatus("Audio device isn't ready. Try again after the device initializes.", true);
        return;
    }
    if (metrics.silentMode) {
        reportPreviewStatus("Audio is running in silent mode. Enable audio to preview music.", true);
        return;
    }

    // Stop any existing preview
    stopPreview();
    clearPreviewStatus();
    
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
        reportPreviewStatus("Playing music preview for '" + key + "'.", false);
    } else {
        gb2d::logging::LogManager::warn("Failed to start music preview for '{}'", key);
        reportPreviewStatus("Failed to start music preview for '" + key + "'.", true);
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