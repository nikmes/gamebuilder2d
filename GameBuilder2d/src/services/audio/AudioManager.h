#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "raylib.h"

namespace gb2d::audio {

struct AudioConfig {
    bool enabled{true};
    bool diagnosticsLoggingEnabled{true};
    float masterVolume{1.0f};
    float musicVolume{1.0f};
    float sfxVolume{1.0f};
    std::size_t maxConcurrentSounds{16};
    std::vector<std::string> searchPaths{};
    std::vector<std::string> preloadSounds{};
    std::vector<std::string> preloadMusic{};
    std::unordered_map<std::string, std::string> soundAliases{};
    std::unordered_map<std::string, std::string> musicAliases{};
};

struct AudioMetrics {
    bool initialized{false};
    bool deviceReady{false};
    bool silentMode{false};
    std::size_t loadedSounds{0};
    std::size_t loadedMusic{0};
    std::size_t activeSoundInstances{0};
    std::size_t maxSoundSlots{0};
};

struct AcquireSoundResult {
    std::string key;
    const Sound* sound{nullptr};
    bool placeholder{false};
    bool newlyLoaded{false};
};

struct AcquireMusicResult {
    std::string key;
    const Music* music{nullptr};
    bool placeholder{false};
    bool newlyLoaded{false};
};

struct PlaybackParams {
    float volume{1.0f};
    float pitch{1.0f};
    float pan{0.5f}; // 0.0 = left, 0.5 = center, 1.0 = right
};

struct SoundInventoryRecord {
    std::string key;
    std::string path;
    float durationSeconds{0.0f};
    std::size_t refCount{0};
    bool placeholder{false};
    std::uint32_t sampleRate{0};
    std::uint32_t channels{0};
};

struct MusicInventoryRecord {
    std::string key;
    std::string path;
    float durationSeconds{0.0f};
    std::size_t refCount{0};
    bool placeholder{false};
    std::uint32_t sampleRate{0};
    std::uint32_t channels{0};
};

struct MusicPlaybackStatus {
    bool valid{false};
    bool playing{false};
    bool paused{false};
    float positionSeconds{0.0f};
    float durationSeconds{0.0f};
};

enum class AudioEventType {
    SoundLoaded,
    SoundUnloaded,
    MusicLoaded,
    MusicUnloaded,
    SoundPlaybackStarted,
    SoundPlaybackStopped,
    MusicPlaybackStarted,
    MusicPlaybackPaused,
    MusicPlaybackResumed,
    MusicPlaybackStopped,
    PreviewStarted,
    PreviewStopped,
    ConfigChanged,
    DeviceError
};

struct AudioEvent {
    AudioEventType type;
    std::string key; // asset key, empty for global events
    std::uint64_t timestampMs;
    std::string details; // additional info
};

class AudioEventSink {
public:
    virtual ~AudioEventSink() = default;
    virtual void onAudioEvent(const AudioEvent& event) = 0;
};

struct AudioEventSubscription {
    std::uint32_t id{0};
    AudioEventSink* sink{nullptr};
    bool active{false};
};

struct PlaybackHandle {
    int slot{-1};
    std::uint32_t generation{0};
    bool valid() const { return slot >= 0; }
};

class AudioManager {
public:
    static bool init();
    static void shutdown();
    static bool isInitialized();
    static bool isDeviceReady();
    static void tick(float deltaSeconds = 0.0f);

    static AudioConfig config();

    static AcquireSoundResult acquireSound(const std::string& identifier,
                                           std::optional<std::string> alias = std::nullopt);
    static const Sound* tryGetSound(const std::string& key);
    static bool releaseSound(const std::string& key);

    static AcquireMusicResult acquireMusic(const std::string& identifier,
                                           std::optional<std::string> alias = std::nullopt);
    static const Music* tryGetMusic(const std::string& key);
    static bool releaseMusic(const std::string& key);

    static PlaybackHandle playSound(const std::string& key, const PlaybackParams& params = {});
    static bool stopSound(PlaybackHandle handle);
    static bool stopAllSounds();
    static bool isHandleActive(PlaybackHandle handle);
    static bool updateSoundPlayback(PlaybackHandle handle, const PlaybackParams& params);

    static bool playMusic(const std::string& key);
    static bool pauseMusic(const std::string& key);
    static bool resumeMusic(const std::string& key);
    static bool stopMusic(const std::string& key);
    static bool setMusicVolume(const std::string& key, float volume);
    static bool seekMusic(const std::string& key, float positionSeconds);
    static MusicPlaybackStatus musicPlaybackStatus(const std::string& key);

    static bool reloadAll();
    static AudioMetrics metrics();

    // Inventory and event APIs for AudioManagerWindow
    static std::vector<SoundInventoryRecord> captureSoundInventorySnapshot();
    static std::vector<MusicInventoryRecord> captureMusicInventorySnapshot();
    static AudioEventSubscription subscribeToAudioEvents(AudioEventSink* sink);
    static bool unsubscribeFromAudioEvents(AudioEventSubscription& subscription);
    static std::size_t activeSubscriptionCountForTesting();

    struct Backend {
        virtual ~Backend() = default;
        virtual void initDevice() = 0;
        virtual void closeDevice() = 0;
        virtual bool isDeviceReady() = 0;
        virtual void setMasterVolume(float volume) = 0;
    };

    struct RaylibHooks {
        Sound (*loadSound)(const char* path);
        void (*unloadSound)(Sound sound);
        Sound (*loadSoundAlias)(Sound sound);
        void (*unloadSoundAlias)(Sound sound);
        void (*playSound)(Sound sound);
        void (*stopSound)(Sound sound);
        bool (*isSoundPlaying)(Sound sound);
        void (*setSoundVolume)(Sound sound, float volume);
        void (*setSoundPitch)(Sound sound, float pitch);
        void (*setSoundPan)(Sound sound, float pan);
        Music (*loadMusicStream)(const char* path);
        void (*unloadMusicStream)(Music music);
        void (*playMusicStream)(Music music);
        void (*pauseMusicStream)(Music music);
        void (*resumeMusicStream)(Music music);
        void (*stopMusicStream)(Music music);
        void (*updateMusicStream)(Music music);
        bool (*isMusicStreamPlaying)(Music music);
        void (*setMusicVolume)(Music music, float volume);
        void (*seekMusicStream)(Music music, float positionSeconds);
        float (*getMusicTimeLength)(Music music);
        float (*getMusicTimePlayed)(Music music);
    };

    static void setBackendForTesting(Backend* backend);
    static void setRaylibHooksForTesting(const RaylibHooks* hooks);
    static void resetForTesting();
};

} // namespace gb2d::audio
