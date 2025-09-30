#include "ui/Windows/Games/PacMan.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string>
#include <raymath.h>

namespace gb2d::games {

namespace {

struct Int2 { int x; int y; };

constexpr std::array<const char*, 24> kMapTemplate = {
    "############################",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#o####.#####.##.#####.####o#",
    "#.####.#####.##.#####.####.#",
    "#..........................#",
    "#.####.##.########.##.####.#",
    "#.####.##.########.##.####.#",
    "#......##....##....##......#",
    "######.#####.##.#####.######",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#o####.#####.##.#####.####o#",
    "#.####.##..........##.####.#",
    "#......##.########.##......#",
    "######.##.########.##.######",
    "#............##............#",
    "#.####.#####.##.#####.####.#",
    "#o..##................##..o#",
    "###.##.##.########.##.##.###",
    "#......##....##....##......#",
    "#.##########.##.##########.#",
    "#..........................#",
    "############################"
};

constexpr Int2 kPacmanStart{13, 17};
constexpr std::array<Int2, 4> kGhostStartTiles = {
    Int2{13, 11},
    Int2{14, 11},
    Int2{12, 11},
    Int2{13, 12}
};
constexpr std::array<Int2, 4> kScatterCorners = {
    Int2{1, 1},
    Int2{26, 1},
    Int2{1, 21},
    Int2{26, 21}
};
constexpr std::array<Color, 4> kGhostColors = {
    Color{255, 0, 0, 255},
    Color{255, 105, 180, 255},
    Color{0, 255, 255, 255},
    Color{255, 165, 0, 255}
};

constexpr Vector2 DIR_RIGHT{1.0f, 0.0f};
constexpr Vector2 DIR_LEFT{-1.0f, 0.0f};
constexpr Vector2 DIR_UP{0.0f, -1.0f};
constexpr Vector2 DIR_DOWN{0.0f, 1.0f};
constexpr std::array<Vector2, 4> kDirections = { DIR_RIGHT, DIR_LEFT, DIR_UP, DIR_DOWN };

} // namespace

void PacMan::init(int width, int height) {
    setup(width, height);
}

void PacMan::reset(int width, int height) {
    setup(width, height);
}

void PacMan::onResize(int width, int height) {
    width_ = width;
    height_ = height;
    if (grid_.empty()) {
        setup(width, height);
        return;
    }

    tileSize_ = std::clamp(std::min(width_ / std::max(1, gridWidth()), height_ / std::max(1, gridHeight())), 12, 42);
    offset_ = { (width_ - gridWidth() * tileSize_) * 0.5f, std::max(24.0f, (height_ - gridHeight() * tileSize_) * 0.5f) };

    // Reproject positions to the new scale
    pacmanPos_ = tileCenter(static_cast<int>(worldToGrid(pacmanPos_).x), static_cast<int>(worldToGrid(pacmanPos_).y));
    for (auto& ghost : ghosts_) {
        Vector2 gTile = worldToGrid(ghost.pos);
        ghost.pos = tileCenter(static_cast<int>(gTile.x), static_cast<int>(gTile.y));
    }
}

void PacMan::unload() {
    grid_.clear();
    ghosts_.clear();
}

void PacMan::setup(int width, int height) {
    width_ = width;
    height_ = height;

    rebuildGrid();

    tileSize_ = std::clamp(std::min(width_ / std::max(1, gridWidth()), height_ / std::max(1, gridHeight())), 12, 42);
    offset_ = { (width_ - gridWidth() * tileSize_) * 0.5f, std::max(24.0f, (height_ - gridHeight() * tileSize_) * 0.5f) };
    pacmanSpeed_ = std::max(60.0f, tileSize_ * 5.2f);

    score_ = 0;
    lives_ = 3;
    victory_ = false;
    gameOver_ = false;
    powerTimer_ = 0.0f;
    deathTimer_ = 0.0f;

    scatterPhase_ = true;
    globalModeTimer_ = 7.0f;

    resetAfterDeath();
}

void PacMan::rebuildGrid() {
    grid_.assign(kMapTemplate.begin(), kMapTemplate.end());
    pelletsRemaining_ = 0;

    for (auto& row : grid_) {
        for (char c : row) {
            if (c == '.' || c == 'o') {
                pelletsRemaining_++;
            }
        }
    }

    // Clear spawn tiles to avoid pellet placement
    if (kPacmanStart.y >= 0 && kPacmanStart.y < gridHeight() && kPacmanStart.x >= 0 && kPacmanStart.x < gridWidth()) {
        if (grid_[kPacmanStart.y][kPacmanStart.x] == '.') { pelletsRemaining_--; }
        grid_[kPacmanStart.y][kPacmanStart.x] = ' ';
    }
    for (const auto& ghostStart : kGhostStartTiles) {
        if (ghostStart.y >= 0 && ghostStart.y < gridHeight() && ghostStart.x >= 0 && ghostStart.x < gridWidth()) {
            if (grid_[ghostStart.y][ghostStart.x] == '.') { pelletsRemaining_--; }
            grid_[ghostStart.y][ghostStart.x] = ' ';
        }
    }
}

void PacMan::resetCharacters() {
    resetAfterDeath();
}

void PacMan::resetAfterDeath() {
    pacmanPos_ = tileCenter(kPacmanStart.x, kPacmanStart.y);
    pacmanDir_ = DIR_LEFT;
    pacmanNextDir_ = DIR_LEFT;
    pacmanAlive_ = true;
    deathTimer_ = 0.0f;
    powerTimer_ = 0.0f;

    ghosts_.clear();
    for (size_t i = 0; i < kGhostStartTiles.size(); ++i) {
        Ghost ghost;
        ghost.spawnTile = { static_cast<float>(kGhostStartTiles[i].x), static_cast<float>(kGhostStartTiles[i].y) };
        ghost.scatterTile = { static_cast<float>(kScatterCorners[i].x), static_cast<float>(kScatterCorners[i].y) };
        ghost.pos = tileCenter(kGhostStartTiles[i].x, kGhostStartTiles[i].y);
        ghost.dir = (i == 0) ? DIR_LEFT : ((i == 1) ? DIR_RIGHT : DIR_UP);
        ghost.mode = GhostMode::Scatter;
        ghost.modeTimer = 0.0f;
        ghost.frightenedTimer = 0.0f;
        ghost.color = kGhostColors[i];
        ghost.eyesOnly = false;
        ghosts_.push_back(ghost);
    }

    scatterPhase_ = true;
    globalModeTimer_ = 7.0f;
}

void PacMan::update(float dt, int width, int height, bool acceptInput) {
    width_ = width;
    height_ = height;

    if ((victory_ || gameOver_) && acceptInput && IsKeyPressed(KEY_ENTER)) {
        setup(width_, height_);
        return;
    }

    if (!pacmanAlive_) {
        deathTimer_ -= dt;
        if (deathTimer_ <= 0.0f) {
            if (lives_ > 0 && !gameOver_) {
                resetAfterDeath();
            } else {
                gameOver_ = true;
            }
        }
    }

    if (pacmanAlive_ && !victory_) {
        updatePacman(dt, acceptInput);
        updateGhosts(dt);
        handlePellets();
        handleCollisions();
    } else if (!pacmanAlive_) {
        updateGhosts(dt);
    }

    if (pelletsRemaining_ <= 0 && !victory_) {
        victory_ = true;
    }

    powerTimer_ = std::max(0.0f, powerTimer_ - dt);
}

void PacMan::updatePacman(float dt, bool acceptInput) {
    if (!pacmanAlive_) return;

    if (acceptInput) {
        if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) pacmanNextDir_ = DIR_UP;
        if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) pacmanNextDir_ = DIR_DOWN;
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) pacmanNextDir_ = DIR_LEFT;
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) pacmanNextDir_ = DIR_RIGHT;
    }

    Vector2 gridPos = worldToGrid(pacmanPos_);
    int gx = static_cast<int>(gridPos.x);
    int gy = static_cast<int>(gridPos.y);
    Vector2 center = tileCenter(gx, gy);
    float centerTolerance = pacmanSpeed_ * dt + 0.5f;

    if ((pacmanNextDir_.x != pacmanDir_.x || pacmanNextDir_.y != pacmanDir_.y) && canTurn(pacmanPos_, pacmanNextDir_, false)) {
        if (Vector2Distance(pacmanPos_, center) <= centerTolerance) {
            pacmanPos_ = center;
            pacmanDir_ = pacmanNextDir_;
        }
    }

    if (pacmanDir_.x == 0.0f && pacmanDir_.y == 0.0f) {
        if (canTurn(pacmanPos_, pacmanNextDir_, false)) {
            pacmanDir_ = pacmanNextDir_;
        }
    }

    if (pacmanDir_.x != 0.0f || pacmanDir_.y != 0.0f) {
        Vector2 proposed = Vector2Add(pacmanPos_, Vector2Scale(pacmanDir_, pacmanSpeed_ * dt));
        Vector2 aheadTile = worldToGrid(Vector2Add(proposed, Vector2Scale(pacmanDir_, tileSize_ * 0.2f)));
        int nextGX = static_cast<int>(aheadTile.x);
        int nextGY = static_cast<int>(aheadTile.y);
        if (!isWalkable(nextGX, nextGY, false)) {
            pacmanPos_ = tileCenter(gx, gy);
            pacmanDir_ = { 0.0f, 0.0f };
        } else {
            pacmanPos_ = proposed;
        }
    }

    pacmanPos_ = wrapPosition(pacmanPos_);
}

void PacMan::updateGhosts(float dt) {
    if (ghosts_.empty()) return;

    if (!victory_ && pacmanAlive_) {
        globalModeTimer_ -= dt;
        if (globalModeTimer_ <= 0.0f) {
            scatterPhase_ = !scatterPhase_;
            globalModeTimer_ = scatterPhase_ ? 6.0f : 20.0f;
        }
    }

    for (auto& ghost : ghosts_) {
        updateGhost(ghost, dt);
    }
}

void PacMan::updateGhost(Ghost& ghost, float dt) {
    Vector2 currentGrid = worldToGrid(ghost.pos);
    int gx = static_cast<int>(currentGrid.x);
    int gy = static_cast<int>(currentGrid.y);
    Vector2 center = tileCenter(gx, gy);

    float speed = pacmanSpeed_ * 0.92f;
    if (ghost.mode == GhostMode::Frightened && !ghost.eyesOnly) {
        ghost.frightenedTimer = std::max(0.0f, ghost.frightenedTimer - dt);
        if (ghost.frightenedTimer <= 0.0f) {
            ghost.mode = scatterPhase_ ? GhostMode::Scatter : GhostMode::Chase;
            ghost.eyesOnly = false;
        }
        speed *= 0.65f;
    } else if (ghost.mode == GhostMode::Returning) {
        speed *= 1.45f;
    } else {
        speed *= 1.0f;
    }

    bool allowGate = (ghost.mode == GhostMode::Returning);

    std::vector<Vector2> options = availableDirections(ghost.pos, ghost.dir, allowGate);

    if (!options.empty()) {
        // Avoid immediate reversal if possible
        std::vector<Vector2> filtered;
        for (const auto& dir : options) {
            if (ghost.dir.x != 0.0f || ghost.dir.y != 0.0f) {
                if (dir.x == -ghost.dir.x && dir.y == -ghost.dir.y && options.size() > 1) {
                    continue;
                }
            }
            filtered.push_back(dir);
        }
        if (filtered.empty()) filtered = options;

        Vector2 chosenDir = ghost.dir;
        if (ghost.mode == GhostMode::Frightened && !ghost.eyesOnly) {
            int idx = GetRandomValue(0, static_cast<int>(filtered.size()) - 1);
            chosenDir = filtered[static_cast<size_t>(idx)];
        } else {
            Vector2 target = pacmanPos_;
            if (ghost.mode == GhostMode::Scatter) {
                target = tileCenter(static_cast<int>(ghost.scatterTile.x), static_cast<int>(ghost.scatterTile.y));
            } else if (ghost.mode == GhostMode::Returning) {
                target = tileCenter(static_cast<int>(ghost.spawnTile.x), static_cast<int>(ghost.spawnTile.y));
            }
            float best = std::numeric_limits<float>::max();
            for (const auto& dir : filtered) {
                int nx = gx + static_cast<int>(dir.x);
                int ny = gy + static_cast<int>(dir.y);
                Vector2 candidateCenter = tileCenter(nx, ny);
                float dist = distanceSquared(candidateCenter, target);
                if (dist < best) {
                    best = dist;
                    chosenDir = dir;
                }
            }
        }
        ghost.dir = normalizeDir(chosenDir);
    }

    ghost.pos = Vector2Add(ghost.pos, Vector2Scale(ghost.dir, speed * dt));
    ghost.pos = wrapPosition(ghost.pos);

    if (Vector2Distance(ghost.pos, center) < speed * dt * 0.8f) {
        ghost.pos = center;
    }

    if (ghost.mode == GhostMode::Returning) {
        Vector2 target = tileCenter(static_cast<int>(ghost.spawnTile.x), static_cast<int>(ghost.spawnTile.y));
        if (Vector2Distance(ghost.pos, target) < tileSize_ * 0.2f) {
            reviveGhost(ghost);
        }
    }
}

void PacMan::reviveGhost(Ghost& ghost) {
    ghost.mode = scatterPhase_ ? GhostMode::Scatter : GhostMode::Chase;
    ghost.eyesOnly = false;
    ghost.frightenedTimer = 0.0f;
    ghost.pos = tileCenter(static_cast<int>(ghost.spawnTile.x), static_cast<int>(ghost.spawnTile.y));
    ghost.dir = DIR_UP;
}

void PacMan::handlePellets() {
    Vector2 gridPos = worldToGrid(pacmanPos_);
    int gx = static_cast<int>(gridPos.x);
    int gy = static_cast<int>(gridPos.y);
    if (gx < 0 || gx >= gridWidth() || gy < 0 || gy >= gridHeight()) return;

    char& tile = grid_[gy][gx];
    if (tile == '.') {
        tile = ' ';
        score_ += 10;
        pelletsRemaining_ = std::max(0, pelletsRemaining_ - 1);
    } else if (tile == 'o') {
        tile = ' ';
        score_ += 50;
        pelletsRemaining_ = std::max(0, pelletsRemaining_ - 1);
        powerTimer_ = 6.0f;
        enterFrightenedMode();
    }
}

void PacMan::enterFrightenedMode() {
    for (auto& ghost : ghosts_) {
        if (ghost.mode == GhostMode::Returning) continue;
        ghost.mode = GhostMode::Frightened;
        ghost.frightenedTimer = powerTimer_;
        ghost.eyesOnly = false;
    }
}

void PacMan::handleCollisions() {
    if (!pacmanAlive_) return;

    for (auto& ghost : ghosts_) {
        float dist = Vector2Distance(ghost.pos, pacmanPos_);
        if (dist <= tileSize_ * 0.45f) {
            if (ghost.mode == GhostMode::Frightened && !ghost.eyesOnly) {
                ghost.mode = GhostMode::Returning;
                ghost.eyesOnly = true;
                ghost.frightenedTimer = 0.0f;
                score_ += 200;
            } else if (ghost.mode != GhostMode::Returning) {
                pacmanAlive_ = false;
                pacmanDir_ = {0.0f, 0.0f};
                pacmanNextDir_ = {0.0f, 0.0f};
                deathTimer_ = 1.5f;
                if (lives_ > 0) {
                    lives_ -= 1;
                    if (lives_ < 0) lives_ = 0;
                }
                if (lives_ <= 0) {
                    gameOver_ = true;
                }
                break;
            }
        }
    }
}

void PacMan::render(int /*width*/, int /*height*/) {
    ClearBackground(Color{10, 10, 24, 255});

    // Draw maze
    for (int y = 0; y < gridHeight(); ++y) {
        for (int x = 0; x < gridWidth(); ++x) {
            char tile = grid_[y][x];
            Rectangle cell{ offset_.x + x * tileSize_, offset_.y + y * tileSize_, static_cast<float>(tileSize_), static_cast<float>(tileSize_) };
            if (tile == '#') {
                DrawRectangleRounded(cell, 0.4f, 6, Color{30, 30, 130, 255});
            } else if (tile == '.') {
                DrawCircle(cell.x + cell.width * 0.5f, cell.y + cell.height * 0.5f, std::max(2.0f, cell.width * 0.12f), Color{255, 220, 120, 255});
            } else if (tile == 'o') {
                DrawCircle(cell.x + cell.width * 0.5f, cell.y + cell.height * 0.5f, std::max(4.0f, cell.width * 0.25f), Color{255, 240, 140, 255});
            }
        }
    }

    // Draw Pac-Man
    float radius = tileSize_ * 0.45f;
    if (pacmanAlive_) {
        Vector2 dir = pacmanDir_;
        if (dir.x == 0.0f && dir.y == 0.0f) dir = DIR_RIGHT;
        float heading = 0.0f;
        if (dir.x > 0.5f) heading = 0.0f;
        else if (dir.x < -0.5f) heading = 180.0f;
        else if (dir.y > 0.5f) heading = 90.0f;
        else if (dir.y < -0.5f) heading = 270.0f;
        float mouthAnim = (std::sinf(GetTime() * 6.0f) * 0.5f + 0.5f);
        float mouth = 28.0f + mouthAnim * 10.0f;
        DrawCircleSector(pacmanPos_, radius, heading - mouth, heading + mouth, 32, Color{255, 252, 0, 255});
    } else {
        float collapse = std::max(0.0f, deathTimer_ / 1.5f);
        DrawCircleV(pacmanPos_, radius * collapse, Color{255, 252, 0, 255});
    }

    // Draw ghosts
    for (const auto& ghost : ghosts_) {
        Vector2 pos = ghost.pos;
        float bodyRadius = tileSize_ * 0.42f;
        Color bodyColor = ghost.color;
        if (ghost.mode == GhostMode::Frightened && !ghost.eyesOnly) {
            bool blink = ghost.frightenedTimer < 2.0f && (static_cast<int>(GetTime() * 6.0f) % 2 == 0);
            bodyColor = blink ? Color{220, 220, 255, 255} : Color{70, 70, 255, 255};
        }
        if (ghost.eyesOnly) {
            bodyColor = Color{220, 220, 255, 200};
        }

        DrawCircleV({ pos.x, pos.y - bodyRadius * 0.2f }, bodyRadius, bodyColor);
        Rectangle skirt{ pos.x - bodyRadius, pos.y - bodyRadius * 0.2f, bodyRadius * 2.0f, bodyRadius * 1.2f };
        DrawRectangleRounded(skirt, 0.6f, 8, bodyColor);

        Color eyeWhite = RAYWHITE;
        Color pupilColor = (ghost.mode == GhostMode::Frightened && !ghost.eyesOnly) ? Color{0, 0, 160, 255} : Color{20, 20, 60, 255};
        Vector2 eyeDir = normalizeDir(ghost.dir);
        Vector2 eyeOffset = Vector2Scale(eyeDir, bodyRadius * 0.25f);
        Vector2 leftEye = { pos.x - bodyRadius * 0.35f + eyeOffset.x, pos.y - bodyRadius * 0.25f + eyeOffset.y };
        Vector2 rightEye = { pos.x + bodyRadius * 0.35f + eyeOffset.x, pos.y - bodyRadius * 0.25f + eyeOffset.y };
        DrawCircleV(leftEye, bodyRadius * 0.28f, eyeWhite);
        DrawCircleV(rightEye, bodyRadius * 0.28f, eyeWhite);
        DrawCircleV(leftEye, bodyRadius * 0.12f, pupilColor);
        DrawCircleV(rightEye, bodyRadius * 0.12f, pupilColor);
    }

    // HUD
    std::string scoreText = "Score: " + std::to_string(score_);
    DrawText(scoreText.c_str(), 16, 16, 22, RAYWHITE);
    std::string livesText = "Lives: " + std::to_string(std::max(0, lives_));
    int textWidth = MeasureText(livesText.c_str(), 22);
    DrawText(livesText.c_str(), width_ - textWidth - 16, 16, 22, RAYWHITE);

    if (victory_) {
        const char* msg = "Level clear! Press Enter";
        int w = MeasureText(msg, 26);
        DrawText(msg, width_ / 2 - w / 2, height_ / 2 - 20, 26, Color{255, 255, 0, 255});
    } else if (gameOver_) {
        const char* msg = "Game Over - Press Enter";
        int w = MeasureText(msg, 26);
        DrawText(msg, width_ / 2 - w / 2, height_ / 2 - 20, 26, RED);
    } else if (powerTimer_ > 0.0f) {
        const char* msg = "Power!";
        int w = MeasureText(msg, 22);
        DrawText(msg, width_ / 2 - w / 2, static_cast<int>(offset_.y) - 26, 22, Color{120, 210, 255, 255});
    }
}

int PacMan::gridWidth() const {
    return grid_.empty() ? 0 : static_cast<int>(grid_.front().size());
}

int PacMan::gridHeight() const {
    return static_cast<int>(grid_.size());
}

Vector2 PacMan::tileCenter(int gx, int gy) const {
    if (grid_.empty()) return {0.0f, 0.0f};
    int cols = gridWidth();
    int rows = gridHeight();
    int x = ((gx % cols) + cols) % cols;
    int y = std::clamp(gy, 0, rows - 1);
    return { offset_.x + (x + 0.5f) * tileSize_, offset_.y + (y + 0.5f) * tileSize_ };
}

Vector2 PacMan::wrapPosition(Vector2 pos) const {
    if (grid_.empty()) return pos;
    float span = static_cast<float>(tileSize_ * gridWidth());
    if (pos.x < offset_.x - tileSize_ * 0.5f) pos.x += span;
    if (pos.x > offset_.x + span - tileSize_ * 0.5f) pos.x -= span;
    return pos;
}

bool PacMan::isGate(int gx, int gy) const {
    if (grid_.empty()) return false;
    if (gy < 0 || gy >= gridHeight()) return false;
    int cols = gridWidth();
    int x = ((gx % cols) + cols) % cols;
    return grid_[gy][x] == '-';
}

bool PacMan::isWall(int gx, int gy) const {
    if (grid_.empty()) return true;
    if (gy < 0 || gy >= gridHeight()) return true;
    int cols = gridWidth();
    int x = ((gx % cols) + cols) % cols;
    return grid_[gy][x] == '#';
}

bool PacMan::isWalkable(int gx, int gy, bool allowGate) const {
    if (grid_.empty()) return false;
    if (gy < 0 || gy >= gridHeight()) return false;
    int cols = gridWidth();
    int x = ((gx % cols) + cols) % cols;
    char tile = grid_[gy][x];
    if (tile == '#') return false;
    if (!allowGate && tile == '-') return false;
    return true;
}

std::vector<Vector2> PacMan::availableDirections(Vector2 pos, Vector2 currentDir, bool allowGate) const {
    (void)currentDir;
    std::vector<Vector2> dirs;
    Vector2 gridPos = worldToGrid(pos);
    int gx = static_cast<int>(gridPos.x);
    int gy = static_cast<int>(gridPos.y);
    for (const auto& dir : kDirections) {
        int nx = gx + static_cast<int>(dir.x);
        int ny = gy + static_cast<int>(dir.y);
        if (isWalkable(nx, ny, allowGate)) {
            dirs.push_back(dir);
        }
    }
    return dirs;
}

bool PacMan::canTurn(Vector2 pos, Vector2 desiredDir, bool allowGate) const {
    if (desiredDir.x == 0.0f && desiredDir.y == 0.0f) return false;
    Vector2 gridPos = worldToGrid(pos);
    int gx = static_cast<int>(gridPos.x);
    int gy = static_cast<int>(gridPos.y);
    Vector2 center = tileCenter(gx, gy);
    if (Vector2Distance(pos, center) > tileSize_ * 0.4f) return false;
    int nx = gx + static_cast<int>(desiredDir.x);
    int ny = gy + static_cast<int>(desiredDir.y);
    return isWalkable(nx, ny, allowGate);
}

Vector2 PacMan::worldToGrid(Vector2 pos) const {
    if (grid_.empty()) return {0.0f, 0.0f};
    float gx = (pos.x - offset_.x) / static_cast<float>(tileSize_);
    float gy = (pos.y - offset_.y) / static_cast<float>(tileSize_);
    return { std::floor(gx), std::floor(gy) };
}

Vector2 PacMan::normalizeDir(Vector2 dir) const {
    Vector2 result{0.0f, 0.0f};
    if (std::fabs(dir.x) > std::fabs(dir.y)) {
        result.x = (dir.x > 0.0f) ? 1.0f : -1.0f;
    } else if (std::fabs(dir.y) > 0.0f) {
        result.y = (dir.y > 0.0f) ? 1.0f : -1.0f;
    }
    return result;
}

float PacMan::distanceSquared(Vector2 a, Vector2 b) const {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

} // namespace gb2d::games
