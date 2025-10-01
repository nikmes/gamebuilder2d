#pragma once
#include "games/Game.h"
#include <raylib.h>
#include <vector>
#include <string>

namespace gb2d::games {

class PacMan final : public Game {
public:
    PacMan() = default;
    ~PacMan() override = default;

    const char* id() const override { return "pac-man"; }
    const char* name() const override { return "Pac-Man"; }

    void init(int width, int height) override;
    void update(float dt, int width, int height, bool acceptInput) override;
    void render(int width, int height) override;
    void unload() override;
    void onResize(int width, int height) override;
    void reset(int width, int height) override;

private:
    enum class GhostMode { Scatter, Chase, Frightened, Returning };

    struct Ghost {
        Vector2 pos{};
        Vector2 dir{};
        Vector2 spawnTile{};
        Vector2 scatterTile{};
        GhostMode mode{GhostMode::Scatter};
        float modeTimer{0.0f};
        float frightenedTimer{0.0f};
        Color color{YELLOW};
        bool eyesOnly{false};
    };

    void setup(int width, int height);
    void rebuildGrid();
    void resetCharacters();
    void updatePacman(float dt, bool acceptInput);
    void updateGhosts(float dt);
    void updateGhost(Ghost& ghost, float dt);
    void handlePellets();
    void handleCollisions();
    void enterFrightenedMode();
    void reviveGhost(Ghost& ghost);
    void resetAfterDeath();

    Vector2 tileCenter(int gx, int gy) const;
    Vector2 wrapPosition(Vector2 pos) const;
    bool isWall(int gx, int gy) const;
    bool isGate(int gx, int gy) const;
    bool isWalkable(int gx, int gy, bool allowGate) const;
    std::vector<Vector2> availableDirections(Vector2 pos, Vector2 currentDir, bool allowGate) const;
    bool canTurn(Vector2 pos, Vector2 desiredDir, bool allowGate) const;
    Vector2 worldToGrid(Vector2 pos) const;
    Vector2 normalizeDir(Vector2 dir) const;
    float distanceSquared(Vector2 a, Vector2 b) const;

    int gridWidth() const;
    int gridHeight() const;

    int width_{0};
    int height_{0};
    int tileSize_{18};
    Vector2 offset_{};

    std::vector<std::string> grid_{};

    Vector2 pacmanPos_{};
    Vector2 pacmanDir_{};
    Vector2 pacmanNextDir_{};
    float pacmanSpeed_{90.0f};
    int pelletsRemaining_{0};
    int score_{0};
    int lives_{3};
    bool pacmanAlive_{true};
    bool victory_{false};
    bool gameOver_{false};
    float deathTimer_{0.0f};
    float powerTimer_{0.0f};

    std::vector<Ghost> ghosts_{};
    float globalModeTimer_{7.0f};
    bool scatterPhase_{true};
};

} // namespace gb2d::games
