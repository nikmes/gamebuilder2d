#include "ui/Windows/AudioManagerWindow.h"
#include "ui/WindowContext.h"
#include "services/audio/AudioManager.h"
#include "services/configuration/ConfigurationManager.h"
#include "services/logger/LogManager.h"
#include "ImGuiFileDialog.h"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <iomanip>
#include <optional>
#include <filesystem>
#include <sstream>
#include <utility>
#include <unordered_map>

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
    const auto addr = reinterpret_cast<std::uintptr_t>(this);
    soundLoadDialogId_ = "AudioSoundLoadDlg##" + std::to_string(addr);
    musicLoadDialogId_ = "AudioMusicLoadDlg##" + std::to_string(addr);

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

    // Release any assets we loaded during this session that weren't persisted
    discardSessionPreloads();
    
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

    renderLoadAssetControls();
    ImGui::Separator();
    
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

void AudioManagerWindow::renderLoadAssetControls() {
    ImGui::Spacing();
    if (!ImGui::CollapsingHeader("Load Assets", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        return;
    }

    const ImVec4 successColor{0.30f, 0.85f, 0.39f, 1.0f};
    const ImVec4 errorColor{0.94f, 0.33f, 0.24f, 1.0f};
    const ImVec4 warnColor{1.0f, 0.85f, 0.3f, 1.0f};

    constexpr const char* kSoundFilters = "Audio{.wav,.ogg,.mp3,.flac,.mod,.xm,.it,.s3m,.aif,.aiff,.wav}";
    constexpr const char* kMusicFilters = "Audio Streams{.ogg,.mp3,.flac,.wav,.mod,.xm,.it,.s3m,.aif,.aiff}";

    auto renderForm = [&](const char* label,
                          LoadAssetFormState& form,
                          const std::string& dialogId,
                          const char* dialogTitle,
                          const char* filters,
                          auto loaderCallback) {
        ImGui::PushID(label);
        ImGui::Text("%s", label);

        if (!form.statusMessage.empty()) {
            const ImVec4 color = form.statusIsError
                ? errorColor
                : (form.statusIsWarning ? warnColor : successColor);
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextColored(color, "%s", form.statusMessage.c_str());
            ImGui::PopTextWrapPos();
        }

        const ImGuiStyle& style = ImGui::GetStyle();
        const float loadButtonWidth = 150.0f;
        const float manualButtonWidth = 140.0f;
        const float labelSpacing = style.ItemInnerSpacing.x;

        float identifierWidth = std::max(120.0f, ImGui::GetContentRegionAvail().x - loadButtonWidth - labelSpacing);
        ImGui::SetNextItemWidth(identifierWidth);
        if (ImGui::InputTextWithHint("##identifier",
                                     "Identifier or relative path...",
                                     &form.identifier)) {
            form.statusMessage.clear();
            form.statusIsError = false;
            form.statusIsWarning = false;
        }

        ImGui::SameLine();
        if (ImGui::Button("Load From File...", ImVec2(loadButtonWidth, 0.0f))) {
            IGFD::FileDialogConfig cfg;
            std::string initialPath = determineBrowseDirectory();
            cfg.path = initialPath.empty() ? std::string(".") : initialPath;
            cfg.flags = ImGuiFileDialogFlags_Modal;
            ImGuiFileDialog::Instance()->OpenDialog(dialogId.c_str(), dialogTitle, filters, cfg);
            form.statusMessage.clear();
            form.statusIsError = false;
            form.statusIsWarning = false;
        }

        ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x));
        if (ImGui::InputTextWithHint("##alias",
                                     "Optional alias (leave empty to use identifier)",
                                     &form.alias)) {
            form.statusMessage.clear();
            form.statusIsError = false;
            form.statusIsWarning = false;
        }

        const std::string trimmedIdentifier = trimCopy(form.identifier);
        const bool canManualLoad = !trimmedIdentifier.empty();
        if (!canManualLoad) {
            ImGui::BeginDisabled();
        }
        ImGui::SameLine(0.0f, labelSpacing);
        if (ImGui::Button("Load Identifier", ImVec2(manualButtonWidth, 0.0f))) {
            loaderCallback(trimmedIdentifier, trimCopy(form.alias), form);
        }
        if (!canManualLoad) {
            ImGui::EndDisabled();
        }

        if (ImGuiFileDialog::Instance()->Display(dialogId.c_str(), ImGuiWindowFlags_NoCollapse, ImVec2(600, 400))) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                auto selectedPath = ImGuiFileDialog::Instance()->GetFilePathName();
                std::filesystem::path fsPath{selectedPath};
                if (fsPath.has_parent_path()) {
                    try {
                        lastLoadDirectory_ = fsPath.parent_path().string();
                    } catch (...) {
                        // ignore issues capturing directory
                    }
                }

                std::string identifier = makeIdentifierFromSelection(fsPath);
                form.identifier = identifier;

                std::string alias = trimCopy(form.alias);
                if (alias.empty()) {
                    try {
                        alias = fsPath.stem().string();
                        form.alias = alias;
                    } catch (...) {
                        alias.clear();
                    }
                }

                loaderCallback(identifier, trimCopy(form.alias), form);
            }
            ImGuiFileDialog::Instance()->Close();
        }

        ImGui::Spacing();
        ImGui::PopID();
    };

    renderForm("Sound", soundLoadForm_, soundLoadDialogId_, "Select Sound", kSoundFilters,
               [&](const std::string& identifier,
                   const std::string& alias,
                   LoadAssetFormState& form) {
        std::optional<std::string> aliasOpt;
        if (!alias.empty()) {
            aliasOpt = alias;
        }

        auto result = audio::AudioManager::acquireSound(identifier, aliasOpt);
        if (result.key.empty()) {
            form.statusIsError = true;
            form.statusIsWarning = false;
            form.statusMessage = "Failed to load sound. Verify the identifier and audio device.";
            return;
        }

        noteLoadedSoundAsset(result.key, identifier, aliasOpt, result.newlyLoaded);

        if (result.placeholder) {
            form.statusIsError = false;
            form.statusIsWarning = true;
            form.statusMessage = "Sound added as placeholder (device unavailable or file missing).";
        } else {
            form.statusIsError = false;
            form.statusIsWarning = false;
            form.statusMessage = "Sound loaded successfully.";
        }
    });

    renderForm("Music", musicLoadForm_, musicLoadDialogId_, "Select Music", kMusicFilters,
               [&](const std::string& identifier,
                   const std::string& alias,
                   LoadAssetFormState& form) {
        std::optional<std::string> aliasOpt;
        if (!alias.empty()) {
            aliasOpt = alias;
        }

        auto result = audio::AudioManager::acquireMusic(identifier, aliasOpt);
        if (result.key.empty()) {
            form.statusIsError = true;
            form.statusIsWarning = false;
            form.statusMessage = "Failed to load music. Verify the identifier and audio device.";
            return;
        }

    noteLoadedMusicAsset(result.key, identifier, aliasOpt, result.newlyLoaded);

        if (result.placeholder) {
            form.statusIsError = false;
            form.statusIsWarning = true;
            form.statusMessage = "Music added as placeholder (device unavailable or file missing).";
        } else {
            form.statusIsError = false;
            form.statusIsWarning = false;
            form.statusMessage = "Music loaded successfully.";
        }
    });

    if (!pendingSoundPreloads_.empty() || !pendingMusicPreloads_.empty()) {
        ImGui::TextColored(warnColor, "Pending preloads will be saved when you apply changes.");
    }

    ImGui::Spacing();
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

    if (ImGui::Checkbox("Enable diagnostics logging", &configWorking_.diagnosticsLoggingEnabled)) {
        changed = true;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("When disabled, the Diagnostics tab stops recording new audio events.");
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

    ImGui::Separator();
    ImGui::Text("Preload Sounds");

    auto removePendingEntry = [&](std::vector<std::string>& list, const std::string& value) {
        const std::string canonical = canonicalizePreloadInput(value);
        list.erase(std::remove_if(list.begin(), list.end(), [&](const std::string& existing) {
            return canonicalizePreloadInput(existing) == canonical;
        }), list.end());
    };

    auto removeAliasEntry = [&](std::unordered_map<std::string, std::string>& aliases, const std::string& value) {
        aliases.erase(canonicalizePreloadInput(value));
    };

    if (configWorking_.preloadSounds.empty()) {
        ImGui::TextDisabled("No sounds configured for preload.");
    }
    for (std::size_t i = 0; i < configWorking_.preloadSounds.size(); ++i) {
        ImGui::PushID(static_cast<int>(i + 1000));
        const float removeWidth = ImGui::CalcTextSize("Remove").x + style.FramePadding.x * 2.0f;
        const float identifierWidth = std::max(150.0f, ImGui::GetContentRegionAvail().x - removeWidth - style.ItemInnerSpacing.x);
        const std::string previousValue = configWorking_.preloadSounds[i];
        const std::string previousCanonical = canonicalizePreloadInput(previousValue);

        ImGui::SetNextItemWidth(identifierWidth);
        if (ImGui::InputText("##preload_sound", &configWorking_.preloadSounds[i])) {
            changed = true;
            const std::string newCanonical = canonicalizePreloadInput(configWorking_.preloadSounds[i]);
            if (newCanonical != previousCanonical) {
                auto aliasIt = configWorking_.preloadSoundAliases.find(previousCanonical);
                if (aliasIt != configWorking_.preloadSoundAliases.end()) {
                    std::string aliasValue = std::move(aliasIt->second);
                    configWorking_.preloadSoundAliases.erase(aliasIt);
                    if (!newCanonical.empty()) {
                        configWorking_.preloadSoundAliases.emplace(newCanonical, std::move(aliasValue));
                    }
                }
            }
        }

        const std::string currentValue = configWorking_.preloadSounds[i];

        ImGui::SameLine();
        if (ImGui::Button("Remove")) {
            auto removed = currentValue;
            configWorking_.preloadSounds.erase(configWorking_.preloadSounds.begin() + static_cast<std::ptrdiff_t>(i));
            removePendingEntry(pendingSoundPreloads_, removed);
            removeAliasEntry(configWorking_.preloadSoundAliases, removed);
            changed = true;
            ImGui::PopID();
            break;
        }

        const std::string canonical = canonicalizePreloadInput(configWorking_.preloadSounds[i]);
        if (!canonical.empty()) {
            auto aliasIt = configWorking_.preloadSoundAliases.find(canonical);
            if (aliasIt == configWorking_.preloadSoundAliases.end()) {
                aliasIt = configWorking_.preloadSoundAliases.emplace(canonical, std::string{}).first;
            }
            ImGui::PushID("alias");
            ImGui::SetNextItemWidth(identifierWidth);
            if (ImGui::InputTextWithHint("##preload_sound_alias", "Alias (optional)", &aliasIt->second)) {
                changed = true;
            }
            ImGui::PopID();
        }

        ImGui::PopID();
    }

    const float addSoundWidth = ImGui::CalcTextSize("Add Sound").x + style.FramePadding.x * 2.0f;
    const float addSoundInputWidth = std::max(150.0f, ImGui::GetContentRegionAvail().x - addSoundWidth - style.ItemInnerSpacing.x);
    ImGui::SetNextItemWidth(addSoundInputWidth);
    if (ImGui::InputTextWithHint("##new_preload_sound", "Add sound identifier...", &configWorking_.newPreloadSound)) {
        changed = true;
    }
    ImGui::SetNextItemWidth(addSoundInputWidth);
    if (ImGui::InputTextWithHint("##new_preload_sound_alias", "Alias (optional)", &configWorking_.newPreloadSoundAlias)) {
        changed = true;
    }
    ImGui::SameLine();
    const std::string trimmedSoundPreload = trimCopy(configWorking_.newPreloadSound);
    const bool canAddSoundPreload = !trimmedSoundPreload.empty();
    if (!canAddSoundPreload) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Add Sound", ImVec2(addSoundWidth, 0.0f))) {
        const bool inserted = ensurePreloadEntry(configWorking_.preloadSounds, trimmedSoundPreload);
        ensurePreloadEntry(pendingSoundPreloads_, trimmedSoundPreload);
        const std::string canonical = canonicalizePreloadInput(trimmedSoundPreload);
        const std::string aliasTrimmed = trimCopy(configWorking_.newPreloadSoundAlias);
        if (!aliasTrimmed.empty()) {
            configWorking_.preloadSoundAliases[canonical] = aliasTrimmed;
        } else {
            configWorking_.preloadSoundAliases.erase(canonical);
        }
        configWorking_.newPreloadSound.clear();
        configWorking_.newPreloadSoundAlias.clear();
        changed = inserted || changed;
        if (!inserted) {
            configStatusMessage_ = "Sound identifier already present.";
            configStatusIsError_ = false;
        }
    }
    if (!canAddSoundPreload) {
        ImGui::EndDisabled();
    }

    ImGui::Separator();
    ImGui::Text("Preload Music");

    if (configWorking_.preloadMusic.empty()) {
        ImGui::TextDisabled("No music configured for preload.");
    }
    for (std::size_t i = 0; i < configWorking_.preloadMusic.size(); ++i) {
        ImGui::PushID(static_cast<int>(i + 2000));
        const float removeWidth = ImGui::CalcTextSize("Remove").x + style.FramePadding.x * 2.0f;
        const float identifierWidth = std::max(150.0f, ImGui::GetContentRegionAvail().x - removeWidth - style.ItemInnerSpacing.x);
        const std::string previousValue = configWorking_.preloadMusic[i];
        const std::string previousCanonical = canonicalizePreloadInput(previousValue);

        ImGui::SetNextItemWidth(identifierWidth);
        if (ImGui::InputText("##preload_music", &configWorking_.preloadMusic[i])) {
            changed = true;
            const std::string newCanonical = canonicalizePreloadInput(configWorking_.preloadMusic[i]);
            if (newCanonical != previousCanonical) {
                auto aliasIt = configWorking_.preloadMusicAliases.find(previousCanonical);
                if (aliasIt != configWorking_.preloadMusicAliases.end()) {
                    std::string aliasValue = std::move(aliasIt->second);
                    configWorking_.preloadMusicAliases.erase(aliasIt);
                    if (!newCanonical.empty()) {
                        configWorking_.preloadMusicAliases.emplace(newCanonical, std::move(aliasValue));
                    }
                }
            }
        }

        const std::string currentValue = configWorking_.preloadMusic[i];

        ImGui::SameLine();
        if (ImGui::Button("Remove")) {
            auto removed = currentValue;
            configWorking_.preloadMusic.erase(configWorking_.preloadMusic.begin() + static_cast<std::ptrdiff_t>(i));
            removePendingEntry(pendingMusicPreloads_, removed);
            removeAliasEntry(configWorking_.preloadMusicAliases, removed);
            changed = true;
            ImGui::PopID();
            break;
        }

        const std::string canonical = canonicalizePreloadInput(configWorking_.preloadMusic[i]);
        if (!canonical.empty()) {
            auto aliasIt = configWorking_.preloadMusicAliases.find(canonical);
            if (aliasIt == configWorking_.preloadMusicAliases.end()) {
                aliasIt = configWorking_.preloadMusicAliases.emplace(canonical, std::string{}).first;
            }
            ImGui::PushID("alias");
            ImGui::SetNextItemWidth(identifierWidth);
            if (ImGui::InputTextWithHint("##preload_music_alias", "Alias (optional)", &aliasIt->second)) {
                changed = true;
            }
            ImGui::PopID();
        }

        ImGui::PopID();
    }

    const float addMusicWidth = ImGui::CalcTextSize("Add Music").x + style.FramePadding.x * 2.0f;
    const float addMusicInputWidth = std::max(150.0f, ImGui::GetContentRegionAvail().x - addMusicWidth - style.ItemInnerSpacing.x);
    ImGui::SetNextItemWidth(addMusicInputWidth);
    if (ImGui::InputTextWithHint("##new_preload_music", "Add music identifier...", &configWorking_.newPreloadMusic)) {
        changed = true;
    }
    ImGui::SetNextItemWidth(addMusicInputWidth);
    if (ImGui::InputTextWithHint("##new_preload_music_alias", "Alias (optional)", &configWorking_.newPreloadMusicAlias)) {
        changed = true;
    }
    ImGui::SameLine();
    const std::string trimmedMusicPreload = trimCopy(configWorking_.newPreloadMusic);
    const bool canAddMusicPreload = !trimmedMusicPreload.empty();
    if (!canAddMusicPreload) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Add Music", ImVec2(addMusicWidth, 0.0f))) {
        const bool inserted = ensurePreloadEntry(configWorking_.preloadMusic, trimmedMusicPreload);
        ensurePreloadEntry(pendingMusicPreloads_, trimmedMusicPreload);
        const std::string canonical = canonicalizePreloadInput(trimmedMusicPreload);
        const std::string aliasTrimmed = trimCopy(configWorking_.newPreloadMusicAlias);
        if (!aliasTrimmed.empty()) {
            configWorking_.preloadMusicAliases[canonical] = aliasTrimmed;
        } else {
            configWorking_.preloadMusicAliases.erase(canonical);
        }
        configWorking_.newPreloadMusic.clear();
        configWorking_.newPreloadMusicAlias.clear();
        changed = inserted || changed;
        if (!inserted) {
            configStatusMessage_ = "Music identifier already present.";
            configStatusIsError_ = false;
        }
    }
    if (!canAddMusicPreload) {
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
    const bool canRevert = dirty || !trimCopy(configWorking_.newSearchPath).empty() ||
                           !trimCopy(configWorking_.newPreloadSound).empty() ||
                           !trimCopy(configWorking_.newPreloadMusic).empty();
    if (!canRevert) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Revert", ImVec2(100, 0))) {
        configWorking_ = configBaseline_;
        configWorking_.newSearchPath.clear();
        configWorking_.newPreloadSound.clear();
        configWorking_.newPreloadMusic.clear();
        configStatusMessage_.clear();
        configStatusIsError_ = false;
        discardSessionPreloads();
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
    const bool loggingEnabled = configBaseline_.diagnosticsLoggingEnabled;
    if (!loggingEnabled) {
        ImGui::TextDisabled("Diagnostics logging is disabled. Enable it in the Config tab to capture new events.");
    }
    
    if (ImGui::Button("Clear Log")) {
        eventLog_.clear();
    }
    
    if (!loggingEnabled) {
        ImGui::BeginDisabled();
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
    if (!loggingEnabled) {
        ImGui::EndDisabled();
    }
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
    configBaseline_.diagnosticsLoggingEnabled = cfg.diagnosticsLoggingEnabled;
    configBaseline_.masterVolume = cfg.masterVolume;
    configBaseline_.musicVolume = cfg.musicVolume;
    configBaseline_.sfxVolume = cfg.sfxVolume;
    configBaseline_.maxConcurrentSounds = static_cast<int>(cfg.maxConcurrentSounds);
    configBaseline_.searchPaths = cfg.searchPaths;
    configBaseline_.newSearchPath.clear();
    configBaseline_.preloadSounds = cfg.preloadSounds;
    configBaseline_.preloadMusic = cfg.preloadMusic;
    configBaseline_.newPreloadSound.clear();
    configBaseline_.newPreloadMusic.clear();
    configBaseline_.preloadSoundAliases = cfg.soundAliases;
    configBaseline_.preloadMusicAliases = cfg.musicAliases;
    configBaseline_.newPreloadSoundAlias.clear();
    configBaseline_.newPreloadMusicAlias.clear();

    configWorking_ = configBaseline_;
    pendingSoundPreloads_.clear();
    pendingMusicPreloads_.clear();
}

bool AudioManagerWindow::isConfigDirty() const {
    constexpr float kFloatTolerance = 1e-3f;
    if (configWorking_.enabled != configBaseline_.enabled) return true;
    if (configWorking_.diagnosticsLoggingEnabled != configBaseline_.diagnosticsLoggingEnabled) return true;
    if (std::abs(configWorking_.masterVolume - configBaseline_.masterVolume) > kFloatTolerance) return true;
    if (std::abs(configWorking_.musicVolume - configBaseline_.musicVolume) > kFloatTolerance) return true;
    if (std::abs(configWorking_.sfxVolume - configBaseline_.sfxVolume) > kFloatTolerance) return true;
    if (configWorking_.maxConcurrentSounds != configBaseline_.maxConcurrentSounds) return true;
    if (configWorking_.searchPaths != configBaseline_.searchPaths) return true;
    if (configWorking_.preloadSounds != configBaseline_.preloadSounds) return true;
    if (configWorking_.preloadMusic != configBaseline_.preloadMusic) return true;
    if (configWorking_.preloadSoundAliases != configBaseline_.preloadSoundAliases) return true;
    if (configWorking_.preloadMusicAliases != configBaseline_.preloadMusicAliases) return true;
    return false;
}

bool AudioManagerWindow::hasConfigDraft() const {
    if (isConfigDirty()) {
        return true;
    }
    if (!trimCopy(configWorking_.newSearchPath).empty()) {
        return true;
    }
    if (!trimCopy(configWorking_.newPreloadSound).empty()) {
        return true;
    }
    if (!trimCopy(configWorking_.newPreloadMusic).empty()) {
        return true;
    }
    if (!trimCopy(configWorking_.newPreloadSoundAlias).empty()) {
        return true;
    }
    if (!trimCopy(configWorking_.newPreloadMusicAlias).empty()) {
        return true;
    }
    return false;
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

    std::vector<std::string> sanitizedSoundPreloads;
    sanitizedSoundPreloads.reserve(configWorking_.preloadSounds.size());
    for (const auto& entry : configWorking_.preloadSounds) {
        ensurePreloadEntry(sanitizedSoundPreloads, entry);
    }

    std::vector<std::string> sanitizedMusicPreloads;
    sanitizedMusicPreloads.reserve(configWorking_.preloadMusic.size());
    for (const auto& entry : configWorking_.preloadMusic) {
        ensurePreloadEntry(sanitizedMusicPreloads, entry);
    }

    std::unordered_map<std::string, std::string> sanitizedSoundAliases;
    sanitizedSoundAliases.reserve(configWorking_.preloadSoundAliases.size());
    for (const auto& entry : sanitizedSoundPreloads) {
        const std::string canonical = canonicalizePreloadInput(entry);
        auto aliasIt = configWorking_.preloadSoundAliases.find(canonical);
        if (aliasIt != configWorking_.preloadSoundAliases.end()) {
            std::string trimmedAlias = trimCopy(aliasIt->second);
            if (!trimmedAlias.empty()) {
                sanitizedSoundAliases.emplace(canonical, trimmedAlias);
            }
        }
    }

    std::unordered_map<std::string, std::string> sanitizedMusicAliases;
    sanitizedMusicAliases.reserve(configWorking_.preloadMusicAliases.size());
    for (const auto& entry : sanitizedMusicPreloads) {
        const std::string canonical = canonicalizePreloadInput(entry);
        auto aliasIt = configWorking_.preloadMusicAliases.find(canonical);
        if (aliasIt != configWorking_.preloadMusicAliases.end()) {
            std::string trimmedAlias = trimCopy(aliasIt->second);
            if (!trimmedAlias.empty()) {
                sanitizedMusicAliases.emplace(canonical, trimmedAlias);
            }
        }
    }

    ConfigurationManager::set("audio::core::enabled", configWorking_.enabled);
    ConfigurationManager::set("audio::core::diagnostics_logging", configWorking_.diagnosticsLoggingEnabled);
    ConfigurationManager::set("audio::volumes::master", static_cast<double>(std::clamp(configWorking_.masterVolume, 0.0f, 1.0f)));
    ConfigurationManager::set("audio::volumes::music", static_cast<double>(std::clamp(configWorking_.musicVolume, 0.0f, 1.0f)));
    ConfigurationManager::set("audio::volumes::sfx", static_cast<double>(std::clamp(configWorking_.sfxVolume, 0.0f, 1.0f)));
    ConfigurationManager::set("audio::engine::max_concurrent_sounds", static_cast<int64_t>(std::max(configWorking_.maxConcurrentSounds, 0)));
    ConfigurationManager::set("audio::engine::search_paths", sanitizedPaths);
    ConfigurationManager::set("audio::preload::sounds", sanitizedSoundPreloads);
    ConfigurationManager::set("audio::preload::music", sanitizedMusicPreloads);
    nlohmann::json soundAliasJson = nlohmann::json::object();
    for (const auto& [canonical, alias] : sanitizedSoundAliases) {
        soundAliasJson[canonical] = alias;
    }
    nlohmann::json musicAliasJson = nlohmann::json::object();
    for (const auto& [canonical, alias] : sanitizedMusicAliases) {
        musicAliasJson[canonical] = alias;
    }
    ConfigurationManager::setJson("audio::preload::sound_aliases", soundAliasJson);
    ConfigurationManager::setJson("audio::preload::music_aliases", musicAliasJson);

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
    noteAppliedPreloads();

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
        configWorking_.newPreloadSound.clear();
        configWorking_.newPreloadMusic.clear();
        configStatusMessage_.clear();
        configStatusIsError_ = false;
        discardSessionPreloads();
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

void AudioManagerWindow::discardSessionPreloads() {
    for (const auto& key : sessionLoadedSoundKeys_) {
        audio::AudioManager::releaseSound(key);
    }
    sessionLoadedSoundKeys_.clear();

    for (const auto& key : sessionLoadedMusicKeys_) {
        audio::AudioManager::releaseMusic(key);
    }
    sessionLoadedMusicKeys_.clear();

    pendingSoundPreloads_.clear();
    pendingMusicPreloads_.clear();
}

void AudioManagerWindow::noteAppliedPreloads() {
    pendingSoundPreloads_.clear();
    pendingMusicPreloads_.clear();
    sessionLoadedSoundKeys_.clear();
    sessionLoadedMusicKeys_.clear();
}

bool AudioManagerWindow::ensurePreloadEntry(std::vector<std::string>& list, const std::string& value) {
    std::string trimmed = trimCopy(value);
    if (trimmed.empty()) {
        return false;
    }
    const std::string canonical = canonicalizePreloadInput(trimmed);
    auto it = std::find_if(list.begin(), list.end(), [&](const std::string& existing) {
        return canonicalizePreloadInput(existing) == canonical;
    });
    if (it != list.end()) {
        *it = trimmed;
        return false;
    }
    list.push_back(trimmed);
    return true;
}

std::string AudioManagerWindow::canonicalizePreloadInput(const std::string& value) {
    std::string trimmed = trimCopy(value);
    std::replace(trimmed.begin(), trimmed.end(), '\\', '/');
    std::transform(trimmed.begin(), trimmed.end(), trimmed.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return trimmed;
}

void AudioManagerWindow::noteLoadedSoundAsset(const std::string& canonicalKey,
                                              const std::string& identifierForConfig,
                                              std::optional<std::string> aliasUsed,
                                              bool newlyLoaded) {
    if (newlyLoaded) {
        if (std::find(sessionLoadedSoundKeys_.begin(), sessionLoadedSoundKeys_.end(), canonicalKey) ==
            sessionLoadedSoundKeys_.end()) {
            sessionLoadedSoundKeys_.push_back(canonicalKey);
        }
    } else {
        audio::AudioManager::releaseSound(canonicalKey);
    }

    if (ensurePreloadEntry(configWorking_.preloadSounds, identifierForConfig)) {
        configStatusMessage_.clear();
    }
    ensurePreloadEntry(pendingSoundPreloads_, identifierForConfig);

    if (aliasUsed && !aliasUsed->empty()) {
        configStatusMessage_ = "Sound alias '" + *aliasUsed + "' loaded from '" + identifierForConfig + "'. Apply changes to persist.";
    } else {
        configStatusMessage_ = "Sound '" + identifierForConfig + "' loaded. Apply changes to persist.";
    }
    configStatusIsError_ = false;
    inventoryDirty_ = true;
}

void AudioManagerWindow::noteLoadedMusicAsset(const std::string& canonicalKey,
                                              const std::string& identifierForConfig,
                                              std::optional<std::string> aliasUsed,
                                              bool newlyLoaded) {
    if (newlyLoaded) {
        if (std::find(sessionLoadedMusicKeys_.begin(), sessionLoadedMusicKeys_.end(), canonicalKey) ==
            sessionLoadedMusicKeys_.end()) {
            sessionLoadedMusicKeys_.push_back(canonicalKey);
        }
    } else {
        audio::AudioManager::releaseMusic(canonicalKey);
    }

    if (ensurePreloadEntry(configWorking_.preloadMusic, identifierForConfig)) {
        configStatusMessage_.clear();
    }
    ensurePreloadEntry(pendingMusicPreloads_, identifierForConfig);

    if (aliasUsed && !aliasUsed->empty()) {
        configStatusMessage_ = "Music alias '" + *aliasUsed + "' loaded from '" + identifierForConfig + "'. Apply changes to persist.";
    } else {
        configStatusMessage_ = "Music '" + identifierForConfig + "' loaded. Apply changes to persist.";
    }
    configStatusIsError_ = false;
    inventoryDirty_ = true;
}

std::string AudioManagerWindow::determineBrowseDirectory() const {
    if (!lastLoadDirectory_.empty()) {
        return lastLoadDirectory_;
    }

    auto resolvePath = [](const std::string& path) -> std::string {
        if (path.empty()) {
            return {};
        }
        std::filesystem::path fsPath{path};
        std::error_code ec;
        if (!fsPath.is_absolute()) {
            auto cwd = std::filesystem::current_path(ec);
            if (!ec) {
                fsPath = cwd / fsPath;
            }
        }
        auto canonical = std::filesystem::weakly_canonical(fsPath, ec);
        if (!ec) {
            return canonical.string();
        }
        ec.clear();
        return fsPath.lexically_normal().string();
    };

    for (const auto& search : configWorking_.searchPaths) {
        std::string candidate = resolvePath(search);
        if (!candidate.empty()) {
            return candidate;
        }
    }
    for (const auto& search : configBaseline_.searchPaths) {
        std::string candidate = resolvePath(search);
        if (!candidate.empty()) {
            return candidate;
        }
    }
    return {};
}

std::string AudioManagerWindow::makeIdentifierFromSelection(const std::filesystem::path& absolutePath) const {
    auto normalizedSelection = [&]() {
        std::error_code ec;
        auto canonical = std::filesystem::weakly_canonical(absolutePath, ec);
        if (!ec) {
            return canonical;
        }
        ec.clear();
        return absolutePath.lexically_normal();
    }();

    auto attemptRelative = [&](const std::string& base) -> std::optional<std::string> {
        if (base.empty()) {
            return std::nullopt;
        }
        std::filesystem::path basePath{base};
        std::error_code ec;
        if (!basePath.is_absolute()) {
            auto cwd = std::filesystem::current_path(ec);
            if (ec) {
                return std::nullopt;
            }
            basePath = cwd / basePath;
        }
        auto canonicalBase = std::filesystem::weakly_canonical(basePath, ec);
        if (!ec) {
            basePath = canonicalBase;
        } else {
            ec.clear();
            basePath = basePath.lexically_normal();
        }
        auto relative = std::filesystem::relative(normalizedSelection, basePath, ec);
        if (ec) {
            return std::nullopt;
        }
        auto relString = relative.generic_string();
        if (relString.empty()) {
            return std::nullopt;
        }
        if (relString.find("..") != std::string::npos) {
            return std::nullopt;
        }
        return relString;
    };

    for (const auto& search : configWorking_.searchPaths) {
        if (auto rel = attemptRelative(search)) {
            return *rel;
        }
    }
    for (const auto& search : configBaseline_.searchPaths) {
        if (auto rel = attemptRelative(search)) {
            return *rel;
        }
    }

    std::error_code ec;
    auto cwd = std::filesystem::current_path(ec);
    if (!ec) {
        if (auto rel = attemptRelative(cwd.string())) {
            return *rel;
        }
    }

    return normalizedSelection.generic_string();
}

void AudioManagerWindow::handleEvent(const audio::AudioEvent& event) {
    const bool diagnosticsLoggingEnabled = configBaseline_.diagnosticsLoggingEnabled;
    if (diagnosticsLoggingEnabled) {
        gb2d::logging::LogManager::info("AudioManagerWindow::handleEvent called for event type {}", static_cast<int>(event.type));
        EventLogEntry entry{event, ""};

        auto time_t = std::chrono::system_clock::from_time_t(event.timestampMs / 1000);
        auto time = std::chrono::system_clock::to_time_t(time_t);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%H:%M:%S");
        entry.formattedTime = ss.str();

        eventLog_.push_back(std::move(entry));

        if (eventLog_.size() > maxEventLogSize_) {
            eventLog_.erase(eventLog_.begin());
        }
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
            if (diagnosticsLoggingEnabled) {
                gb2d::logging::LogManager::info("Sound preview finished for '{}'", event.key);
            }
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
            if (diagnosticsLoggingEnabled) {
                gb2d::logging::LogManager::info("Music preview stopped for '{}'", event.key);
            }
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