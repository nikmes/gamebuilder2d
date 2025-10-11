#pragma once

#include "ui/Window.h"
#include "services/audio/AudioManager.h"

#include <nlohmann/json.hpp>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>
#include <string>
#include <filesystem>

namespace gb2d {

class AudioManagerWindowTestAccess;

class AudioManagerWindow : public IWindow, public audio::AudioEventSink {
public:
    AudioManagerWindow();
    ~AudioManagerWindow();

    const char* typeId() const override { return "audio_manager"; }
    const char* displayName() const override { return "Audio Manager"; }

    std::string title() const override { return title_; }
    void setTitle(std::string title) override { title_ = std::move(title); }

    void render(WindowContext& ctx) override;
    bool handleCloseRequest(WindowContext& ctx) override;
    void serialize(nlohmann::json& out) const override;
    void deserialize(const nlohmann::json& in) override;

    // AudioEventSink implementation
    void onAudioEvent(const audio::AudioEvent& event) override;

private:
    void renderAssetList();
    void renderLoadAssetControls();
    void renderPreviewPanel();
    void renderConfigPanel();
    void renderDiagnosticsPanel();
    void renderClosePromptModal();
    void processPendingCloseAction();
    
    void refreshInventorySnapshots();
    void handleEvent(const audio::AudioEvent& event);
    void refreshConfigState();
    bool isConfigDirty() const;
    bool hasConfigDraft() const;
    bool applyConfigChanges();
    void openClosePrompt();
    void finalizeClose();

    void discardSessionPreloads();
    void noteAppliedPreloads();
    void noteLoadedSoundAsset(const std::string& canonicalKey,
                              const std::string& identifierForConfig,
                              std::optional<std::string> aliasUsed,
                              bool newlyLoaded);
    void noteLoadedMusicAsset(const std::string& canonicalKey,
                              const std::string& identifierForConfig,
                              std::optional<std::string> aliasUsed,
                              bool newlyLoaded);
    bool ensurePreloadEntry(std::vector<std::string>& list, const std::string& value);
    static std::string canonicalizePreloadInput(const std::string& value);
    void setPreloadAlias(std::unordered_map<std::string, std::string>& aliases,
                         const std::string& identifier,
                         std::optional<std::string> aliasValue);
    std::string makeIdentifierFromSelection(const std::filesystem::path& absolutePath) const;
    std::string determineBrowseDirectory() const;
    
    void startSoundPreview(const std::string& key);
    void startMusicPreview(const std::string& key);
    void stopPreview();
    void reportPreviewStatus(std::string message, bool isError);
    void clearPreviewStatus();

    std::string title_{"Audio Manager"};
    
    // Inventory data
    std::vector<audio::SoundInventoryRecord> soundInventory_;
    std::vector<audio::MusicInventoryRecord> musicInventory_;
    bool inventoryDirty_{true};
    
    // Event subscription
    audio::AudioEventSubscription eventSubscription_;
    
    // Event log for diagnostics
    struct EventLogEntry {
        audio::AudioEvent event;
        std::string formattedTime;
    };
    std::vector<EventLogEntry> eventLog_;
    std::size_t maxEventLogSize_{100};
    
    // Preview state
    enum class PreviewType { None, Sound, Music };
    PreviewType previewType_{PreviewType::None};
    std::string previewKey_;
    std::string selectedAssetKey_;
    bool isPlayingPreview_{false};
    float previewVolume_{1.0f};
    float previewPan_{0.5f};
    float previewPitch_{1.0f};
    audio::PlaybackHandle previewSoundHandle_{};
    std::string previewStatusMessage_{};
    bool previewStatusIsError_{false};

    struct LoadAssetFormState {
        std::string identifier;
        std::string alias;
        std::string statusMessage;
        bool statusIsError{false};
        bool statusIsWarning{false};
    };

    LoadAssetFormState soundLoadForm_{};
    LoadAssetFormState musicLoadForm_{};
    std::string soundLoadDialogId_{};
    std::string musicLoadDialogId_{};
    std::string lastLoadDirectory_{};

    struct ConfigPanelState {
        bool enabled{true};
        bool diagnosticsLoggingEnabled{true};
        float masterVolume{1.0f};
        float musicVolume{1.0f};
        float sfxVolume{1.0f};
        int maxConcurrentSounds{16};
        std::vector<std::string> searchPaths{};
        std::string newSearchPath;
        std::vector<std::string> preloadSounds{};
        std::vector<std::string> preloadMusic{};
        std::string newPreloadSound;
        std::string newPreloadMusic;
        std::unordered_map<std::string, std::string> preloadSoundAliases{};
        std::unordered_map<std::string, std::string> preloadMusicAliases{};
        std::string newPreloadSoundAlias;
        std::string newPreloadMusicAlias;
    };

    ConfigPanelState configBaseline_{};
    ConfigPanelState configWorking_{};
    std::string configStatusMessage_;
    bool configStatusIsError_{false};

    std::vector<std::string> pendingSoundPreloads_{};
    std::vector<std::string> pendingMusicPreloads_{};
    std::vector<std::string> sessionLoadedSoundKeys_{};
    std::vector<std::string> sessionLoadedMusicKeys_{};

    enum class ClosePrompt { None, UnsavedChanges };
    enum class PendingCloseAction { None, ApplyAndClose, DiscardAndClose };

    ClosePrompt closePrompt_{ClosePrompt::None};
    PendingCloseAction pendingCloseAction_{PendingCloseAction::None};
    std::function<void()> requestCloseCallback_{};

    friend class AudioManagerWindowTestAccess;
};

} // namespace gb2d