#include "games/PlarformerGame.h"

#include <algorithm>

namespace gb2d::games {

namespace {
constexpr float kEvenOutSpeed = 700.0f;
constexpr Vector2 kPlayerSize{40.0f, 40.0f};
constexpr Vector2 kCameraBounds{0.2f, 0.2f};
} // namespace

const char* PlarformerGame::id() const {
    return "plarformer";
}

const char* PlarformerGame::name() const {
    return "Plarformer";
}

void PlarformerGame::init(int width, int height) {
    resetState(width, height);
}

void PlarformerGame::reset(int width, int height) {
    resetState(width, height);
}

void PlarformerGame::unload() {
    envItems_.clear();
}

void PlarformerGame::resetState(int width, int height) {
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);

    envItems_.clear();
    envItems_.reserve(5);
    envItems_.push_back({ Rectangle{ 0,    0,    1000, 400 }, false, LIGHTGRAY });
    envItems_.push_back({ Rectangle{ 0,  400,   1000, 200 }, true,  GRAY });
    envItems_.push_back({ Rectangle{ 300, 200,   400,  10 }, true,  GRAY });
    envItems_.push_back({ Rectangle{ 250, 300,   100,  10 }, true,  GRAY });
    envItems_.push_back({ Rectangle{ 650, 300,   100,  10 }, true,  GRAY });

    player_.position = Vector2{ 400.0f, 280.0f };
    player_.speed = 0.0f;
    player_.canJump = false;

    camera_.target = player_.position;
    camera_.offset = Vector2{ width_ * 0.5f, height_ * 0.5f };
    camera_.rotation = 0.0f;
    camera_.zoom = 1.0f;

    cameraOption_ = 0;
    eveningOut_ = false;
    evenOutTarget_ = player_.position.y;
}

void PlarformerGame::update(float dt, int width, int height, bool acceptInput) {
    width_ = std::max(width, 1);
    height_ = std::max(height, 1);

    if (acceptInput) {
        updatePlayer(dt, true);

        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            camera_.zoom = std::clamp(camera_.zoom + wheel * zoomStep_, minZoom_, maxZoom_);
        }

        if (IsKeyPressed(KEY_R)) {
            camera_.zoom = 1.0f;
            player_.position = Vector2{ 400.0f, 280.0f };
            player_.speed = 0.0f;
        }

        if (IsKeyPressed(KEY_C)) {
            cameraOption_ = (cameraOption_ + 1) % static_cast<int>(cameraUpdaters_.size());
        }
    } else {
        updatePlayer(dt, false);
    }

    auto updater = cameraUpdaters_[cameraOption_];
    (this->*updater)(dt, width_, height_);
}

void PlarformerGame::render(int width, int height) {
    (void)width;
    (void)height;

    ClearBackground(LIGHTGRAY);

    BeginMode2D(camera_);
    for (const auto& env : envItems_) {
        DrawRectangleRec(env.rect, env.color);
    }

    Rectangle playerRect{ player_.position.x - kPlayerSize.x * 0.5f,
                          player_.position.y - kPlayerSize.y,
                          kPlayerSize.x,
                          kPlayerSize.y };
    DrawRectangleRec(playerRect, RED);
    DrawCircleV(player_.position, 5.0f, GOLD);

    EndMode2D();

    DrawText("Controls:", 20, 20, 18, BLACK);
    DrawText("- Left/Right to move", 40, 44, 16, DARKGRAY);
    DrawText("- Space to jump", 40, 66, 16, DARKGRAY);
    DrawText("- Mouse Wheel to zoom, R to reset", 40, 88, 16, DARKGRAY);
    DrawText("- C to change camera mode", 40, 110, 16, DARKGRAY);

    DrawText("Current camera mode:", 20, 140, 18, BLACK);
    DrawText(kCameraDescriptions[cameraOption_], 40, 164, 16, DARKGRAY);
}

void PlarformerGame::updatePlayer(float dt, bool acceptInput) {
    if (acceptInput) {
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
            player_.position.x -= playerHorizontalSpeed_ * dt;
        }
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
            player_.position.x += playerHorizontalSpeed_ * dt;
        }
        if (player_.canJump && (IsKeyDown(KEY_SPACE) || IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))) {
            player_.speed = -playerJumpSpeed_;
            player_.canJump = false;
        }
    }

    bool hitObstacle = false;
    for (auto& env : envItems_) {
        if (!env.blocking) continue;
        if (env.rect.x <= player_.position.x &&
            env.rect.x + env.rect.width >= player_.position.x &&
            env.rect.y >= player_.position.y &&
            env.rect.y <= player_.position.y + player_.speed * dt) {
            hitObstacle = true;
            player_.speed = 0.0f;
            player_.position.y = env.rect.y;
            break;
        }
    }

    if (!hitObstacle) {
        player_.position.y += player_.speed * dt;
        player_.speed += gravity_ * dt;
        player_.canJump = false;
    } else {
        player_.canJump = true;
    }
}

void PlarformerGame::updateCameraCenter(float, int width, int height) {
    camera_.offset = Vector2{ width * 0.5f, height * 0.5f };
    camera_.target = player_.position;
}

void PlarformerGame::updateCameraCenterInsideMap(float, int width, int height) {
    camera_.target = player_.position;
    camera_.offset = Vector2{ width * 0.5f, height * 0.5f };

    float minX = 1000.0f;
    float minY = 1000.0f;
    float maxX = -1000.0f;
    float maxY = -1000.0f;

    for (const auto& env : envItems_) {
        minX = std::min(env.rect.x, minX);
        maxX = std::max(env.rect.x + env.rect.width, maxX);
        minY = std::min(env.rect.y, minY);
        maxY = std::max(env.rect.y + env.rect.height, maxY);
    }

    Vector2 max = GetWorldToScreen2D(Vector2{ maxX, maxY }, camera_);
    Vector2 min = GetWorldToScreen2D(Vector2{ minX, minY }, camera_);

    if (max.x < width) camera_.offset.x = width - (max.x - width * 0.5f);
    if (max.y < height) camera_.offset.y = height - (max.y - height * 0.5f);
    if (min.x > 0) camera_.offset.x = width * 0.5f - min.x;
    if (min.y > 0) camera_.offset.y = height * 0.5f - min.y;
}

void PlarformerGame::updateCameraCenterSmoothFollow(float dt, int width, int height) {
    constexpr float minSpeed = 30.0f;
    constexpr float minEffectLength = 10.0f;
    constexpr float fractionSpeed = 0.8f;

    camera_.offset = Vector2{ width * 0.5f, height * 0.5f };
    Vector2 diff = Vector2Subtract(player_.position, camera_.target);
    float length = Vector2Length(diff);

    if (length > minEffectLength) {
        float speed = std::max(fractionSpeed * length, minSpeed);
        camera_.target = Vector2Add(camera_.target, Vector2Scale(diff, speed * dt / length));
    }
}

void PlarformerGame::updateCameraEvenOutOnLanding(float dt, int width, int height) {
    camera_.offset = Vector2{ width * 0.5f, height * 0.5f };
    camera_.target.x = player_.position.x;

    if (eveningOut_) {
        if (evenOutTarget_ > camera_.target.y) {
            camera_.target.y += kEvenOutSpeed * dt;
            if (camera_.target.y > evenOutTarget_) {
                camera_.target.y = evenOutTarget_;
                eveningOut_ = false;
            }
        } else {
            camera_.target.y -= kEvenOutSpeed * dt;
            if (camera_.target.y < evenOutTarget_) {
                camera_.target.y = evenOutTarget_;
                eveningOut_ = false;
            }
        }
    } else {
        if (player_.canJump && player_.speed == 0.0f && player_.position.y != camera_.target.y) {
            eveningOut_ = true;
            evenOutTarget_ = player_.position.y;
        }
    }
}

void PlarformerGame::updateCameraPlayerBoundsPush(float, int width, int height) {
    Vector2 bboxWorldMin = GetScreenToWorld2D(Vector2{ (1 - kCameraBounds.x) * 0.5f * width,
                                                       (1 - kCameraBounds.y) * 0.5f * height },
                                              camera_);
    Vector2 bboxWorldMax = GetScreenToWorld2D(Vector2{ (1 + kCameraBounds.x) * 0.5f * width,
                                                       (1 + kCameraBounds.y) * 0.5f * height },
                                              camera_);
    camera_.offset = Vector2{ (1 - kCameraBounds.x) * 0.5f * width,
                              (1 - kCameraBounds.y) * 0.5f * height };

    if (player_.position.x < bboxWorldMin.x) camera_.target.x = player_.position.x;
    if (player_.position.y < bboxWorldMin.y) camera_.target.y = player_.position.y;
    if (player_.position.x > bboxWorldMax.x) camera_.target.x = bboxWorldMin.x + (player_.position.x - bboxWorldMax.x);
    if (player_.position.y > bboxWorldMax.y) camera_.target.y = bboxWorldMin.y + (player_.position.y - bboxWorldMax.y);
}

} // namespace gb2d::games
