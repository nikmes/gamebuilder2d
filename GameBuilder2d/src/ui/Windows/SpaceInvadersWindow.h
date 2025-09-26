#pragma once
#include "ui/Window.h"
#include <nlohmann/json_fwd.hpp>
#include <raylib.h>
#include <string>
#include <vector>

namespace gb2d {

class SpaceInvadersWindow : public IWindow {
public:
    SpaceInvadersWindow() = default;
    ~SpaceInvadersWindow() override { unloadRenderTarget(); }

    const char* typeId() const override { return "space-invaders"; }
    const char* displayName() const override { return "Space Invaders"; }

    std::string title() const override { return title_; }
    void setTitle(std::string t) override { title_ = std::move(t); }

    void render(WindowContext& ctx) override;

    void serialize(nlohmann::json& out) const override;
    void deserialize(const nlohmann::json& in) override;

private:
    // Game entities
    struct Bullet { Vector2 pos; float vy; bool alive{true}; };
    struct Player { Vector2 pos{0,0}; float speed{400}; std::vector<Bullet> bullets; float cooldown{0.0f}; };
    struct Invader { Rectangle rect; bool alive{true}; };

    void ensureRenderTarget(int w, int h);
    void unloadRenderTarget();
    void resetGame(int w, int h);
    void updateGame(float dt, int w, int h, bool acceptInput);
    void drawGame(int w, int h);

    // State
    std::string title_ { "Space Invaders" };
    RenderTexture2D rt_{};
    int rt_w_{0}, rt_h_{0};
    bool game_over_{false};
    bool game_won_{false};
    Player player_{};
    std::vector<Invader> invaders_{};
    int inv_cols_{10};
    int inv_rows_{5};
    float inv_dir_{1.0f};
    float inv_speed_{60.0f};
    float inv_step_down_{24.0f};
};

} // namespace gb2d
