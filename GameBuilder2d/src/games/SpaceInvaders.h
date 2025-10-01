#pragma once
#include "games/Game.h"
#include <raylib.h>
#include <vector>

namespace gb2d::games {

class SpaceInvaders final : public Game {
public:
    SpaceInvaders() = default;
    ~SpaceInvaders() override = default;

    const char* id() const override { return "space-invaders"; }
    const char* name() const override { return "Space Invaders"; }

    void init(int width, int height) override;
    void update(float dt, int width, int height, bool acceptInput) override;
    void render(int width, int height) override;
    void unload() override;
    void onResize(int width, int height) override;
    void reset(int width, int height) override;

private:
    struct Bullet { Vector2 pos; float vy; bool alive{true}; };
    struct Player { Vector2 pos{0,0}; float speed{400}; std::vector<Bullet> bullets; float cooldown{0.0f}; };
    struct Invader { Rectangle rect; bool alive{true}; };

    void rebuildArena(int width, int height);
    void updateBullets(float dt);
    void updateInvaders(float dt, int width);
    void handleCollisions();

    int width_{0};
    int height_{0};
    Player player_{};
    std::vector<Invader> invaders_{};
    int inv_cols_{10};
    int inv_rows_{5};
    float inv_dir_{1.0f};
    float inv_speed_{60.0f};
    float inv_step_down_{24.0f};
    bool game_over_{false};
    bool game_won_{false};
};

} // namespace gb2d::games
