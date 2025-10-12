#include "ui/FullscreenSession.h"

#include <raylib.h>
#include <algorithm>

#include "services/configuration/ConfigurationManager.h"
#include "services/logger/LogManager.h"
#include "services/hotkey/HotKeyActions.h"
#include "services/hotkey/HotKeyManager.h"

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

    const int fallbackWidth = previousWidth_ > 0 ? previousWidth_ : 1920;
    const int fallbackHeight = previousHeight_ > 0 ? previousHeight_ : 1080;

    auto resolveDimension = [](const char* gameKey, const char* windowKey, const char* fullscreenKey, int fallback) {
        int value = static_cast<int>(ConfigurationManager::getInt(gameKey, -1));
        if (value > 0) return value;
        value = static_cast<int>(ConfigurationManager::getInt(windowKey, -1));
        if (value > 0) return value;
        value = static_cast<int>(ConfigurationManager::getInt(fullscreenKey, fallback));
        if (value <= 0) value = fallback;
        return value;
    };

    int desiredWidth = resolveDimension("fullscreen.game_width", "window.width", "fullscreen.width", fallbackWidth);
    int desiredHeight = resolveDimension("fullscreen.game_height", "window.height", "fullscreen.height", fallbackHeight);

    desiredWidth = std::max(desiredWidth, 320);
    desiredHeight = std::max(desiredHeight, 240);

    gb2d::logging::LogManager::debug("FullscreenSession target resolution {}x{} (window={}x{}, fullscreen={}x{})",
                                     desiredWidth,
                                     desiredHeight,
                                     ConfigurationManager::getInt("window.width", fallbackWidth),
                                     ConfigurationManager::getInt("window.height", fallbackHeight),
                                     ConfigurationManager::getInt("fullscreen.width", fallbackWidth),
                                     ConfigurationManager::getInt("fullscreen.height", fallbackHeight));

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

    if (IsWindowFullscreen()) {
        if (!previousFullscreen_) {
            ToggleFullscreen();
            if (previousWidth_ > 0 && previousHeight_ > 0) {
                SetWindowSize(previousWidth_, previousHeight_);
            }
        } else if (previousWidth_ > 0 && previousHeight_ > 0) {
            SetWindowSize(previousWidth_, previousHeight_);
        }
    } else if (previousWidth_ > 0 && previousHeight_ > 0) {
        SetWindowSize(previousWidth_, previousHeight_);
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

    const bool ctrlDown = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    const bool exitWithCtrlW = ctrlDown && IsKeyPressed(KEY_W);
    const bool exitViaHotkey = gb2d::hotkeys::HotKeyManager::isInitialized() &&
                               gb2d::hotkeys::HotKeyManager::consumeTriggered(gb2d::hotkeys::actions::FullscreenExit);
    if (exitWithCtrlW || exitViaHotkey) {
        requestStop();
    }
}

void FullscreenSession::setResetHook(std::function<void()> hook) {
    reset_hook_ = std::move(hook);
}

} // namespace gb2d
