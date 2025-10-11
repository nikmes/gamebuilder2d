#include <catch2/catch_test_macros.hpp>

#include "services/audio/AudioManager.h"
#include "services/configuration/ConfigurationManager.h"
#include "ui/Windows/AudioManagerWindow.h"

#include <cstdint>
#include <string>
#include <utility>

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
};

} // namespace gb2d

namespace {

struct AudioManagerWindowTestFixture {
    AudioManagerWindowTestFixture() {
        audio::AudioManager::resetForTesting();
        ConfigurationManager::loadOrDefault();
    }

    ~AudioManagerWindowTestFixture() {
        audio::AudioManager::resetForTesting();
    }
};

audio::AudioEvent makeEvent(audio::AudioEventType type,
                            const std::string& key,
                            std::uint64_t timestampMs,
                            std::string details = {}) {
    return audio::AudioEvent{type, key, timestampMs, std::move(details)};
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
