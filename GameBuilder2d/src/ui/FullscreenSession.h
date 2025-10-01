#pragma once

#include "games/Game.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace gb2d {

class FullscreenSession {
public:
    struct Callbacks {
        std::function<void()> onEnter;
        std::function<void()> onExit;
        std::function<void()> requestGameTextureReset;
    };

    explicit FullscreenSession(Callbacks callbacks = {});

    [[nodiscard]] bool isActive() const noexcept;
    void requestStart(games::Game& game, const std::string& gameId, int width, int height);
    void requestStop();
    void tick(float dt);
    void setResetHook(std::function<void()> hook);

private:
    Callbacks callbacks_{};
    bool active_ = false;
    games::Game* game_ = nullptr;
    std::string gameId_{};
    int targetWidth_ = 0;
    int targetHeight_ = 0;
    int returnWidth_ = 0;
    int returnHeight_ = 0;
    int previousWidth_ = 0;
    int previousHeight_ = 0;
    int previousMonitor_ = 0;
    bool previousFullscreen_ = false;
    std::function<void()> reset_hook_{};
};

} // namespace gb2d
