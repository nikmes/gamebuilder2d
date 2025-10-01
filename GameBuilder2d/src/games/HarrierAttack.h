#pragma once
#include "games/Game.h"
#include <raylib.h>
#include <raymath.h>
#include <array>
#include <string>
#include <vector>

namespace gb2d::games {

class HarrierAttack final : public Game {
public:
    HarrierAttack() = default;
    ~HarrierAttack() override = default;

    const char* id() const override { return "harrier-attack"; }
    const char* name() const override { return "Harrier Attack"; }

    void init(int width, int height) override;
    void update(float dt, int width, int height, bool acceptInput) override;
    void render(int width, int height) override;
    void unload() override;
    void onResize(int width, int height) override;
    void reset(int width, int height) override;

private:
    struct Difficulty {
        std::string label;
        float fuelConsumption;
        float enemySpawnInterval;
        float groundFireInterval;
        float enemySpeedMultiplier;
        float fuelReserve;
    };

    struct Player {
        Vector2 pos{};
        Vector2 vel{};
        float fuel{0.0f};
        int bombs{10};
        int rockets{6};
        bool alive{true};
        bool landed{false};
        bool missionComplete{false};
        float invuln{1.0f};
        float bombCooldown{0.0f};
        float rocketCooldown{0.0f};
    };

    struct Bomb {
        Vector2 pos{};
        Vector2 vel{};
        bool alive{true};
    };

    struct Rocket {
        Vector2 pos{};
        Vector2 vel{};
        bool alive{true};
    };

    struct EnemyJet {
        Vector2 pos{};
        Vector2 vel{};
        bool alive{true};
        float fireTimer{0.0f};
    };

    struct EnemyShot {
        Vector2 pos{};
        Vector2 vel{};
        bool alive{true};
    };

    struct GroundTarget {
        Rectangle rect{};
        bool alive{true};
        float fireTimer{0.0f};
    };

    void configureWorld(int width, int height);
    void rebuildEntities();
    void updatePlayer(float dt, bool acceptInput);
    void updateBombs(float dt);
    void updateRockets(float dt);
    void updateEnemyJets(float dt);
    void updateEnemyShots(float dt);
    void updateGroundBatteries(float dt);
    void handleCollisions();
    void spawnEnemyJet();
    void spawnGroundShot(GroundTarget& target);
    void handleLanding();
    void checkMissionState();
    bool allTargetsDestroyed() const;
    Vector2 toScreen(Vector2 world) const;
    float cameraX() const;
    void setStatusMessage(const std::string& msg, float duration = 3.0f);

    static float randomFloat(float minValue, float maxValue);

    int width_{0};
    int height_{0};
    float groundY_{0.0f};
    float worldWidth_{2000.0f};
    float carrierStart_{0.0f};
    float carrierEnd_{220.0f};
    float islandStart_{600.0f};
    float islandEnd_{1400.0f};

    Player player_{};
    std::vector<Bomb> bombs_{};
    std::vector<Rocket> rockets_{};
    std::vector<EnemyJet> enemyJets_{};
    std::vector<EnemyShot> enemyShots_{};
    std::vector<GroundTarget> groundTargets_{};

    int difficultyIndex_{0};
    std::array<Difficulty,5> difficulties_{};
    float enemySpawnTimer_{0.0f};
    float statusMessageTimer_{0.0f};
    bool missionFailed_{false};
    int score_{0};
    std::string statusMessage_{};
};

} // namespace gb2d::games
