#include "games/Galaga.h"
#include "services/audio/AudioManager.h"
#include "services/logger/LogManager.h"
#include <algorithm>
#include <cmath>
#include <raymath.h>

namespace gb2d::games {

Vector2 Galaga::evalBezier(const std::array<Vector2, 4>& path, float t) {
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;
    Vector2 result{};
    result.x = uuu * path[0].x + 3.0f * uu * t * path[1].x + 3.0f * u * tt * path[2].x + ttt * path[3].x;
    result.y = uuu * path[0].y + 3.0f * uu * t * path[1].y + 3.0f * u * tt * path[2].y + ttt * path[3].y;
    return result;
}

float Galaga::randomFloat(float minValue, float maxValue) {
    if (maxValue <= minValue) return minValue;
    int span = 1000;
    float r = static_cast<float>(GetRandomValue(0, span)) / static_cast<float>(span);
    return minValue + (maxValue - minValue) * r;
}

void Galaga::init(int width, int height) {
    loadAudioAssets();
    setup(width, height);
}

void Galaga::reset(int width, int height) {
    setup(width, height);
}

void Galaga::onResize(int width, int height) {
    setup(width, height);
}

void Galaga::unload() {
    player_bullets_.clear();
    enemy_bullets_.clear();
    enemies_.clear();
    stars_.clear();
    releaseAudioAssets();
}

void Galaga::setup(int width, int height) {
    width_ = width;
    height_ = height;

    player_ = Player{};
    player_.pos = { width_ * 0.5f, height_ - 60.0f };
    player_.lives = 3;
    player_.alive = true;
    player_.cooldown = 0.0f;
    player_.invulnTimer = 1.0f;
    player_.respawnTimer = 0.0f;

    player_bullets_.clear();
    enemy_bullets_.clear();
    enemies_.clear();

    regenerateStarfield();
    setupFormation();

    dive_timer_ = 2.5f;
    score_ = 0;
    victory_ = false;
    game_over_ = false;
    victory_cue_played_ = false;
    game_over_cue_played_ = false;
}

void Galaga::setupFormation() {
    const int columns = 8;
    const int rows = 4;
    float marginX = width_ * 0.1f;
    marginX = std::clamp(marginX, 60.0f, 160.0f);
    float spacingX = columns > 1 ? (width_ - marginX * 2.0f) / (columns - 1) : 0.0f;
    float startY = height_ * 0.18f;
    float spacingY = 52.0f;

    enemies_.reserve(columns * rows);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < columns; ++c) {
            Enemy enemy;
            enemy.row = r;
            enemy.formationPos = {
                marginX + spacingX * static_cast<float>(c),
                startY + spacingY * static_cast<float>(r)
            };
            enemy.pos = enemy.formationPos;
            enemy.alive = true;
            enemy.state = EnemyState::Entering;
            enemy.pathT = 0.0f;
            enemy.pathSpeed = 0.7f + 0.08f * static_cast<float>(r);
            enemy.bobPhase = randomFloat(0.0f, 2.0f * PI);
            enemy.hasShot = false;
            enemy.heading = -PI * 0.5f;
            assignEntryPath(enemy, r, c, columns);
            enemies_.push_back(enemy);
        }
    }
}

void Galaga::regenerateStarfield() {
    stars_.clear();
    int starCount = std::clamp(width_ / 8, 40, 140);
    stars_.reserve(starCount);
    for (int i = 0; i < starCount; ++i) {
        Star star;
        star.pos = { randomFloat(0.0f, static_cast<float>(width_)), randomFloat(0.0f, static_cast<float>(height_)) };
        star.speed = randomFloat(14.0f, 80.0f);
        star.scale = randomFloat(0.8f, 2.4f);
        stars_.push_back(star);
    }
}

void Galaga::assignEntryPath(Enemy& enemy, int row, int col, int columns) {
    float horizontalDir = (col < columns / 2) ? 1.0f : -1.0f;
    Vector2 start = { enemy.formationPos.x + horizontalDir * (width_ * 0.6f), -120.0f - row * 40.0f };
    Vector2 c1 = { width_ * 0.5f + horizontalDir * 80.0f, height_ * (0.12f + row * 0.06f) };
    Vector2 c2 = { enemy.formationPos.x - horizontalDir * 60.0f, enemy.formationPos.y - 80.0f };
    enemy.path = { start, c1, c2, enemy.formationPos };
    enemy.pathT = 0.0f;
    enemy.state = EnemyState::Entering;
    enemy.hasShot = false;
}

void Galaga::assignDivePath(Enemy& enemy) {
    enemy.state = EnemyState::Diving;
    enemy.pathT = 0.0f;
    enemy.pathSpeed = 1.6f;
    enemy.hasShot = false;

    float dir = (enemy.pos.x < player_.pos.x) ? 1.0f : -1.0f;
    Vector2 start = enemy.pos;
    Vector2 c1 = { enemy.pos.x + dir * 90.0f, height_ * 0.32f };
    Vector2 c2 = { player_.pos.x + dir * 140.0f, height_ * 0.65f };
    Vector2 end = { player_.pos.x, height_ + 160.0f };
    enemy.path = { start, c1, c2, end };
}

void Galaga::assignReturnPath(Enemy& enemy) {
    enemy.state = EnemyState::Returning;
    enemy.pathT = 0.0f;
    enemy.pathSpeed = 1.0f;

    Vector2 start = enemy.pos;
    Vector2 c1 = { width_ * 0.5f, height_ * 0.55f };
    Vector2 c2 = { enemy.formationPos.x, height_ * 0.25f };
    enemy.path = { start, c1, c2, enemy.formationPos };
}

void Galaga::update(float dt, int width, int height, bool acceptInput) {
    width_ = width;
    height_ = height;

    if ((victory_ || game_over_) && acceptInput && IsKeyPressed(KEY_ENTER)) {
        setup(width_, height_);
        return;
    }

    updateStarfield(dt);

    if (!victory_ && !game_over_) {
        updatePlayer(dt, acceptInput);
        updatePlayerBullets(dt);
        updateEnemyBullets(dt);
        updateEnemies(dt);
        handleCollisions();

        if (!victory_) {
            bool anyAlive = false;
            for (const auto& enemy : enemies_) {
                if (enemy.alive) { anyAlive = true; break; }
            }
            if (!anyAlive) {
                victory_ = true;
            }
        }
    } else {
        updatePlayer(dt, false);
        updateEnemyBullets(dt);
    }

    if (victory_ && !victory_cue_played_) {
        playSound(sfx_victory_, 1.0f);
        victory_cue_played_ = true;
    }
    if (game_over_ && !game_over_cue_played_) {
        playSound(sfx_game_over_, 1.0f);
        game_over_cue_played_ = true;
    }
}

void Galaga::updatePlayer(float dt, bool acceptInput) {
    if (player_.respawnTimer > 0.0f) {
        player_.respawnTimer -= dt;
        if (player_.respawnTimer <= 0.0f && !game_over_) {
            player_.alive = true;
            player_.pos = { width_ * 0.5f, height_ - 60.0f };
            player_.cooldown = 0.0f;
            player_.invulnTimer = std::max(player_.invulnTimer, 1.5f);
        }
    }

    player_.invulnTimer = std::max(0.0f, player_.invulnTimer - dt);
    player_.cooldown = std::max(0.0f, player_.cooldown - dt);

    if (!player_.alive) return;

    if (acceptInput) {
        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
            player_.pos.x -= player_.speed * dt;
        }
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
            player_.pos.x += player_.speed * dt;
        }
    float minX = std::min(32.0f, static_cast<float>(width_) - 32.0f);
    float maxX = std::max(32.0f, static_cast<float>(width_) - 32.0f);
    player_.pos.x = std::clamp(player_.pos.x, minX, maxX);

        if ((IsKeyDown(KEY_SPACE) || IsKeyPressed(KEY_Z)) && player_.cooldown <= 0.0f) {
            Shot shot;
            shot.pos = { player_.pos.x, player_.pos.y - 24.0f };
            shot.vel = { 0.0f, -480.0f };
            player_bullets_.push_back(shot);
            player_.cooldown = 0.18f;
            playSound(sfx_player_shot_, 0.8f, panForX(player_.pos.x));
        }
    }
}

void Galaga::updatePlayerBullets(float dt) {
    for (auto& shot : player_bullets_) {
        if (!shot.alive) continue;
        shot.pos = Vector2Add(shot.pos, Vector2Scale(shot.vel, dt));
        if (shot.pos.y < -40.0f) {
            shot.alive = false;
        }
    }
    player_bullets_.erase(std::remove_if(player_bullets_.begin(), player_bullets_.end(), [](const Shot& s){ return !s.alive; }), player_bullets_.end());
}

void Galaga::updateEnemyBullets(float dt) {
    for (auto& shot : enemy_bullets_) {
        if (!shot.alive) continue;
        shot.pos = Vector2Add(shot.pos, Vector2Scale(shot.vel, dt));
        if (shot.pos.y > height_ + 40.0f) {
            shot.alive = false;
        }
    }
    enemy_bullets_.erase(std::remove_if(enemy_bullets_.begin(), enemy_bullets_.end(), [](const Shot& s){ return !s.alive; }), enemy_bullets_.end());
}

bool Galaga::anyFormationEnemies() const {
    for (const auto& enemy : enemies_) {
        if (enemy.alive && enemy.state == EnemyState::Formation) return true;
    }
    return false;
}

void Galaga::spawnDive() {
    std::vector<int> candidates;
    candidates.reserve(enemies_.size());
    for (int i = 0; i < static_cast<int>(enemies_.size()); ++i) {
        if (enemies_[i].alive && enemies_[i].state == EnemyState::Formation) {
            candidates.push_back(i);
        }
    }
    if (candidates.empty()) return;
    int maxGroup = std::min<int>(3, static_cast<int>(candidates.size()));
    int roll = GetRandomValue(0, 99);
    int groupSize = 1;
    if (maxGroup >= 3) {
        if (roll < 25) {
            groupSize = 3;
        } else if (roll < 70) {
            groupSize = 2;
        }
    } else if (maxGroup == 2) {
        if (roll < 55) groupSize = 2;
    }

    int launched = 0;
    int baseIndexList = GetRandomValue(0, static_cast<int>(candidates.size()) - 1);
    int baseEnemyIdx = candidates[baseIndexList];
    int baseRow = enemies_[baseEnemyIdx].row;
    assignDivePath(enemies_[baseEnemyIdx]);
    ++launched;
    candidates.erase(candidates.begin() + baseIndexList);

    std::vector<int> rowPool;
    rowPool.reserve(candidates.size());
    for (int idx : candidates) {
        if (enemies_[idx].row == baseRow) {
            rowPool.push_back(idx);
        }
    }

    while (launched < groupSize && !candidates.empty()) {
        int enemyIdx;
        if (!rowPool.empty()) {
            int poolPos = GetRandomValue(0, static_cast<int>(rowPool.size()) - 1);
            enemyIdx = rowPool[poolPos];
            rowPool.erase(rowPool.begin() + poolPos);
            auto it = std::find(candidates.begin(), candidates.end(), enemyIdx);
            if (it != candidates.end()) {
                candidates.erase(it);
            }
        } else {
            int pick = GetRandomValue(0, static_cast<int>(candidates.size()) - 1);
            enemyIdx = candidates[pick];
            candidates.erase(candidates.begin() + pick);
        }

        assignDivePath(enemies_[enemyIdx]);
        ++launched;
    }

    dive_timer_ = randomFloat(dive_interval_min_, dive_interval_max_);
}

void Galaga::updateEnemies(float dt) {
    bool entering = false;
    for (auto& enemy : enemies_) {
        if (!enemy.alive) continue;

        Vector2 previousPos = enemy.pos;

        switch (enemy.state) {
            case EnemyState::Entering:
            case EnemyState::Diving:
            case EnemyState::Returning: {
                enemy.pathT += enemy.pathSpeed * dt * 0.5f;
                if (enemy.pathT > 1.0f) enemy.pathT = 1.0f;
                enemy.pos = evalBezier(enemy.path, enemy.pathT);

                if (enemy.state == EnemyState::Diving && !enemy.hasShot && enemy.pathT > 0.35f) {
                    Shot shot;
                    shot.pos = { enemy.pos.x, enemy.pos.y + 12.0f };
                    shot.vel = { 0.0f, 220.0f + 40.0f * enemy.row };
                    enemy_bullets_.push_back(shot);
                    enemy.hasShot = true;
                }

                if (enemy.pathT >= 1.0f) {
                    if (enemy.state == EnemyState::Entering) {
                        enemy.state = EnemyState::Formation;
                        enemy.pos = enemy.formationPos;
                        enemy.bobPhase = randomFloat(0.0f, 2.0f * PI);
                    } else if (enemy.state == EnemyState::Diving) {
                        assignReturnPath(enemy);
                    } else if (enemy.state == EnemyState::Returning) {
                        enemy.state = EnemyState::Formation;
                        enemy.pos = enemy.formationPos;
                        enemy.bobPhase = randomFloat(0.0f, 2.0f * PI);
                    }
                }
                if (enemy.state == EnemyState::Entering && enemy.pathT < 1.0f) entering = true;
                break;
            }
            case EnemyState::Formation: {
                enemy.bobPhase += dt * (1.0f + 0.2f * enemy.row);
                Vector2 target = enemy.formationPos;
                target.y += std::sinf(enemy.bobPhase * 2.0f) * 6.0f;
                enemy.pos = Vector2Lerp(enemy.pos, target, std::clamp(dt * 4.0f, 0.0f, 1.0f));
                break;
            }
        }

        Vector2 velocity = Vector2Subtract(enemy.pos, previousPos);
        float speed = Vector2Length(velocity);
        if (speed > 1e-3f) {
            enemy.heading = std::atan2(velocity.y, velocity.x);
        }
    }

    if (!entering && anyFormationEnemies()) {
        dive_timer_ -= dt;
        if (dive_timer_ <= 0.0f) {
            spawnDive();
        }
    }
}

void Galaga::handleCollisions() {
    if (!player_bullets_.empty()) {
        for (auto& shot : player_bullets_) {
            if (!shot.alive) continue;
            for (auto& enemy : enemies_) {
                if (!enemy.alive) continue;
                float dist = Vector2Distance(shot.pos, enemy.pos);
                if (dist < 18.0f) {
                    shot.alive = false;
                    enemy.alive = false;
                    score_ += 150 + enemy.row * 60;
                    playSound(sfx_enemy_down_, 0.85f, panForX(enemy.pos.x));
                    break;
                }
            }
        }
    }

    if (player_.alive && player_.invulnTimer <= 0.0f) {
        for (auto& shot : enemy_bullets_) {
            if (!shot.alive) continue;
            if (Vector2Distance(shot.pos, player_.pos) < 22.0f) {
                shot.alive = false;
                handlePlayerHit();
                break;
            }
        }
    }

    if (player_.alive && player_.invulnTimer <= 0.0f) {
        for (auto& enemy : enemies_) {
            if (!enemy.alive) continue;
            if (enemy.state == EnemyState::Diving) {
                if (Vector2Distance(enemy.pos, player_.pos) < 26.0f) {
                    handlePlayerHit();
                    enemy.alive = false;
                    score_ += 200;
                    break;
                }
            }
        }
    }

    enemy_bullets_.erase(std::remove_if(enemy_bullets_.begin(), enemy_bullets_.end(), [](const Shot& s){ return !s.alive; }), enemy_bullets_.end());
    player_bullets_.erase(std::remove_if(player_bullets_.begin(), player_bullets_.end(), [](const Shot& s){ return !s.alive; }), player_bullets_.end());
}

void Galaga::handlePlayerHit() {
    if (!player_.alive || player_.invulnTimer > 0.0f || game_over_) return;

    player_.lives -= 1;
    player_bullets_.clear();
    player_.pos = { width_ * 0.5f, height_ - 60.0f };
    playSound(sfx_player_hit_, 1.0f, panForX(player_.pos.x));
    if (player_.lives <= 0) {
        player_.alive = false;
        game_over_ = true;
        return;
    }

    player_.alive = false;
    player_.respawnTimer = 1.2f;
    player_.invulnTimer = 2.4f;
}

void Galaga::updateStarfield(float dt) {
    for (auto& star : stars_) {
        star.pos.y += star.speed * dt;
        if (star.pos.y > height_ + 5.0f) {
            star.pos.y -= static_cast<float>(height_) + 10.0f;
            star.pos.x = randomFloat(0.0f, static_cast<float>(width_));
        }
    }
}

void Galaga::render(int width, int height) 
{
    (void)width; (void)height;
    ClearBackground(Color{ 5, 8, 28, 255 });

    for (const auto& star : stars_) {
        Color starColor = Color{ 180, 190, 255, static_cast<unsigned char>(120 + star.scale * 50.0f) };
        DrawCircleV(star.pos, star.scale, starColor);
    }

    for (const auto& shot : player_bullets_) {
        DrawRectangleV(Vector2{ shot.pos.x - 1.5f, shot.pos.y - 10.0f }, Vector2{ 3.0f, 20.0f }, YELLOW);
    }
    for (const auto& shot : enemy_bullets_) {
        DrawRectangleV(Vector2{ shot.pos.x - 2.0f, shot.pos.y - 6.0f }, Vector2{ 4.0f, 12.0f }, ORANGE);
    }

    static const Color enemyColors[4] = {
        SKYBLUE, LIME, GOLD, PURPLE
    };
    auto rotateVec = [](Vector2 v, float radians) {
        float c = std::cos(radians);
        float s = std::sin(radians);
        return Vector2{ v.x * c - v.y * s, v.x * s + v.y * c };
    };
    for (const auto& enemy : enemies_) {
        if (!enemy.alive) continue;
        Color body = enemyColors[enemy.row % 4];
        float rotation = enemy.heading - PI * 0.5f;
        Vector2 nose = Vector2Add(enemy.pos, rotateVec(Vector2{0.0f, -12.0f}, rotation));
        Vector2 leftWing = Vector2Add(enemy.pos, rotateVec(Vector2{-14.0f, 8.0f}, rotation));
        Vector2 rightWing = Vector2Add(enemy.pos, rotateVec(Vector2{14.0f, 8.0f}, rotation));
        DrawTriangle(nose, leftWing, rightWing, body);
        DrawCircleV(Vector2Add(enemy.pos, rotateVec(Vector2{-8.0f, 0.0f}, rotation)), 4.0f, ColorBrightness(body, 0.3f));
        DrawCircleV(Vector2Add(enemy.pos, rotateVec(Vector2{8.0f, 0.0f}, rotation)), 4.0f, ColorBrightness(body, 0.3f));
    }

    if (player_.alive) {
        bool visible = (player_.invulnTimer <= 0.0f) || (std::fmod(player_.invulnTimer * 10.0f, 2.0f) < 1.0f);
        if (visible) {
            Vector2 top = { player_.pos.x, player_.pos.y - 16.0f };
            Vector2 left = { player_.pos.x - 14.0f, player_.pos.y + 14.0f };
            Vector2 right = { player_.pos.x + 14.0f, player_.pos.y + 14.0f };
            DrawTriangle(top, left, right, RAYWHITE);
            DrawRectangleV(Vector2{ player_.pos.x - 4.0f, player_.pos.y + 14.0f }, Vector2{ 8.0f, 6.0f }, SKYBLUE);
        }
    } else if (!game_over_) {
        DrawCircleV(player_.pos, 18.0f, ORANGE);
    }

    DrawText(TextFormat("Score: %06d", score_), 18, 16, 22, RAYWHITE);
    DrawText(TextFormat("Lives: %d", std::max(player_.lives, 0)), 18, 44, 20, RAYWHITE);

    if (victory_) {
        const char* msg = "WAVE CLEARED! Press Enter to Restart";
        int textWidth = MeasureText(msg, 24);
        DrawText(msg, width_ / 2 - textWidth / 2, height_ / 2 - 20, 24, GOLD);
    } else if (game_over_) {
        const char* msg = "GAME OVER - Press Enter to Restart";
        int textWidth = MeasureText(msg, 24);
        DrawText(msg, width_ / 2 - textWidth / 2, height_ / 2 - 20, 24, RED);
    }
}

void Galaga::loadAudioAssets() {
    using gb2d::audio::AudioManager;
    using gb2d::logging::LogManager;

    struct AssetConfig {
        const char* identifier;
        const char* alias;
        SoundAsset* slot;
    };

    const std::array<AssetConfig, 5> assets{ {
        {"galaga/player_shot.wav", "game/galaga/player-shot", &sfx_player_shot_},
        {"galaga/enemy_down.wav", "game/galaga/enemy-down", &sfx_enemy_down_},
        {"galaga/player_hit.wav", "game/galaga/player-hit", &sfx_player_hit_},
        {"galaga/victory.wav", "game/galaga/victory", &sfx_victory_},
        {"galaga/game_over.wav", "game/galaga/game-over", &sfx_game_over_}
    } };

    for (const auto& asset : assets) {
        if (!asset.slot->key.empty()) {
            continue;
        }
        auto result = AudioManager::acquireSound(asset.identifier, asset.alias);
        asset.slot->key = result.key;
        asset.slot->placeholder = result.placeholder;
        if (result.key.empty()) {
            LogManager::warn("Galaga audio failed to acquire '{}'", asset.identifier);
        } else if (result.placeholder) {
            LogManager::debug("Galaga audio '{}' using placeholder", asset.identifier);
        } else {
            LogManager::debug("Galaga audio '{}' ready (key='{}')", asset.identifier, result.key);
        }
    }
}

void Galaga::releaseAudioAssets() {
    using gb2d::audio::AudioManager;
    using gb2d::logging::LogManager;

    std::array<SoundAsset*, 5> slots{ {&sfx_player_shot_, &sfx_enemy_down_, &sfx_player_hit_, &sfx_victory_, &sfx_game_over_} };
    for (auto* slot : slots) {
        if (slot->key.empty()) {
            continue;
        }
        if (!AudioManager::releaseSound(slot->key)) {
            LogManager::warn("Galaga audio failed to release '{}'", slot->key);
        }
        slot->key.clear();
        slot->placeholder = true;
    }
}

void Galaga::playSound(const SoundAsset& asset, float volume, float pan) {
    if (asset.key.empty() || asset.placeholder) {
        return;
    }
    gb2d::audio::PlaybackParams params;
    params.volume = volume;
    params.pan = std::clamp(pan, 0.0f, 1.0f);
    gb2d::audio::AudioManager::playSound(asset.key, params);
}

float Galaga::panForX(float worldX) const {
    if (width_ <= 0) {
        return 0.5f;
    }
    float normalized = worldX / static_cast<float>(width_);
    return std::clamp(normalized, 0.0f, 1.0f);
}

} // namespace gb2d::games
