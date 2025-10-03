#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "raylib.h"

namespace gb2d::audio {

struct AudioConfig {
    bool enabled{true};
    float masterVolume{1.0f};
    float musicVolume{1.0f};
    float sfxVolume{1.0f};
    std::size_t maxConcurrentSounds{16};
    std::vector<std::string> searchPaths{};
    std::vector<std::string> preloadSounds{};
    std::vector<std::string> preloadMusic{};
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

    static bool playMusic(const std::string& key);
    static bool pauseMusic(const std::string& key);
    static bool resumeMusic(const std::string& key);
    static bool stopMusic(const std::string& key);
    static bool setMusicVolume(const std::string& key, float volume);
    static bool seekMusic(const std::string& key, float positionSeconds);

    static bool reloadAll();
    static AudioMetrics metrics();

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
    };

    static void setBackendForTesting(Backend* backend);
    static void setRaylibHooksForTesting(const RaylibHooks* hooks);
    static void resetForTesting();
};

} // namespace gb2d::audio
