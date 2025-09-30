#include "ui/Windows/Games/HarrierAttack.h"
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
    // Bombs vs ground targets
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

    // Rockets vs jets
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

    // Player vs enemy shots
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

    // Player vs jets
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

    // Player vs terrain (crash)
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
        setStatusMessage("Fuel exhausted", 2.8f);
    }

    if (!player_.alive) {
        missionFailed_ = true;
    }

    if (player_.missionComplete) {
        missionFailed_ = false;
    }
}

float HarrierAttack::cameraX() const {
    float cam = player_.pos.x - width_ * 0.4f;
    cam = std::clamp(cam, 0.0f, std::max(0.0f, worldWidth_ - static_cast<float>(width_)));
    return cam;
}

Vector2 HarrierAttack::toScreen(Vector2 world) const {
    return { world.x - cameraX(), world.y };
}

void HarrierAttack::render(int width, int height) {
    (void)width; (void)height;
    ClearBackground(Color{ 18, 26, 56, 255 });

    float cam = cameraX();

    // Sky gradient
    for (int i = 0; i < height_; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(height_);
        Color c = ColorLerp(Color{ 16, 24, 46, 255 }, Color{ 30, 64, 120, 255 }, t);
        DrawLine(0, i, width_, i, c);
    }

    // Sea
    DrawRectangle(0, static_cast<int>(groundY_), width_, height_ - static_cast<int>(groundY_), Color{ 10, 34, 80, 255 });

    // Carrier deck
    Rectangle carrierDeck{ carrierStart_ - cam + 10.0f, groundY_ - 6.0f, carrierEnd_ - carrierStart_ - 20.0f, 6.0f };
    DrawRectangleRec(carrierDeck, Color{ 60, 68, 82, 255 });
    DrawLine(static_cast<int>(carrierDeck.x), static_cast<int>(carrierDeck.y), static_cast<int>(carrierDeck.x + carrierDeck.width), static_cast<int>(carrierDeck.y), Color{ 200, 210, 220, 255 });

    // Island ground band
    Rectangle islandRect{ islandStart_ - cam, groundY_ - 22.0f, islandEnd_ - islandStart_, 22.0f };
    DrawRectangleRec(islandRect, Color{ 68, 92, 60, 255 });

    // Ground targets
    for (const auto& target : groundTargets_) {
        if (!target.alive) continue;
        Rectangle drawRect{ target.rect.x - cam, target.rect.y, target.rect.width, target.rect.height };
        DrawRectangleRec(drawRect, Color{ 160, 74, 58, 255 });
        DrawRectangle(drawRect.x, drawRect.y, drawRect.width, drawRect.height, Color{ 230, 200, 200, 200 });
    }

    // Bombs & rockets
    for (const auto& bomb : bombs_) {
        Vector2 pos = toScreen(bomb.pos);
        DrawCircleV(pos, 4.0f, YELLOW);
    }
    for (const auto& rocket : rockets_) {
        Vector2 pos = toScreen(rocket.pos);
        DrawRectangleV({ pos.x - 3.0f, pos.y - 2.0f }, { 8.0f, 4.0f }, ORANGE);
        DrawRectangleV({ pos.x, pos.y - 1.0f }, { 8.0f, 2.0f }, RED);
    }

    // Enemy jets
    for (const auto& jet : enemyJets_) {
        if (!jet.alive) continue;
        Vector2 pos = toScreen(jet.pos);
        Vector2 tip = { pos.x - 18.0f, pos.y };
        Vector2 top = { pos.x + 10.0f, pos.y - 10.0f };
        Vector2 bottom = { pos.x + 10.0f, pos.y + 10.0f };
        DrawTriangle(tip, top, bottom, Color{ 235, 168, 52, 255 });
        DrawCircleV({ pos.x - 10.0f, pos.y }, 4.0f, Color{ 80, 16, 16, 255 });
    }

    for (const auto& shot : enemyShots_) {
        if (!shot.alive) continue;
        Vector2 pos = toScreen(shot.pos);
        DrawRectangleV({ pos.x - 2.0f, pos.y - 2.0f }, { 4.0f, 4.0f }, SKYBLUE);
    }

    // Player aircraft
    if (player_.alive || player_.invuln > 0.0f) {
        bool blink = (player_.invuln > 0.0f) && (std::fmod(player_.invuln * 10.0f, 2.0f) > 1.0f);
        if (!blink) {
            Vector2 pos = toScreen(player_.pos);
            Vector2 nose = { pos.x + 22.0f, pos.y };
            Vector2 tail = { pos.x - 22.0f, pos.y };
            Vector2 top = { pos.x - 6.0f, pos.y - 12.0f };
            Vector2 bottom = { pos.x - 6.0f, pos.y + 12.0f };
            DrawTriangle(nose, top, bottom, RAYWHITE);
            DrawTriangle(tail, top, bottom, Color{ 90, 110, 140, 255 });
            DrawCircleV({ pos.x, pos.y }, 6.0f, Color{ 60, 80, 110, 255 });
        }
    }

    // HUD
    const auto& diff = difficulties_[difficultyIndex_];
    DrawRectangle(0, 0, width_, 46, Color{ 12, 20, 38, 230 });
    DrawText(TextFormat("Fuel: %03d", static_cast<int>(player_.fuel)), 18, 10, 18, RAYWHITE);
    DrawText(TextFormat("Bombs: %02d", player_.bombs), 150, 10, 18, YELLOW);
    DrawText(TextFormat("Rockets: %02d", player_.rockets), 260, 10, 18, ORANGE);
    DrawText(TextFormat("Score: %05d", score_), 390, 10, 18, SKYBLUE);
    DrawText(TextFormat("Difficulty: %s", diff.label.c_str()), 520, 10, 18, LIGHTGRAY);
    DrawText("1-5 to change difficulty", 520, 28, 16, Color{ 180, 180, 220, 200 });

    if (statusMessageTimer_ > 0.0f && !statusMessage_.empty()) {
        int widthMsg = MeasureText(statusMessage_.c_str(), 20);
        DrawText(statusMessage_.c_str(), width_ / 2 - widthMsg / 2, 52, 20, GOLD);
    }

    if (missionFailed_) {
        const char* msg = "Mission Failed - Press Enter";
        int tw = MeasureText(msg, 28);
        DrawText(msg, width_ / 2 - tw / 2, height_ / 2 - 20, 28, RED);
    } else if (player_.missionComplete) {
        const char* msg = "Mission Complete - Press Enter";
        int tw = MeasureText(msg, 28);
        DrawText(msg, width_ / 2 - tw / 2, height_ / 2 - 20, 28, GREEN);
    } else if (allTargetsDestroyed() && !player_.landed) {
        const char* msg = "Return to carrier!";
        int tw = MeasureText(msg, 24);
        DrawText(msg, width_ / 2 - tw / 2, height_ / 2 - 20, 24, YELLOW);
    }
}

} // namespace gb2d::games
