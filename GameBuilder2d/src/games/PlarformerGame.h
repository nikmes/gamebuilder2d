#pragma once

#include "games/Game.h"

#include <raylib.h>
#include <raymath.h>

#include <array>
#include <string>
#include <vector>

namespace gb2d::games {

class PlarformerGame : public Game {
public:
    const char* id() const override;
    const char* name() const override;

    void init(int width, int height) override;
    void update(float dt, int width, int height, bool acceptInput) override;
    void render(int width, int height) override;
    void unload() override;
    void reset(int width, int height) override;

private:
    struct Player {
        Vector2 position{};
        float speed{};
        bool canJump{};
    };

    struct EnvItem {
        Rectangle rect{};
        bool blocking{};
        Color color{};
    };

    using CameraUpdateFn = void (PlarformerGame::*)(float, int, int);

    void resetState(int width, int height);
    void updatePlayer(float dt, bool acceptInput);

    void updateCameraCenter(float dt, int width, int height);
    void updateCameraCenterInsideMap(float dt, int width, int height);
    void updateCameraCenterSmoothFollow(float dt, int width, int height);
    void updateCameraEvenOutOnLanding(float dt, int width, int height);
    void updateCameraPlayerBoundsPush(float dt, int width, int height);

    static constexpr float gravity_ = 400.0f;
    static constexpr float playerJumpSpeed_ = 350.0f;
    static constexpr float playerHorizontalSpeed_ = 200.0f;
    static constexpr float zoomStep_ = 0.05f;
    static constexpr float minZoom_ = 0.25f;
    static constexpr float maxZoom_ = 3.0f;

    inline static constexpr std::array<const char*, 5> kCameraDescriptions{
        "Follow player center",
        "Follow player center, but clamp to map edges",
        "Follow player center; smoothed",
        "Follow player center horizontally; update player center vertically after landing",
        "Player push camera on getting too close to screen edge"
    };

    const std::array<CameraUpdateFn, 5> cameraUpdaters_ {
        &PlarformerGame::updateCameraCenter,
        &PlarformerGame::updateCameraCenterInsideMap,
        &PlarformerGame::updateCameraCenterSmoothFollow,
        &PlarformerGame::updateCameraEvenOutOnLanding,
        &PlarformerGame::updateCameraPlayerBoundsPush
    };

    std::vector<EnvItem> envItems_;
    Player player_{};
    Camera2D camera_{};

    int width_ = 0;
    int height_ = 0;

    int cameraOption_ = 0;
    bool eveningOut_ = false;
    float evenOutTarget_ = 0.0f;
};

} // namespace gb2d::games
