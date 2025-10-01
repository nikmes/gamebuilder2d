#include "games/HarrierAttack.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace gb2d::games {

namespace {
constexpr float kGravity = 260.0f;
constexpr float kMaxAltitude = 80.0f;
constexpr float kSafeLandingSpeed = 65.0f;
}

float HarrierAttack::randomFloat(float minValue, float maxValue) {
    if (maxValue <= minValue) return minValue;
    constexpr int span = 1000;
    float r = static_cast<float>(GetRandomValue(0, span)) / static_cast<float>(span);
    return minValue + (maxValue - minValue) * r;
}

void HarrierAttack::setStatusMessage(const std::string& msg, float duration) {
    statusMessage_ = msg;
    statusMessageTimer_ = duration;
}

void HarrierAttack::configureWorld(int width, int height) {
    width_ = width;
    height_ = height;
    groundY_ = static_cast<float>(height_) - 80.0f;
    worldWidth_ = std::max(1800.0f, width_ * 2.0f);
    carrierStart_ = 0.0f;
    carrierEnd_ = carrierStart_ + 240.0f;
    islandStart_ = std::max(520.0f, carrierEnd_ + 280.0f);
    islandEnd_ = std::min(worldWidth_ - 120.0f, islandStart_ + 880.0f);

    difficulties_ = {
        Difficulty{ "Cadet",   0.016f, 8.0f, 6.0f, 0.85f, 240.0f },
        Difficulty{ "Pilot",   0.018f, 6.6f, 5.2f, 1.00f, 220.0f },
        Difficulty{ "Veteran", 0.021f, 5.4f, 4.4f, 1.12f, 205.0f },
        Difficulty{ "Ace",     0.025f, 4.6f, 3.6f, 1.25f, 190.0f },
        Difficulty{ "Legend",  0.030f, 3.8f, 2.9f, 1.38f, 175.0f }
    };
}

void HarrierAttack::rebuildEntities() {
    difficultyIndex_ = std::clamp(difficultyIndex_, 0, static_cast<int>(difficulties_.size()) - 1);
    player_ = Player{};
    const auto& diff = difficulties_[difficultyIndex_];
    player_.pos = { carrierStart_ + 80.0f, groundY_ - 120.0f };
    player_.vel = { 0.0f, 0.0f };
    player_.fuel = diff.fuelReserve;
    player_.bombs = 10;
    player_.rockets = 6;
    player_.alive = true;
    player_.landed = false;
    player_.missionComplete = false;
    player_.invuln = 2.0f;
    player_.bombCooldown = 0.0f;
    player_.rocketCooldown = 0.0f;

    bombs_.clear();
    rockets_.clear();
    enemyJets_.clear();
    enemyShots_.clear();

    groundTargets_.clear();
    int targetCount = 6;
    float spacing = (islandEnd_ - islandStart_) / static_cast<float>(targetCount);
    for (int i = 0; i < targetCount; ++i) {
        float x = islandStart_ + spacing * (i + 0.5f);
        GroundTarget target;
        target.rect = Rectangle{ x - 28.0f, groundY_ - 36.0f, 56.0f, 36.0f };
        target.alive = true;
        target.fireTimer = randomFloat(2.0f, 4.0f);
        groundTargets_.push_back(target);
    }

    enemySpawnTimer_ = difficulties_[difficultyIndex_].enemySpawnInterval;
    missionFailed_ = false;
    score_ = 0;
    statusMessageTimer_ = 0.0f;
    statusMessage_.clear();
}

void HarrierAttack::init(int width, int height) {
    configureWorld(width, height);
    rebuildEntities();
}

void HarrierAttack::reset(int width, int height) {
    configureWorld(width, height);
    rebuildEntities();
    setStatusMessage("Mission restarted", 2.0f);
}

void HarrierAttack::onResize(int width, int height) {
    configureWorld(width, height);
}

void HarrierAttack::unload() {
    bombs_.clear();
    rockets_.clear();
    enemyJets_.clear();
    enemyShots_.clear();
    groundTargets_.clear();
}

void HarrierAttack::update(float dt, int width, int height, bool acceptInput) {
    configureWorld(width, height);

    if (statusMessageTimer_ > 0.0f) {
        statusMessageTimer_ = std::max(0.0f, statusMessageTimer_ - dt);
        if (statusMessageTimer_ == 0.0f) statusMessage_.clear();
    }

    if (acceptInput) {
        if (IsKeyPressed(KEY_ONE)) { difficultyIndex_ = 0; rebuildEntities(); setStatusMessage("Difficulty: Cadet"); return; }
        if (IsKeyPressed(KEY_TWO)) { difficultyIndex_ = 1; rebuildEntities(); setStatusMessage("Difficulty: Pilot"); return; }
        if (IsKeyPressed(KEY_THREE)) { difficultyIndex_ = 2; rebuildEntities(); setStatusMessage("Difficulty: Veteran"); return; }
        if (IsKeyPressed(KEY_FOUR)) { difficultyIndex_ = 3; rebuildEntities(); setStatusMessage("Difficulty: Ace"); return; }
        if (IsKeyPressed(KEY_FIVE)) { difficultyIndex_ = 4; rebuildEntities(); setStatusMessage("Difficulty: Legend"); return; }
    }

    if ((missionFailed_ || player_.missionComplete) && acceptInput && IsKeyPressed(KEY_ENTER)) {
        reset(width_, height_);
        return;
    }

    if (!player_.alive) {
        missionFailed_ = true;
    }

    if (!missionFailed_ && !player_.missionComplete) {
        updatePlayer(dt, acceptInput);
        updateBombs(dt);
        updateRockets(dt);
        updateEnemyJets(dt);
        updateGroundBatteries(dt);
        updateEnemyShots(dt);
        handleCollisions();
        handleLanding();
        checkMissionState();
    } else {
        updateEnemyShots(dt);
        updateEnemyJets(dt);
    }

    player_.invuln = std::max(0.0f, player_.invuln - dt);
}

void HarrierAttack::updatePlayer(float dt, bool acceptInput) {
    const auto& diff = difficulties_[difficultyIndex_];
    Vector2 desiredVel{ 120.0f, 0.0f };
    bool boost = false;

    if (acceptInput) {
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) desiredVel.x -= 90.0f;
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) desiredVel.x += 120.0f;
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) desiredVel.y -= 180.0f;
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) desiredVel.y += 200.0f;
        boost = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        if (boost) desiredVel.x += 90.0f;

        if (IsKeyDown(KEY_SPACE) && player_.bombCooldown <= 0.0f && player_.bombs > 0) {
            Bomb bomb;
            bomb.pos = player_.pos + Vector2{ 0.0f, 18.0f };
            bomb.vel = Vector2{ player_.vel.x * 0.3f, 120.0f };
            bombs_.push_back(bomb);
            player_.bombs -= 1;
            player_.bombCooldown = 0.35f;
        }
        if ((IsKeyDown(KEY_X) || IsKeyDown(KEY_RIGHT_CONTROL)) && player_.rocketCooldown <= 0.0f && player_.rockets > 0) {
            Rocket rocket;
            rocket.pos = player_.pos + Vector2{ 26.0f, -6.0f };
            rocket.vel = Vector2{ 280.0f, 0.0f };
            rockets_.push_back(rocket);
            player_.rockets -= 1;
            player_.rocketCooldown = 0.65f;
        }
    }

    desiredVel.x = std::clamp(desiredVel.x, 40.0f, 340.0f);
    desiredVel.y = std::clamp(desiredVel.y, -220.0f, 220.0f);

    player_.vel = Vector2Lerp(player_.vel, desiredVel, std::clamp(dt * 3.0f, 0.0f, 1.0f));
    player_.pos = Vector2Add(player_.pos, Vector2Scale(player_.vel, dt));

    player_.pos.x = std::clamp(player_.pos.x, 0.0f, worldWidth_ - 10.0f);
    player_.pos.y = std::clamp(player_.pos.y, kMaxAltitude, groundY_ - 18.0f);

    float fuelUse = diff.fuelConsumption * (1.0f + (boost ? 0.9f : 0.0f) + std::fabs(player_.vel.y) / 260.0f);
    player_.fuel = std::max(0.0f, player_.fuel - fuelUse * dt);

    player_.bombCooldown = std::max(0.0f, player_.bombCooldown - dt);
    player_.rocketCooldown = std::max(0.0f, player_.rocketCooldown - dt);
}

void HarrierAttack::updateBombs(float dt) {
    for (auto& bomb : bombs_) {
        if (!bomb.alive) continue;
        bomb.vel.y += kGravity * dt * 0.6f;
        bomb.pos = Vector2Add(bomb.pos, Vector2Scale(bomb.vel, dt));
        if (bomb.pos.y >= groundY_) bomb.alive = false;
    }
    bombs_.erase(std::remove_if(bombs_.begin(), bombs_.end(), [](const Bomb& b){ return !b.alive; }), bombs_.end());
}

void HarrierAttack::updateRockets(float dt) {
    for (auto& rocket : rockets_) {
        if (!rocket.alive) continue;
        EnemyJet* target = nullptr;
        float bestDist = 1e9f;
        for (auto& jet : enemyJets_) {
            if (!jet.alive) continue;
            float dist = Vector2Distance(rocket.pos, jet.pos);
            if (dist < bestDist) {
                bestDist = dist;
                target = &jet;
            }
        }
        if (target) {
            Vector2 dir = Vector2Normalize(Vector2Subtract(target->pos, rocket.pos));
            rocket.vel = Vector2Lerp(rocket.vel, Vector2Scale(dir, 360.0f), std::clamp(dt * 3.6f, 0.0f, 1.0f));
        }
        rocket.pos = Vector2Add(rocket.pos, Vector2Scale(rocket.vel, dt));
        if (rocket.pos.x > worldWidth_ + 60.0f || rocket.pos.y < -60.0f || rocket.pos.y > height_ + 60.0f) {
            rocket.alive = false;
        }
    }
    rockets_.erase(std::remove_if(rockets_.begin(), rockets_.end(), [](const Rocket& r){ return !r.alive; }), rockets_.end());
}

void HarrierAttack::spawnEnemyJet() {
    const auto& diff = difficulties_[difficultyIndex_];
    EnemyJet jet;
    jet.pos = { std::min(worldWidth_ - 40.0f, player_.pos.x + randomFloat(480.0f, 680.0f)), randomFloat(kMaxAltitude + 40.0f, groundY_ - 150.0f) };
    float speed = 140.0f * diff.enemySpeedMultiplier;
    jet.vel = { -speed, randomFloat(-30.0f, 30.0f) };
    jet.alive = true;
    jet.fireTimer = randomFloat(1.8f, 3.0f);
    enemyJets_.push_back(jet);
}

void HarrierAttack::updateEnemyJets(float dt) {
    if (!missionFailed_ && !player_.missionComplete) {
        enemySpawnTimer_ -= dt;
        if (enemySpawnTimer_ <= 0.0f) {
            spawnEnemyJet();
            enemySpawnTimer_ = difficulties_[difficultyIndex_].enemySpawnInterval * randomFloat(0.7f, 1.3f);
        }
    }

    for (auto& jet : enemyJets_) {
        if (!jet.alive) continue;
        jet.pos = Vector2Add(jet.pos, Vector2Scale(jet.vel, dt));
        jet.pos.y = std::clamp(jet.pos.y, kMaxAltitude + 20.0f, groundY_ - 120.0f);
        jet.fireTimer -= dt;
        if (jet.fireTimer <= 0.0f && player_.alive) {
            Vector2 dir = Vector2Normalize(Vector2Subtract(player_.pos, jet.pos));
            EnemyShot shot;
            shot.pos = jet.pos;
            shot.vel = Vector2Scale(dir, 240.0f);
            enemyShots_.push_back(shot);
            jet.fireTimer = randomFloat(2.0f, 3.5f);
        }
        if (jet.pos.x < player_.pos.x - 500.0f || jet.pos.x < -120.0f) {
            jet.alive = false;
        }
    }
    enemyJets_.erase(std::remove_if(enemyJets_.begin(), enemyJets_.end(), [](const EnemyJet& e){ return !e.alive; }), enemyJets_.end());
}

void HarrierAttack::spawnGroundShot(GroundTarget& target) {
    if (!player_.alive) return;
    EnemyShot shot;
    shot.pos = { target.rect.x + target.rect.width * 0.5f, target.rect.y };
    Vector2 dir = Vector2Normalize(Vector2Subtract(player_.pos, shot.pos));
    shot.vel = Vector2Scale(dir, 220.0f);
    enemyShots_.push_back(shot);
}

void HarrierAttack::updateGroundBatteries(float dt) {
    const auto& diff = difficulties_[difficultyIndex_];
    for (auto& target : groundTargets_) {
        if (!target.alive) continue;
        target.fireTimer -= dt;
        if (target.fireTimer <= 0.0f && player_.pos.x > islandStart_ - 80.0f) {
            spawnGroundShot(target);
            target.fireTimer = diff.groundFireInterval * randomFloat(0.8f, 1.2f);
        }
    }
}

void HarrierAttack::updateEnemyShots(float dt) {
    for (auto& shot : enemyShots_) {
        if (!shot.alive) continue;
        shot.pos = Vector2Add(shot.pos, Vector2Scale(shot.vel, dt));
        if (shot.pos.x < -120.0f || shot.pos.x > worldWidth_ + 120.0f || shot.pos.y < -120.0f || shot.pos.y > height_ + 120.0f) {
            shot.alive = false;
        }
    }
    enemyShots_.erase(std::remove_if(enemyShots_.begin(), enemyShots_.end(), [](const EnemyShot& s){ return !s.alive; }), enemyShots_.end());
}

bool HarrierAttack::allTargetsDestroyed() const {
    for (const auto& target : groundTargets_) {
        if (target.alive) return false;
    }
    return true;
}

void HarrierAttack::handleCollisions() {
    for (auto& bomb : bombs_) {
        if (!bomb.alive) continue;
        for (auto& target : groundTargets_) {
            if (!target.alive) continue;
            if (CheckCollisionPointRec(bomb.pos, target.rect)) {
                bomb.alive = false;
                target.alive = false;
                score_ += 500;
                setStatusMessage("Target destroyed", 1.6f);
                break;
            }
        }
    }

    for (auto& rocket : rockets_) {
        if (!rocket.alive) continue;
        for (auto& jet : enemyJets_) {
            if (!jet.alive) continue;
            if (Vector2Distance(rocket.pos, jet.pos) < 26.0f) {
                rocket.alive = false;
                jet.alive = false;
                score_ += 200;
                setStatusMessage("Enemy jet down", 1.6f);
                break;
            }
        }
    }

    if (player_.alive && player_.invuln <= 0.0f) {
        for (auto& shot : enemyShots_) {
            if (!shot.alive) continue;
            if (Vector2Distance(shot.pos, player_.pos) < 22.0f) {
                player_.alive = false;
                missionFailed_ = true;
                setStatusMessage("Hit by enemy fire", 2.5f);
                break;
            }
        }
    }

    if (player_.alive && player_.invuln <= 0.0f) {
        for (auto& jet : enemyJets_) {
            if (!jet.alive) continue;
            if (Vector2Distance(jet.pos, player_.pos) < 32.0f) {
                player_.alive = false;
                missionFailed_ = true;
                setStatusMessage("Collision with enemy jet", 2.5f);
                break;
            }
        }
    }

    if (player_.alive && player_.pos.y >= groundY_ - 4.0f) {
        if (player_.pos.x < carrierStart_ + 20.0f || player_.pos.x > carrierEnd_ - 20.0f || std::fabs(player_.vel.y) > kSafeLandingSpeed * 1.25f) {
            player_.alive = false;
            missionFailed_ = true;
            setStatusMessage("Aircraft lost", 2.5f);
        }
    }
}

void HarrierAttack::handleLanding() {
    if (!player_.alive || missionFailed_) return;
    bool overCarrier = player_.pos.x >= carrierStart_ + 30.0f && player_.pos.x <= carrierEnd_ - 30.0f;
    bool nearDeck = player_.pos.y >= groundY_ - 18.0f;
    bool slowVertical = std::fabs(player_.vel.y) <= kSafeLandingSpeed;
    bool slowHorizontal = std::fabs(player_.vel.x) <= 220.0f;

    if (overCarrier && nearDeck && slowVertical && slowHorizontal) {
        player_.landed = true;
        player_.vel = { 0.0f, 0.0f };
        if (allTargetsDestroyed()) {
            player_.missionComplete = true;
            setStatusMessage("Mission accomplished", 3.0f);
        } else {
            setStatusMessage("Refuel & rearm", 2.0f);
        }
    }
}

void HarrierAttack::checkMissionState() {
    if (player_.fuel <= 0.0f && player_.alive) {
        player_.alive = false;
        missionFailed_ = true;
        setStatusMessage("Fuel exhausted", 2.5f);
    }

    if (player_.missionComplete) {
        missionFailed_ = false;
    }

    if (!player_.alive && player_.fuel <= 0.0f) {
        setStatusMessage("Fuel exhausted", 2.5f);
    }
}

Vector2 HarrierAttack::toScreen(Vector2 world) const {
    float cam = cameraX();
    return { world.x - cam, world.y };
}

float HarrierAttack::cameraX() const {
    float cam = player_.pos.x - width_ * 0.35f;
    cam = std::clamp(cam, 0.0f, std::max(0.0f, worldWidth_ - width_));
    return cam;
}

void HarrierAttack::render(int width, int height) {
    (void)height;
    configureWorld(width, height);

    ClearBackground(Color{ 10, 14, 32, 255 });

    float camX = cameraX();

    Rectangle groundRect{ -camX, groundY_, worldWidth_, height_ - groundY_ };
    DrawRectangleRec(groundRect, Color{ 40, 120, 60, 255 });

    DrawRectangle(-static_cast<int>(camX) + static_cast<int>(carrierStart_), static_cast<int>(groundY_ - 40.0f), 220, 40, Color{ 60, 60, 80, 255 });
    DrawRectangleLines(-static_cast<int>(camX) + static_cast<int>(carrierStart_), static_cast<int>(groundY_ - 40.0f), 220, 40, Color{ 180, 180, 220, 255 });

    for (float x = islandStart_; x < islandEnd_; x += 32.0f) {
        DrawRectangle(static_cast<int>(x - camX), static_cast<int>(groundY_ - 30.0f), 28, 30, Color{ 70, 110, 50, 255 });
    }

    DrawRectangle(static_cast<int>(player_.pos.x - camX - 18.0f), static_cast<int>(player_.pos.y - 12.0f), 36, 10, Color{ 190, 190, 220, 255 });
    DrawTriangle(Vector2{ player_.pos.x - camX + 18.0f, player_.pos.y - 12.0f }, Vector2{ player_.pos.x - camX + 26.0f, player_.pos.y - 4.0f }, Vector2{ player_.pos.x - camX + 18.0f, player_.pos.y + 4.0f }, Color{ 220, 220, 80, 255 });

    for (const auto& bomb : bombs_) {
        if (!bomb.alive) continue;
        Vector2 screenPos = toScreen(bomb.pos);
        DrawCircleV({screenPos.x - camX, screenPos.y}, 4.0f, Color{ 240, 200, 120, 255 });
    }

    for (const auto& rocket : rockets_) {
        if (!rocket.alive) continue;
        Vector2 screenPos = toScreen(rocket.pos);
        DrawRectangle(screenPos.x - camX - 4.0f, screenPos.y - 2.0f, 12, 4, Color{ 240, 220, 80, 255 });
    }

    for (const auto& jet : enemyJets_) {
        if (!jet.alive) continue;
        Vector2 screenPos = toScreen(jet.pos);
        DrawTriangle(Vector2{ screenPos.x - camX - 18.0f, screenPos.y + 10.0f }, Vector2{ screenPos.x - camX + 12.0f, screenPos.y }, Vector2{ screenPos.x - camX - 18.0f, screenPos.y - 10.0f }, Color{ 200, 120, 120, 255 });
    }

    for (const auto& shot : enemyShots_) {
        if (!shot.alive) continue;
        Vector2 screenPos = toScreen(shot.pos);
        DrawCircleV({screenPos.x - camX, screenPos.y}, 3.0f, RED);
    }

    for (const auto& target : groundTargets_) {
        if (!target.alive) continue;
        Rectangle rect = target.rect;
        rect.x -= camX;
        DrawRectangleRec(rect, Color{ 100, 90, 120, 255 });
        DrawRectangleLines(static_cast<int>(rect.x), static_cast<int>(rect.y), static_cast<int>(rect.width), static_cast<int>(rect.height), Color{ 200, 200, 200, 255 });
    }

    DrawRectangle(20, 20, 220, 92, Color{ 20, 30, 60, 200 });
    DrawRectangleLines(20, 20, 220, 92, Color{ 180, 200, 255, 200 });
    DrawText(TextFormat("Speed: %.0f", player_.vel.x), 32, 32, 20, RAYWHITE);
    DrawText(TextFormat("Altitude: %.0f", groundY_ - player_.pos.y), 32, 52, 20, RAYWHITE);
    DrawText(TextFormat("Fuel: %.0f", player_.fuel), 32, 72, 20, (player_.fuel < 30.0f) ? RED : RAYWHITE);
    DrawText(TextFormat("Score: %05d", score_), 32, 92, 20, GOLD);

    std::string difficultyText = "Difficulty: " + difficulties_[difficultyIndex_].label;
    int diffWidth = MeasureText(difficultyText.c_str(), 20);
    DrawText(difficultyText.c_str(), width_ - diffWidth - 20, 24, 20, RAYWHITE);

    if (!statusMessage_.empty()) {
        int msgWidth = MeasureText(statusMessage_.c_str(), 26);
        DrawText(statusMessage_.c_str(), width_ / 2 - msgWidth / 2, 20, 26, Color{ 255, 240, 120, 255 });
    }

    if (player_.missionComplete) {
        const char* msg = "Mission Complete - Press Enter";
        int msgWidth = MeasureText(msg, 28);
        DrawText(msg, width_ / 2 - msgWidth / 2, height_ / 2 - 20, 28, Color{ 180, 255, 180, 255 });
    } else if (missionFailed_) {
        const char* msg = "Mission Failed - Press Enter";
        int msgWidth = MeasureText(msg, 28);
        DrawText(msg, width_ / 2 - msgWidth / 2, height_ / 2 - 20, 28, Color{ 255, 120, 120, 255 });
    }
}

} // namespace gb2d::games
