#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "services/audio/AudioManager.h"
#include "services/configuration/ConfigurationManager.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Forward declaration from raylib internals so test doubles can tag buffers.
struct rAudioBuffer;

using gb2d::ConfigurationManager;
using gb2d::audio::AudioManager;
using gb2d::audio::PlaybackHandle;
using gb2d::audio::PlaybackParams;

namespace {

struct StubBackend final : AudioManager::Backend {
    bool ready{true};
    bool initCalled{false};
    bool closeCalled{false};
    float masterVolume{1.0f};

    void initDevice() override { initCalled = true; }
    void closeDevice() override { closeCalled = true; }
    bool isDeviceReady() override { return ready; }
    void setMasterVolume(float volume) override { masterVolume = volume; }
};

class StubRaylib {
public:
    struct SoundInfo {
        bool isAlias{false};
        bool playing{false};
        float volume{1.0f};
        float pitch{1.0f};
        float pan{0.0f};
    };

    struct MusicInfo {
        bool playing{false};
        bool paused{false};
        float volume{1.0f};
        float length{120.0f};
        float position{0.0f};
    };

    static void reset() {
        std::lock_guard lock(mutex());
        nextId() = 1;
        sounds().clear();
        music().clear();
    }

    static std::size_t activeSoundCount() {
        std::lock_guard lock(mutex());
        std::size_t count = 0;
        for (auto& [id, info] : sounds()) {
            (void)id;
            if (info.playing) {
                ++count;
            }
        }
        return count;
    }

    static void setAllSoundsPlaying(bool playing) {
        std::lock_guard lock(mutex());
        for (auto& [id, info] : sounds()) {
            (void)id;
            info.playing = playing;
        }
    }

    static MusicInfo getMusicInfo(const Music& music) {
        std::lock_guard lock(mutex());
        auto id = idForMusic(music);
        auto it = StubRaylib::music().find(id);
        if (it != StubRaylib::music().end()) {
            return it->second;
        }
        return {};
    }

    static float volumeForSound(const Sound& sound) {
        std::lock_guard lock(mutex());
        auto it = sounds().find(idForSound(sound));
        return it != sounds().end() ? it->second.volume : 0.0f;
    }

    static float volumeForMusic(const Music& music) {
        std::lock_guard lock(mutex());
        auto it = StubRaylib::music().find(idForMusic(music));
        return it != StubRaylib::music().end() ? it->second.volume : 0.0f;
    }

    static const AudioManager::RaylibHooks& hooks() {
        static const AudioManager::RaylibHooks api{
            loadSound,
            unloadSound,
            loadSoundAlias,
            unloadSoundAlias,
            playSound,
            stopSound,
            isSoundPlaying,
            setSoundVolume,
            setSoundPitch,
            setSoundPan,
            loadMusicStream,
            unloadMusicStream,
            playMusicStream,
            pauseMusicStream,
            resumeMusicStream,
            stopMusicStream,
            updateMusicStream,
            isMusicStreamPlaying,
            setMusicVolume,
            seekMusicStream,
            getMusicTimeLength,
            getMusicTimePlayed};
        return api;
    }

private:
    static Sound makeSound(bool alias) {
        Sound sound{};
        sound.frameCount = 1;
        sound.stream.sampleRate = 44100;
        sound.stream.sampleSize = 16;
        sound.stream.channels = 2;
        const auto id = nextId()++;
        sound.stream.buffer = reinterpret_cast<rAudioBuffer*>(static_cast<uintptr_t>(id));
        sounds()[id] = SoundInfo{alias, false, 1.0f, 1.0f, 0.0f};
        return sound;
    }

    static Music makeMusic() {
        Music music{};
        music.frameCount = 1;
        music.stream.sampleRate = 44100;
        music.stream.sampleSize = 16;
        music.stream.channels = 2;
        const auto id = nextId()++;
        music.stream.buffer = reinterpret_cast<rAudioBuffer*>(static_cast<uintptr_t>(id));
        StubRaylib::music()[id] = MusicInfo{};
        return music;
    }

    static Sound loadSound(const char*) {
        std::lock_guard lock(mutex());
        return makeSound(false);
    }

    static void unloadSound(Sound sound) {
        std::lock_guard lock(mutex());
        sounds().erase(idForSound(sound));
    }

    static Sound loadSoundAlias(Sound) {
        std::lock_guard lock(mutex());
        return makeSound(true);
    }

    static void unloadSoundAlias(Sound sound) {
        std::lock_guard lock(mutex());
        sounds().erase(idForSound(sound));
    }

    static void playSound(Sound sound) {
        std::lock_guard lock(mutex());
        sounds()[idForSound(sound)].playing = true;
    }

    static void stopSound(Sound sound) {
        std::lock_guard lock(mutex());
        sounds()[idForSound(sound)].playing = false;
    }

    static bool isSoundPlaying(Sound sound) {
        std::lock_guard lock(mutex());
        auto it = sounds().find(idForSound(sound));
        return it != sounds().end() && it->second.playing;
    }

    static void setSoundVolume(Sound sound, float volume) {
        std::lock_guard lock(mutex());
        sounds()[idForSound(sound)].volume = volume;
    }

    static void setSoundPitch(Sound sound, float pitch) {
        std::lock_guard lock(mutex());
        sounds()[idForSound(sound)].pitch = pitch;
    }

    static void setSoundPan(Sound sound, float pan) {
        std::lock_guard lock(mutex());
        sounds()[idForSound(sound)].pan = pan;
    }

    static Music loadMusicStream(const char*) {
        std::lock_guard lock(mutex());
        return makeMusic();
    }

    static void unloadMusicStream(Music music) {
        std::lock_guard lock(mutex());
        StubRaylib::music().erase(idForMusic(music));
    }

    static void playMusicStream(Music music) {
        std::lock_guard lock(mutex());
        auto& info = StubRaylib::music()[idForMusic(music)];
        info.playing = true;
        info.paused = false;
        info.position = 0.0f;
    }

    static void pauseMusicStream(Music music) {
        std::lock_guard lock(mutex());
        StubRaylib::music()[idForMusic(music)].paused = true;
    }

    static void resumeMusicStream(Music music) {
        std::lock_guard lock(mutex());
        auto& info = StubRaylib::music()[idForMusic(music)];
        info.paused = false;
        info.playing = true;
    }

    static void stopMusicStream(Music music) {
        std::lock_guard lock(mutex());
        StubRaylib::music()[idForMusic(music)].playing = false;
        StubRaylib::music()[idForMusic(music)].paused = false;
        StubRaylib::music()[idForMusic(music)].position = 0.0f;
    }

    static void updateMusicStream(Music music) {
        std::lock_guard lock(mutex());
        auto& info = StubRaylib::music()[idForMusic(music)];
        if (info.playing && !info.paused) {
            info.position = std::min(info.position + 1.0f, info.length);
            if (info.position >= info.length) {
                info.playing = false; // auto-finish to exercise completion path
            }
        }
    }

    static bool isMusicStreamPlaying(Music music) {
        std::lock_guard lock(mutex());
        auto it = StubRaylib::music().find(idForMusic(music));
        if (it == StubRaylib::music().end()) {
            return false;
        }
        return it->second.playing && !it->second.paused;
    }

    static void setMusicVolume(Music music, float volume) {
        std::lock_guard lock(mutex());
        StubRaylib::music()[idForMusic(music)].volume = volume;
    }

    static void seekMusicStream(Music music, float positionSeconds) {
        std::lock_guard lock(mutex());
        auto& info = StubRaylib::music()[idForMusic(music)];
        info.position = std::clamp(positionSeconds, 0.0f, info.length);
    }

    static float getMusicTimeLength(Music music) {
        std::lock_guard lock(mutex());
        return StubRaylib::music()[idForMusic(music)].length;
    }

    static float getMusicTimePlayed(Music music) {
        std::lock_guard lock(mutex());
        return StubRaylib::music()[idForMusic(music)].position;
    }

    static std::unordered_map<std::uintptr_t, SoundInfo>& sounds() {
        static std::unordered_map<std::uintptr_t, SoundInfo> map;
        return map;
    }

    static std::unordered_map<std::uintptr_t, MusicInfo>& music() {
        static std::unordered_map<std::uintptr_t, MusicInfo> map;
        return map;
    }

    static std::uintptr_t& nextId() {
        static std::uintptr_t value = 1;
        return value;
    }

    static std::mutex& mutex() {
        static std::mutex m;
        return m;
    }

    static std::uintptr_t idForSound(const Sound& sound) {
        return reinterpret_cast<std::uintptr_t>(sound.stream.buffer);
    }

    static std::uintptr_t idForMusic(const Music& music) {
        return reinterpret_cast<std::uintptr_t>(music.stream.buffer);
    }
};

struct AudioTestFixture {
    AudioTestFixture() {
        StubRaylib::reset();
        AudioManager::resetForTesting();
        ConfigurationManager::loadOrDefault();

        tempDir = std::filesystem::temp_directory_path() /
                  (std::string("gb2d-audio-tests-") + std::to_string(++suiteCounter));
        std::filesystem::create_directories(tempDir);
        soundPath = tempDir / "blip.wav";
        musicPath = tempDir / "loop.ogg";
        std::ofstream(soundPath).put('\0');
        std::ofstream(musicPath).put('\0');

        ConfigurationManager::set("audio::core::enabled", true);
        ConfigurationManager::set("audio::volumes::master", 0.75);
        ConfigurationManager::set("audio::volumes::music", 0.5);
        ConfigurationManager::set("audio::volumes::sfx", 0.8);
        ConfigurationManager::set("audio::engine::max_concurrent_sounds", static_cast<int64_t>(2));
        ConfigurationManager::set(
            "audio::engine::search_paths",
            std::vector<std::string>{tempDir.string()});
        ConfigurationManager::set("audio::preload::sounds", std::vector<std::string>{});
        ConfigurationManager::set("audio::preload::music", std::vector<std::string>{});

        backend.ready = true;
        AudioManager::setBackendForTesting(&backend);
        AudioManager::setRaylibHooksForTesting(&StubRaylib::hooks());

        REQUIRE(AudioManager::init());
    }

    ~AudioTestFixture() {
        AudioManager::shutdown();
        AudioManager::resetForTesting();
        StubRaylib::reset();
        std::error_code ec;
        std::filesystem::remove_all(tempDir, ec);
    }

    StubBackend backend;
    std::filesystem::path tempDir;
    std::filesystem::path soundPath;
    std::filesystem::path musicPath;
    inline static int suiteCounter = 0;
};

} // namespace

TEST_CASE_METHOD(AudioTestFixture, "AudioManager throttles when sound slots are exhausted", "[audio][sound][slots]") {
    auto sound = AudioManager::acquireSound("blip.wav");
    REQUIRE_FALSE(sound.placeholder);

    auto handle1 = AudioManager::playSound(sound.key, PlaybackParams{0.6f, 1.0f, 0.5f});
    REQUIRE(handle1.valid());
    auto handle2 = AudioManager::playSound(sound.key, PlaybackParams{0.4f, 1.0f, 0.5f});
    REQUIRE(handle2.valid());
    auto handle3 = AudioManager::playSound(sound.key);
    REQUIRE_FALSE(handle3.valid());

    REQUIRE(StubRaylib::activeSoundCount() == 2);
    auto metrics = AudioManager::metrics();
    REQUIRE(metrics.activeSoundInstances == 2);

    StubRaylib::setAllSoundsPlaying(false);
    AudioManager::tick();

    metrics = AudioManager::metrics();
    REQUIRE(metrics.activeSoundInstances == 0);
    REQUIRE(StubRaylib::activeSoundCount() == 0);
}

TEST_CASE_METHOD(AudioTestFixture, "AudioManager stopSound invalidates stale handles", "[audio][sound]") {
    auto sound = AudioManager::acquireSound("blip.wav");
    REQUIRE_FALSE(sound.placeholder);

    auto handle = AudioManager::playSound(sound.key);
    REQUIRE(handle.valid());
    REQUIRE(AudioManager::stopSound(handle));
    REQUIRE_FALSE(AudioManager::stopSound(handle));
}

TEST_CASE_METHOD(AudioTestFixture, "AudioManager music controls propagate to hooks", "[audio][music]") {
    auto music = AudioManager::acquireMusic("loop.ogg");
    REQUIRE_FALSE(music.placeholder);
    REQUIRE(music.music != nullptr);

    REQUIRE(AudioManager::playMusic(music.key));
    auto info = StubRaylib::getMusicInfo(*music.music);
    REQUIRE(info.playing);
    REQUIRE_FALSE(info.paused);
    REQUIRE(StubRaylib::volumeForMusic(*music.music) == Catch::Approx(0.5f));

    REQUIRE(AudioManager::setMusicVolume(music.key, 0.4f));
    REQUIRE(StubRaylib::volumeForMusic(*music.music) == Catch::Approx(0.4f * 0.5f).margin(1e-6f));

    REQUIRE(AudioManager::pauseMusic(music.key));
    info = StubRaylib::getMusicInfo(*music.music);
    REQUIRE(info.paused);

    REQUIRE(AudioManager::resumeMusic(music.key));
    info = StubRaylib::getMusicInfo(*music.music);
    REQUIRE(info.playing);
    REQUIRE_FALSE(info.paused);

    REQUIRE(AudioManager::stopMusic(music.key));
    info = StubRaylib::getMusicInfo(*music.music);
    REQUIRE_FALSE(info.playing);
}

TEST_CASE_METHOD(AudioTestFixture, "AudioManager reports music playback status", "[audio][music][status]") {
    auto music = AudioManager::acquireMusic("loop.ogg");
    REQUIRE_FALSE(music.placeholder);

    REQUIRE(AudioManager::playMusic(music.key));

    auto status = AudioManager::musicPlaybackStatus(music.key);
    REQUIRE(status.valid);
    REQUIRE(status.playing);
    REQUIRE_FALSE(status.paused);
    REQUIRE(status.positionSeconds == Catch::Approx(0.0f));
    REQUIRE(status.durationSeconds == Catch::Approx(120.0f));

    AudioManager::tick();
    status = AudioManager::musicPlaybackStatus(music.key);
    REQUIRE(status.positionSeconds == Catch::Approx(1.0f));
    REQUIRE(status.durationSeconds == Catch::Approx(120.0f));

    REQUIRE(AudioManager::seekMusic(music.key, 42.5f));
    status = AudioManager::musicPlaybackStatus(music.key);
    REQUIRE(status.positionSeconds == Catch::Approx(42.5f).margin(1e-3f));
    REQUIRE(status.durationSeconds == Catch::Approx(120.0f));

    REQUIRE(AudioManager::pauseMusic(music.key));
    status = AudioManager::musicPlaybackStatus(music.key);
    REQUIRE(status.paused);
    REQUIRE(status.playing);

    REQUIRE(AudioManager::resumeMusic(music.key));
    status = AudioManager::musicPlaybackStatus(music.key);
    REQUIRE_FALSE(status.paused);
    REQUIRE(status.playing);

    REQUIRE(AudioManager::stopMusic(music.key));
    status = AudioManager::musicPlaybackStatus(music.key);
    REQUIRE(status.valid);
    REQUIRE_FALSE(status.playing);
    REQUIRE_FALSE(status.paused);
    REQUIRE(status.positionSeconds == Catch::Approx(0.0f));
}
