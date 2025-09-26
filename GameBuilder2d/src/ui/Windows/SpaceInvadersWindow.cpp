#include "ui/Windows/SpaceInvadersWindow.h"
#include "ui/WindowContext.h"
#include <imgui.h>
#include <nlohmann/json.hpp>

namespace gb2d {

void SpaceInvadersWindow::unloadRenderTarget() {
    if (rt_w_ > 0 && rt_h_ > 0) {
        UnloadRenderTexture(rt_);
        rt_w_ = rt_h_ = 0;
    }
}

void SpaceInvadersWindow::ensureRenderTarget(int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (rt_w_ == w && rt_h_ == h) return;
    unloadRenderTarget();
    rt_ = LoadRenderTexture(w, h);
    rt_w_ = w; rt_h_ = h;
    resetGame(w, h);
}

void SpaceInvadersWindow::resetGame(int w, int h) {
    game_over_ = false; game_won_ = false;
    player_ = Player{};
    player_.pos = { w * 0.5f, h - 40.0f };
    player_.bullets.clear();
    invaders_.clear();

    float margin = 40.0f;
    float cellW = (w - 2*margin) / inv_cols_;
    float cellH = 28.0f;
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

void SpaceInvadersWindow::updateGame(float dt, int w, int h, bool acceptInput) {
    if (game_over_ || game_won_) return;
    // Input
    if (acceptInput) {
        if (IsKeyDown(KEY_LEFT))  player_.pos.x -= player_.speed * dt;
        if (IsKeyDown(KEY_RIGHT)) player_.pos.x += player_.speed * dt;
        player_.pos.x = std::max(12.0f, std::min(player_.pos.x, (float)w - 12.0f));
        player_.cooldown -= dt;
        if (IsKeyDown(KEY_SPACE) && player_.cooldown <= 0.0f) {
            player_.bullets.push_back(Bullet{ { player_.pos.x, player_.pos.y - 12.0f }, -420.0f, true });
            player_.cooldown = 0.18f;
        }
    }
    // Move bullets
    for (auto& b : player_.bullets) if (b.alive) { b.pos.y += b.vy * dt; if (b.pos.y < -10) b.alive = false; }
    player_.bullets.erase(std::remove_if(player_.bullets.begin(), player_.bullets.end(), [](const Bullet& b){ return !b.alive; }), player_.bullets.end());

    // Move invaders horizontally; if hit edge, step down and reverse
    float minX = 1e9f, maxX = -1e9f, maxY = 0.0f;
    for (const auto& inv : invaders_) if (inv.alive) { minX = std::min(minX, inv.rect.x); maxX = std::max(maxX, inv.rect.x + inv.rect.width); maxY = std::max(maxY, inv.rect.y + inv.rect.height); }
    bool stepDown = false;
    if (minX < 10.0f && inv_dir_ < 0.0f) stepDown = true;
    if (maxX > (float)w - 10.0f && inv_dir_ > 0.0f) stepDown = true;
    if (stepDown) { inv_dir_ *= -1.0f; for (auto& inv : invaders_) if (inv.alive) inv.rect.y += inv_step_down_; }
    for (auto& inv : invaders_) if (inv.alive) inv.rect.x += inv_speed_ * inv_dir_ * dt;

    // Collisions: bullet vs invader
    for (auto& b : player_.bullets) if (b.alive) {
        for (auto& inv : invaders_) if (inv.alive) {
            if (CheckCollisionPointRec(b.pos, inv.rect)) { inv.alive = false; b.alive = false; break; }
        }
    }

    // Check lose condition
    if (maxY >= player_.pos.y - 8.0f) game_over_ = true;
    // Win condition
    bool anyAlive = false; for (auto& inv : invaders_) if (inv.alive) { anyAlive = true; break; }
    game_won_ = !anyAlive;
}

void SpaceInvadersWindow::drawGame(int w, int h) {
    BeginTextureMode(rt_);
    ClearBackground(BLACK);
    // Player
    DrawTriangle({player_.pos.x, player_.pos.y}, {player_.pos.x-12.0f, player_.pos.y+12.0f}, {player_.pos.x+12.0f, player_.pos.y+12.0f}, GREEN);
    // Bullets
    for (auto& b : player_.bullets) if (b.alive) DrawLineV(b.pos, { b.pos.x, b.pos.y - 8.0f }, YELLOW);
    // Invaders
    for (auto& inv : invaders_) if (inv.alive) DrawRectangleRec(inv.rect, RED);
    if (game_over_) DrawText("GAME OVER", w/2 - 100, h/2 - 10, 20, RAYWHITE);
    if (game_won_) DrawText("YOU WIN!", w/2 - 90, h/2 - 10, 20, RAYWHITE);
    EndTextureMode();
}

void SpaceInvadersWindow::render(WindowContext& /*ctx*/) {
    // Compute the available size inside this ImGui window and ensure RT matches
    ImVec2 avail = ImGui::GetContentRegionAvail();
    int targetW = std::max(32, (int)avail.x);
    int targetH = std::max(32, (int)avail.y);
    ensureRenderTarget(targetW, targetH);

    // Basic controls row
    if (ImGui::Button("Reset")) resetGame(targetW, targetH);
    ImGui::SameLine();
    ImGui::TextDisabled("Use Left/Right + Space");

    // Update game with dt from raylib
    float dt = GetFrameTime();
    // Accept keyboard input only when this window is hovered/focused to avoid stealing input globally
    bool acceptInput = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    updateGame(dt, targetW, targetH, acceptInput);
    drawGame(targetW, targetH);

    // Draw the RT scaled to fit the available region; keep pixel-perfect by using exact size
    // Flip vertically (raylib RenderTexture is upside-down for ImGui)
    ImGui::BeginChild("game_view", ImVec2(0,0), false, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
    {
        // draw texture stretched to child content
        ImVec2 region = ImGui::GetContentRegionAvail();
        float drawW = region.x > 1 ? region.x : (float)rt_w_;
        float drawH = region.y > 1 ? region.y : (float)rt_h_;
        // ImGui expects ImTextureID; raylib texture id is unsigned int
        ImTextureID texId = (ImTextureID)(intptr_t)rt_.texture.id;

        // Build UVs for vertical flip
        ImVec2 uv0(0, 1);
        ImVec2 uv1(1, 0);
        ImGui::Image(texId, ImVec2(drawW, drawH), uv0, uv1);
    }
    ImGui::EndChild();
}

void SpaceInvadersWindow::serialize(nlohmann::json& out) const {
    out["title"] = title_;
}

void SpaceInvadersWindow::deserialize(const nlohmann::json& in) {
    if (auto it = in.find("title"); it != in.end() && it->is_string()) title_ = *it;
}

} // namespace gb2d
