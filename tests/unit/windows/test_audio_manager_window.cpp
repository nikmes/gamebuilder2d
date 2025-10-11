#include <catch2/catch_test_macros.hpp>

#include "services/audio/AudioManager.h"
#include "services/configuration/ConfigurationManager.h"
#include "ui/Windows/AudioManagerWindow.h"

#include <cstdint>
#include <string>
#include <utility>
#include <algorithm>
#include <vector>
#include <cctype>
#include <optional>

using namespace gb2d;

namespace gb2d {

struct AudioManagerWindowTestAccess {
    static audio::AudioEventSubscription& subscription(AudioManagerWindow& window) {
        return window.eventSubscription_;
    }

    static bool inventoryDirty(const AudioManagerWindow& window) {
        return window.inventoryDirty_;
    }

    static void refreshInventory(AudioManagerWindow& window) {
        window.refreshInventorySnapshots();
    }

    static void dispatchEvent(AudioManagerWindow& window, const audio::AudioEvent& event) {
        window.handleEvent(event);
    }

    static const std::vector<AudioManagerWindow::EventLogEntry>& eventLog(const AudioManagerWindow& window) {
        return window.eventLog_;
    }

    static std::size_t maxLogSize(const AudioManagerWindow& window) {
        return window.maxEventLogSize_;
    }

    static const std::vector<std::string>& preloadSounds(const AudioManagerWindow& window) {
        return window.configWorking_.preloadSounds;
    }

    static const std::vector<std::string>& preloadMusic(const AudioManagerWindow& window) {
        return window.configWorking_.preloadMusic;
    }

    static const std::vector<std::string>& pendingSoundPreloads(const AudioManagerWindow& window) {
        return window.pendingSoundPreloads_;
    }

    static const std::vector<std::string>& pendingMusicPreloads(const AudioManagerWindow& window) {
        return window.pendingMusicPreloads_;
    }

    static const std::vector<std::string>& sessionLoadedSoundKeys(const AudioManagerWindow& window) {
        return window.sessionLoadedSoundKeys_;
    }

    static const std::vector<std::string>& sessionLoadedMusicKeys(const AudioManagerWindow& window) {
        return window.sessionLoadedMusicKeys_;
    }

    static void setPreloadDrafts(AudioManagerWindow& window,
                                 std::vector<std::string> sounds,
                                 std::vector<std::string> music) {
        window.configWorking_.preloadSounds = std::move(sounds);
        window.configWorking_.preloadMusic = std::move(music);
    }

    static bool applyConfig(AudioManagerWindow& window) {
        return window.applyConfigChanges();
    }

    static bool isConfigDirty(const AudioManagerWindow& window) {
        return window.isConfigDirty();
    }

    static void noteSound(AudioManagerWindow& window,
                          const std::string& canonicalKey,
                          const std::string& identifier,
                          std::optional<std::string> alias,
                          bool newlyLoaded) {
        window.noteLoadedSoundAsset(canonicalKey, identifier, std::move(alias), newlyLoaded);
    }

    static void noteMusic(AudioManagerWindow& window,
                          const std::string& canonicalKey,
                          const std::string& identifier,
                          std::optional<std::string> alias,
                          bool newlyLoaded) {
        window.noteLoadedMusicAsset(canonicalKey, identifier, std::move(alias), newlyLoaded);
    }
};

} // namespace gb2d

namespace {

struct DummyBackend : audio::AudioManager::Backend {
    void initDevice() override { ready = true; }
    void closeDevice() override { ready = false; }
    bool isDeviceReady() override { return ready; }
    void setMasterVolume(float) override {}

    bool ready{true};
};

struct DummyRaylib {
    static Sound loadSound(const char*) { return Sound{}; }
    static void unloadSound(Sound) {}
    static Sound loadSoundAlias(Sound sound) { return sound; }
    static void unloadSoundAlias(Sound) {}
    static void playSound(Sound) {}
    static void stopSound(Sound) {}
    static bool isSoundPlaying(Sound) { return false; }
    static void setSoundVolume(Sound, float) {}
    static void setSoundPitch(Sound, float) {}
    static void setSoundPan(Sound, float) {}
    static Music loadMusicStream(const char*) { return Music{}; }
    static void unloadMusicStream(Music) {}
    static void playMusicStream(Music) {}
    static void pauseMusicStream(Music) {}
    static void resumeMusicStream(Music) {}
    static void stopMusicStream(Music) {}
    static void updateMusicStream(Music) {}
    static bool isMusicStreamPlaying(Music) { return false; }
    static void setMusicVolume(Music, float) {}
    static void seekMusicStream(Music, float) {}
    static float getMusicTimeLength(Music) { return 0.0f; }
    static float getMusicTimePlayed(Music) { return 0.0f; }

    static audio::AudioManager::RaylibHooks& hooks() {
        static audio::AudioManager::RaylibHooks instance{
            &DummyRaylib::loadSound,
            &DummyRaylib::unloadSound,
            &DummyRaylib::loadSoundAlias,
            &DummyRaylib::unloadSoundAlias,
            &DummyRaylib::playSound,
            &DummyRaylib::stopSound,
            &DummyRaylib::isSoundPlaying,
            &DummyRaylib::setSoundVolume,
            &DummyRaylib::setSoundPitch,
            &DummyRaylib::setSoundPan,
            &DummyRaylib::loadMusicStream,
            &DummyRaylib::unloadMusicStream,
            &DummyRaylib::playMusicStream,
            &DummyRaylib::pauseMusicStream,
            &DummyRaylib::resumeMusicStream,
            &DummyRaylib::stopMusicStream,
            &DummyRaylib::updateMusicStream,
            &DummyRaylib::isMusicStreamPlaying,
            &DummyRaylib::setMusicVolume,
            &DummyRaylib::seekMusicStream,
            &DummyRaylib::getMusicTimeLength,
            &DummyRaylib::getMusicTimePlayed
        };
        return instance;
    }
};

struct AudioManagerWindowTestFixture {
    AudioManagerWindowTestFixture() {
        audio::AudioManager::resetForTesting();
        ConfigurationManager::loadOrDefault();

        originalEnabled = ConfigurationManager::getBool(
            "audio::core::enabled",
            ConfigurationManager::getBool("audio::enabled", true));
        originalMasterVolume = ConfigurationManager::getDouble(
            "audio::volumes::master",
            ConfigurationManager::getDouble("audio::master_volume", 1.0));
        originalMusicVolume = ConfigurationManager::getDouble(
            "audio::volumes::music",
            ConfigurationManager::getDouble("audio::music_volume", 1.0));
        originalSfxVolume = ConfigurationManager::getDouble(
            "audio::volumes::sfx",
            ConfigurationManager::getDouble("audio::sfx_volume", 1.0));
        originalMaxConcurrent = ConfigurationManager::getInt(
            "audio::engine::max_concurrent_sounds",
            ConfigurationManager::getInt("audio::max_concurrent_sounds", 16));
        originalSearchPaths = ConfigurationManager::getStringList(
            "audio::engine::search_paths",
            ConfigurationManager::getStringList("audio::search_paths", {"assets/audio"}));
        originalPreloadSounds = ConfigurationManager::getStringList(
            "audio::preload::sounds",
            ConfigurationManager::getStringList("audio::preload_sounds", {}));
        originalPreloadMusic = ConfigurationManager::getStringList(
            "audio::preload::music",
            ConfigurationManager::getStringList("audio::preload_music", {}));

        backend.ready = true;
        audio::AudioManager::setBackendForTesting(&backend);
        audio::AudioManager::setRaylibHooksForTesting(&DummyRaylib::hooks());

        // Ensure tests start from a clean preload state
        ConfigurationManager::set("audio::preload::sounds", std::vector<std::string>{});
        ConfigurationManager::set("audio::preload::music", std::vector<std::string>{});
    }

    ~AudioManagerWindowTestFixture() {
        audio::AudioManager::resetForTesting();
        audio::AudioManager::setBackendForTesting(nullptr);
        audio::AudioManager::setRaylibHooksForTesting(nullptr);

        ConfigurationManager::set("audio::core::enabled", originalEnabled);
        ConfigurationManager::set("audio::volumes::master", originalMasterVolume);
        ConfigurationManager::set("audio::volumes::music", originalMusicVolume);
        ConfigurationManager::set("audio::volumes::sfx", originalSfxVolume);
        ConfigurationManager::set("audio::engine::max_concurrent_sounds", static_cast<int64_t>(originalMaxConcurrent));
        ConfigurationManager::set("audio::engine::search_paths", originalSearchPaths);
        ConfigurationManager::set("audio::preload::sounds", originalPreloadSounds);
        ConfigurationManager::set("audio::preload::music", originalPreloadMusic);
        ConfigurationManager::save();
        ConfigurationManager::loadOrDefault();
    }

    DummyBackend backend;
    bool originalEnabled{true};
    double originalMasterVolume{1.0};
    double originalMusicVolume{1.0};
    double originalSfxVolume{1.0};
    int originalMaxConcurrent{16};
    std::vector<std::string> originalSearchPaths{};
    std::vector<std::string> originalPreloadSounds{};
    std::vector<std::string> originalPreloadMusic{};
};

audio::AudioEvent makeEvent(audio::AudioEventType type,
                            const std::string& key,
                            std::uint64_t timestampMs,
                            std::string details = {}) {
    return audio::AudioEvent{type, key, timestampMs, std::move(details)};
}

std::string trimCopyTest(const std::string& value) {
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

std::string canonicalizePreloadTest(const std::string& value) {
    std::string trimmed = trimCopyTest(value);
    std::replace(trimmed.begin(), trimmed.end(), '\\', '/');
    std::transform(trimmed.begin(), trimmed.end(), trimmed.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return trimmed;
}

} // namespace

TEST_CASE_METHOD(AudioManagerWindowTestFixture,
                 "AudioManagerWindow subscribes and unsubscribes from audio events",
                 "[windows][audio]") {
    REQUIRE(audio::AudioManager::activeSubscriptionCountForTesting() == 0);

    {
        AudioManagerWindow window;
        REQUIRE(audio::AudioManager::activeSubscriptionCountForTesting() == 1);

        auto& subscription = AudioManagerWindowTestAccess::subscription(window);
        REQUIRE(subscription.active);
        REQUIRE(subscription.sink == &window);
    }

    REQUIRE(audio::AudioManager::activeSubscriptionCountForTesting() == 0);
}

TEST_CASE_METHOD(AudioManagerWindowTestFixture,
                 "AudioManagerWindow refreshes inventory snapshots when marked dirty",
                 "[windows][audio]") {
    AudioManagerWindow window;

    REQUIRE_FALSE(AudioManagerWindowTestAccess::inventoryDirty(window));

    AudioManagerWindowTestAccess::dispatchEvent(
        window,
        makeEvent(audio::AudioEventType::SoundLoaded, "laser", 1000));

    REQUIRE(AudioManagerWindowTestAccess::inventoryDirty(window));

    AudioManagerWindowTestAccess::refreshInventory(window);

    REQUIRE_FALSE(AudioManagerWindowTestAccess::inventoryDirty(window));
}

TEST_CASE_METHOD(AudioManagerWindowTestFixture,
                 "AudioManagerWindow caps event log size",
                 "[windows][audio]") {
    AudioManagerWindow window;
    const auto maxEntries = AudioManagerWindowTestAccess::maxLogSize(window);
    REQUIRE(maxEntries > 0);

    const std::size_t extraEntries = 5;
    const std::size_t totalEvents = maxEntries + extraEntries;

    for (std::size_t i = 0; i < totalEvents; ++i) {
        AudioManagerWindowTestAccess::dispatchEvent(
            window,
            makeEvent(audio::AudioEventType::SoundLoaded,
                      "key" + std::to_string(i),
                      1000 + static_cast<std::uint64_t>(i) * 10));
    }

    const auto& log = AudioManagerWindowTestAccess::eventLog(window);
    REQUIRE(log.size() == maxEntries);

    REQUIRE(log.front().event.key == "key" + std::to_string(extraEntries));
    REQUIRE(log.back().event.key == "key" + std::to_string(totalEvents - 1));
}

TEST_CASE_METHOD(AudioManagerWindowTestFixture,
                 "AudioManagerWindow persists newly loaded sounds into preload configuration",
                 "[windows][audio]") {
    AudioManagerWindow window;

    AudioManagerWindowTestAccess::noteSound(window,
                                            "spaceinvaders/laser.wav",
                                            "spaceinvaders/laser.wav",
                                            std::nullopt,
                                            true);

    REQUIRE(AudioManagerWindowTestAccess::isConfigDirty(window));
    REQUIRE(AudioManagerWindowTestAccess::preloadSounds(window).size() == 1);
    REQUIRE(AudioManagerWindowTestAccess::pendingSoundPreloads(window).size() == 1);
    REQUIRE(AudioManagerWindowTestAccess::sessionLoadedSoundKeys(window).size() == 1);

    REQUIRE(AudioManagerWindowTestAccess::applyConfig(window));

    auto persistedSounds = ConfigurationManager::getStringList(
        "audio::preload::sounds",
        ConfigurationManager::getStringList("audio::preload_sounds", {}));
    REQUIRE(std::find(persistedSounds.begin(), persistedSounds.end(), "spaceinvaders/laser.wav") != persistedSounds.end());
    REQUIRE(AudioManagerWindowTestAccess::pendingSoundPreloads(window).empty());
    REQUIRE(AudioManagerWindowTestAccess::sessionLoadedSoundKeys(window).empty());
}

TEST_CASE_METHOD(AudioManagerWindowTestFixture,
                 "AudioManagerWindow persists canonical identifiers when a load alias is provided",
                 "[windows][audio]") {
    AudioManagerWindow window;

    const std::string aliasKey{"laser-file"};
    const std::string noisyIdentifier{"  SpaceInvaders\\Laser.wav  "};

    AudioManagerWindowTestAccess::noteSound(window,
                                            aliasKey,
                                            noisyIdentifier,
                                            std::optional<std::string>{"laser"},
                                            true);

    REQUIRE(AudioManagerWindowTestAccess::isConfigDirty(window));
    REQUIRE(AudioManagerWindowTestAccess::preloadSounds(window).size() == 1);
    REQUIRE(AudioManagerWindowTestAccess::preloadSounds(window).front() == trimCopyTest(noisyIdentifier));
    REQUIRE(AudioManagerWindowTestAccess::pendingSoundPreloads(window).size() == 1);
    REQUIRE(AudioManagerWindowTestAccess::pendingSoundPreloads(window).front() == trimCopyTest(noisyIdentifier));
    REQUIRE(AudioManagerWindowTestAccess::sessionLoadedSoundKeys(window).size() == 1);

    REQUIRE(AudioManagerWindowTestAccess::applyConfig(window));

    auto persistedSounds = ConfigurationManager::getStringList(
        "audio::preload::sounds",
        ConfigurationManager::getStringList("audio::preload_sounds", {}));
    REQUIRE(persistedSounds.size() == 1);
    REQUIRE(canonicalizePreloadTest(persistedSounds.front()) == "spaceinvaders/laser.wav");
    REQUIRE(std::none_of(persistedSounds.begin(), persistedSounds.end(), [](const std::string& entry) {
        return entry == "laser" || entry == "laser-file";
    }));
    REQUIRE(AudioManagerWindowTestAccess::pendingSoundPreloads(window).empty());
    REQUIRE(AudioManagerWindowTestAccess::sessionLoadedSoundKeys(window).empty());
}

TEST_CASE_METHOD(AudioManagerWindowTestFixture,
                 "AudioManagerWindow avoids duplicating preloads for existing assets",
                 "[windows][audio]") {
    AudioManagerWindow window;

    AudioManagerWindowTestAccess::noteSound(window,
                                            "spaceinvaders/laser.wav",
                                            "spaceinvaders/laser.wav",
                                            std::nullopt,
                                            true);
    AudioManagerWindowTestAccess::noteSound(window,
                                            "spaceinvaders/laser.wav",
                                            "spaceinvaders/laser.wav",
                                            std::nullopt,
                                            false);

    REQUIRE(AudioManagerWindowTestAccess::preloadSounds(window).size() == 1);
    REQUIRE(AudioManagerWindowTestAccess::pendingSoundPreloads(window).size() == 1);
    REQUIRE(AudioManagerWindowTestAccess::sessionLoadedSoundKeys(window).size() == 1);
}

TEST_CASE_METHOD(AudioManagerWindowTestFixture,
                 "AudioManagerWindow apply trims and deduplicates preload lists",
                 "[windows][audio]") {
    AudioManagerWindow window;

    AudioManagerWindowTestAccess::setPreloadDrafts(window,
        {"  spaceinvaders/laser.wav  ", "SPACEINVADERS/LASER.WAV"},
        {"  bgm/theme.ogg  ", "BGM/THEME.OGG"});

    REQUIRE(AudioManagerWindowTestAccess::isConfigDirty(window));
    REQUIRE(AudioManagerWindowTestAccess::applyConfig(window));

    auto persistedSounds = ConfigurationManager::getStringList(
        "audio::preload::sounds",
        ConfigurationManager::getStringList("audio::preload_sounds", {}));
    REQUIRE(persistedSounds.size() == 1);
    REQUIRE(persistedSounds.front() == trimCopyTest(persistedSounds.front()));
    REQUIRE(canonicalizePreloadTest(persistedSounds.front()) == "spaceinvaders/laser.wav");

    auto persistedMusic = ConfigurationManager::getStringList(
        "audio::preload::music",
        ConfigurationManager::getStringList("audio::preload_music", {}));
    REQUIRE(persistedMusic.size() == 1);
    REQUIRE(persistedMusic.front() == trimCopyTest(persistedMusic.front()));
    REQUIRE(canonicalizePreloadTest(persistedMusic.front()) == "bgm/theme.ogg");
}
