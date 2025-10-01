#include "games/SpaceInvaders.h"
#include <algorithm>

namespace gb2d::games {

void SpaceInvaders::init(int width, int height) {
    width_ = width;
    height_ = height;
    rebuildArena(width_, height_);
}

void SpaceInvaders::reset(int width, int height) {
    width_ = width;
    height_ = height;
    rebuildArena(width_, height_);
}

void SpaceInvaders::onResize(int width, int height) {
    reset(width, height);
}

void SpaceInvaders::unload() {
    player_.bullets.clear();
    invaders_.clear();
}

void SpaceInvaders::update(float dt, int width, int height, bool acceptInput) {
    if (game_over_ || game_won_) return;
    width_ = width;
    height_ = height;

    if (acceptInput) {
        if (IsKeyDown(KEY_LEFT))  player_.pos.x -= player_.speed * dt;
        if (IsKeyDown(KEY_RIGHT)) player_.pos.x += player_.speed * dt;
        player_.pos.x = std::max(12.0f, std::min(player_.pos.x, (float)width_ - 12.0f));
        player_.cooldown -= dt;
        if (IsKeyDown(KEY_SPACE) && player_.cooldown <= 0.0f) {
            player_.bullets.push_back(Bullet{ { player_.pos.x, player_.pos.y - 12.0f }, -420.0f, true });
            player_.cooldown = 0.18f;
        }
    }

    updateBullets(dt);
    updateInvaders(dt, width_);
    handleCollisions();
}

void SpaceInvaders::render(int width, int height) {
    (void)width; (void)height;
    ClearBackground(BLACK);
    DrawTriangle({player_.pos.x, player_.pos.y}, {player_.pos.x-12.0f, player_.pos.y+12.0f}, {player_.pos.x+12.0f, player_.pos.y+12.0f}, GREEN);
    for (auto& b : player_.bullets) if (b.alive) DrawLineV(b.pos, { b.pos.x, b.pos.y - 8.0f }, YELLOW);
    for (auto& inv : invaders_) if (inv.alive) DrawRectangleRec(inv.rect, RED);
    if (game_over_) DrawText("GAME OVER", width_/2 - 100, height_/2 - 10, 20, RAYWHITE);
    if (game_won_) DrawText("YOU WIN!", width_/2 - 90, height_/2 - 10, 20, RAYWHITE);
}

void SpaceInvaders::rebuildArena(int width, int height) {
    unload();
    game_over_ = false;
    game_won_ = false;
    player_ = Player{};
    player_.pos = { width * 0.5f, height - 40.0f };
    player_.bullets.clear();

    float margin = 40.0f;
    float cellW = (width - 2*margin) / inv_cols_;
    float cellH = 28.0f;
    invaders_.clear();
    invaders_.reserve(inv_cols_ * inv_rows_);
    for (int r = 0; r < inv_rows_; ++r) {
        for (int c = 0; c < inv_cols_; ++c) {
            float x = margin + c * cellW + 0.5f * (cellW - 24.0f);
            float y = margin + r * cellH;
            invaders_.push_back(Invader{ Rectangle{ x, y, 24.0f, 16.0f }, true });
        }
    }
    inv_dir_ = 1.0f;
    inv_speed_ = 60.0f;
}

void SpaceInvaders::updateBullets(float dt) {
    for (auto& b : player_.bullets) {
        if (!b.alive) continue;
        b.pos.y += b.vy * dt;
        if (b.pos.y < -10.0f) b.alive = false;
    }
    player_.bullets.erase(std::remove_if(player_.bullets.begin(), player_.bullets.end(), [](const Bullet& b){ return !b.alive; }), player_.bullets.end());
}

void SpaceInvaders::updateInvaders(float dt, int width) {
    float minX = 1e9f, maxX = -1e9f, maxY = 0.0f;
    for (const auto& inv : invaders_) if (inv.alive) {
        minX = std::min(minX, inv.rect.x);
        maxX = std::max(maxX, inv.rect.x + inv.rect.width);
        maxY = std::max(maxY, inv.rect.y + inv.rect.height);
    }
    bool stepDown = false;
    if (minX < 10.0f && inv_dir_ < 0.0f) stepDown = true;
    if (maxX > (float)width - 10.0f && inv_dir_ > 0.0f) stepDown = true;
    if (stepDown) {
        inv_dir_ *= -1.0f;
        for (auto& inv : invaders_) if (inv.alive) inv.rect.y += inv_step_down_;
    }
    for (auto& inv : invaders_) if (inv.alive) inv.rect.x += inv_speed_ * inv_dir_ * dt;

    if (maxY >= player_.pos.y - 8.0f) game_over_ = true;
}

void SpaceInvaders::handleCollisions() {
    for (auto& b : player_.bullets) if (b.alive) {
        for (auto& inv : invaders_) if (inv.alive) {
            if (CheckCollisionPointRec(b.pos, inv.rect)) {
                inv.alive = false;
                b.alive = false;
                break;
            }
        }
    }
    bool anyAlive = false;
    for (auto& inv : invaders_) if (inv.alive) { anyAlive = true; break; }
    game_won_ = !anyAlive;
}

} // namespace gb2d::games
