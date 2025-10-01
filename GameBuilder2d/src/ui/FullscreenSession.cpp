#include "ui/FullscreenSession.h"

#include <raylib.h>
#include <algorithm>

#include "services/configuration/ConfigurationManager.h"

namespace gb2d {

FullscreenSession::FullscreenSession(Callbacks callbacks)
    : callbacks_(std::move(callbacks)) {}

bool FullscreenSession::isActive() const noexcept {
    return active_;
}

void FullscreenSession::requestStart(games::Game& game, const std::string& gameId, int width, int height) {
    if (active_) return;

    game_ = &game;
    gameId_ = gameId;
    returnWidth_ = width;
    returnHeight_ = height;

    previousFullscreen_ = IsWindowFullscreen();
    previousMonitor_ = GetCurrentMonitor();
    previousWidth_ = GetScreenWidth();
    previousHeight_ = GetScreenHeight();

    int desiredWidth = static_cast<int>(ConfigurationManager::getInt("fullscreen::width", previousWidth_ > 0 ? previousWidth_ : 1920));
    int desiredHeight = static_cast<int>(ConfigurationManager::getInt("fullscreen::height", previousHeight_ > 0 ? previousHeight_ : 1080));
    desiredWidth = std::max(desiredWidth, 320);
    desiredHeight = std::max(desiredHeight, 240);

    if (!previousFullscreen_) {
        ToggleFullscreen();
    }

    if (IsWindowFullscreen()) {
        SetWindowSize(desiredWidth, desiredHeight);
    }

    targetWidth_ = GetScreenWidth();
    targetHeight_ = GetScreenHeight();
    if (targetWidth_ <= 0 || targetHeight_ <= 0) {
        targetWidth_ = desiredWidth;
        targetHeight_ = desiredHeight;
    }

    if (game_) {
        game_->reset(targetWidth_, targetHeight_);
    }

    active_ = true;
    if (callbacks_.onEnter) callbacks_.onEnter();
}

void FullscreenSession::requestStop() {
    if (!active_) return;

    if (!previousFullscreen_ && IsWindowFullscreen()) {
        ToggleFullscreen();
        if (previousWidth_ > 0 && previousHeight_ > 0) {
            SetWindowSize(previousWidth_, previousHeight_);
        }
    }

    if (game_ && returnWidth_ > 0 && returnHeight_ > 0) {
        game_->reset(returnWidth_, returnHeight_);
    }

    if (reset_hook_) {
        reset_hook_();
        reset_hook_ = {};
    }
    if (callbacks_.requestGameTextureReset) callbacks_.requestGameTextureReset();
    if (callbacks_.onExit) callbacks_.onExit();

    active_ = false;
    game_ = nullptr;
    gameId_.clear();
    targetWidth_ = 0;
    targetHeight_ = 0;
    returnWidth_ = 0;
    returnHeight_ = 0;
    previousWidth_ = 0;
    previousHeight_ = 0;
    previousMonitor_ = 0;
    previousFullscreen_ = false;
}

void FullscreenSession::tick(float dt) {
    if (!active_ || !game_) return;

    if (!IsWindowFullscreen()) {
        ToggleFullscreen();
    }

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();
    if (screenW > 0 && screenH > 0 && (screenW != targetWidth_ || screenH != targetHeight_)) {
        targetWidth_ = screenW;
        targetHeight_ = screenH;
        game_->onResize(targetWidth_, targetHeight_);
    }

    const bool acceptInput = true;
    game_->update(dt, targetWidth_, targetHeight_, acceptInput);

    ClearBackground(BLACK);
    game_->render(targetWidth_, targetHeight_);

    bool ctrlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool exitWithCtrlW = ctrlDown && IsKeyPressed(KEY_W);
    bool exitWithEsc = IsKeyPressed(KEY_ESCAPE);
    if (exitWithCtrlW || exitWithEsc) {
        requestStop();
    }
}

void FullscreenSession::setResetHook(std::function<void()> hook) {
    reset_hook_ = std::move(hook);
}

} // namespace gb2d
