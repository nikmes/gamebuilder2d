#pragma once
#include "ui/Windows/Games/Game.h"
#include <array>
#include <vector>
#include <raylib.h>

namespace gb2d::games {

class Galaga final : public Game {
public:
    Galaga() = default;
    ~Galaga() override = default;

    const char* id() const override { return "galaga"; }
    const char* name() const override { return "Galaga"; }

    void init(int width, int height) override;
    void update(float dt, int width, int height, bool acceptInput) override;
    void render(int width, int height) override;
    void unload() override;
    void onResize(int width, int height) override;
    void reset(int width, int height) override;

private:
    struct Shot {
        Vector2 pos{};
        Vector2 vel{};
        bool alive{true};
    };

    struct Star {
        Vector2 pos{};
        float speed{20.0f};
        float scale{1.0f};
    };

    enum class EnemyState { Entering, Formation, Diving, Returning };

    struct Enemy {
        bool alive{true};
        EnemyState state{EnemyState::Entering};
        Vector2 pos{};
        Vector2 formationPos{};
        std::array<Vector2, 4> path{};
        float pathT{0.0f};
        float pathSpeed{0.8f};
        float bobPhase{0.0f};
        int row{0};
        bool hasShot{false};
        float heading{-PI * 0.5f};
    };

    struct Player {
        Vector2 pos{};
        float speed{360.0f};
        float cooldown{0.0f};
        int lives{3};
        bool alive{true};
        float respawnTimer{0.0f};
        float invulnTimer{1.0f};
    };

    void setup(int width, int height);
    void setupFormation();
    void regenerateStarfield();
    void assignEntryPath(Enemy& enemy, int row, int col, int columns);
    void assignDivePath(Enemy& enemy);
    void assignReturnPath(Enemy& enemy);
    void updatePlayer(float dt, bool acceptInput);
    void updatePlayerBullets(float dt);
    void updateEnemyBullets(float dt);
    void updateEnemies(float dt);
    void updateStarfield(float dt);
    void spawnDive();
    void handleCollisions();
    void handlePlayerHit();
    bool anyFormationEnemies() const;

    static Vector2 evalBezier(const std::array<Vector2, 4>& path, float t);
    static float randomFloat(float minValue, float maxValue);

    int width_{0};
    int height_{0};

    Player player_{};
    std::vector<Shot> player_bullets_{};
    std::vector<Shot> enemy_bullets_{};
    std::vector<Enemy> enemies_{};
    std::vector<Star> stars_{};

    float dive_timer_{2.5f};
    float dive_interval_min_{2.0f};
    float dive_interval_max_{4.0f};
    int score_{0};
    bool victory_{false};
    bool game_over_{false};
};

} // namespace gb2d::games
