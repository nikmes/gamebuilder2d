#include "ui/Windows/GameWindow.h"
#include "ui/WindowContext.h"
#include "ui/FullscreenSession.h"
#include "services/configuration/ConfigurationManager.h"
#include "games/Game.h"
#include "games/SpaceInvaders.h"
#include "games/Galaga.h"
#include "games/HarrierAttack.h"
#include "games/PacMan.h"
#include "games/PlarformerGame.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <utility>

namespace gb2d {

namespace {
    struct GameDescriptor {
        std::string id;
        std::string name;
        std::function<std::unique_ptr<games::Game>()> factory;
    };

    const std::vector<GameDescriptor>& defaultGameDescriptors() {
        static const std::vector<GameDescriptor> descriptors = [](){
            std::vector<GameDescriptor> list;
            list.push_back(GameDescriptor{
                "space-invaders",
                "Space Invaders",
                [](){ return std::make_unique<games::SpaceInvaders>(); }
            });
            list.push_back(GameDescriptor{
                "galaga",
                "Galaga",
                [](){ return std::make_unique<games::Galaga>(); }
            });
            list.push_back(GameDescriptor{
                "harrier-attack",
                "Harrier Attack",
                [](){ return std::make_unique<games::HarrierAttack>(); }
            });
            list.push_back(GameDescriptor{
                "pac-man",
                "Pac-Man",
                [](){ return std::make_unique<games::PacMan>(); }
            });
            list.push_back(GameDescriptor{
                "plarformer",
                "Plarformer",
                [](){ return std::make_unique<games::PlarformerGame>(); }
            });
            return list;
        }();
        return descriptors;
    }
}

GameWindow::GameWindow() {
    registerDefaultGames();
}

GameWindow::~GameWindow() {
    if (current_game_) {
        current_game_->unload();
        current_game_.reset();
    }
    unloadRenderTarget();
}

void GameWindow::unloadRenderTarget() {
    if (rt_w_ > 0 && rt_h_ > 0) {
        UnloadRenderTexture(rt_);
        rt_w_ = rt_h_ = 0;
    }
}

void GameWindow::ensureRenderTarget(int w, int h) {
    if (w <= 0 || h <= 0) return;
    if (rt_w_ == w && rt_h_ == h) return;
    unloadRenderTarget();
    rt_ = LoadRenderTexture(w, h);
    rt_w_ = w; rt_h_ = h;
    if (current_game_) {
        if (game_needs_init_) {
            current_game_->init(rt_w_, rt_h_);
            game_needs_init_ = false;
        } else {
            current_game_->onResize(rt_w_, rt_h_);
        }
    }
}

void GameWindow::registerDefaultGames() {
    for (const auto& desc : defaultGameDescriptors()) {
        GameEntry entry;
        entry.id = desc.id;
        entry.name = desc.name;
        entry.factory = desc.factory;
        games_.push_back(std::move(entry));
    }
}

void GameWindow::ensureGameSelected() {
    if (current_game_index_ >= 0 && current_game_index_ < (int)games_.size()) return;
    if (games_.empty()) return;
    switchGame(0);
}

void GameWindow::switchGame(int index) {
    if (index < 0 || index >= (int)games_.size()) return;
    if (current_game_) {
        current_game_->unload();
    }
    current_game_ = games_[index].factory();
    current_game_index_ = index;
    game_needs_init_ = true;
    ensureGameInitialized();
    if (syncResumePreferenceGameId()) {
        ConfigurationManager::save();
    }
}

void GameWindow::resetCurrentGame() {
    if (current_game_) {
        current_game_->reset(rt_w_, rt_h_);
    }
}

void GameWindow::ensureGameInitialized() {
    if (!current_game_ || !game_needs_init_) return;
    if (rt_w_ <= 0 || rt_h_ <= 0) return;
    current_game_->init(rt_w_, rt_h_);
    game_needs_init_ = false;
}

void GameWindow::render(WindowContext& ctx) {
    ImVec2 avail = ImGui::GetContentRegionAvail();
    int targetW = std::max(32, (int)avail.x);
    int targetH = std::max(32, (int)avail.y);
    ensureRenderTarget(targetW, targetH);
    ensureGameSelected();
    ensureGameInitialized();

    if (!resume_pref_checked_) {
        resume_pref_checked_ = true;
        resume_pref_enabled_ = ConfigurationManager::getBool("window::resume_fullscreen", false);
        resume_pref_last_game_ = ConfigurationManager::getString("window::fullscreen_last_game", "");
        if (resume_pref_enabled_) {
            resume_pref_autostart_pending_ = true;
            if (!resume_pref_last_game_.empty() && resume_pref_last_game_ != currentGameId()) {
                setGameById(resume_pref_last_game_);
            }
        }
    }

    // Toolbar: game selection + reset
    if (!games_.empty()) {
        std::vector<const char*> names;
        names.reserve(games_.size());
        for (auto& g : games_) names.push_back(g.name.c_str());
        int idx = current_game_index_;
        ImGui::SetNextItemWidth(180);
        if (ImGui::Combo("Game", &idx, names.data(), (int)names.size())) {
            switchGame(idx);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            resetCurrentGame();
        }
        ImGui::SameLine();
        bool hasSession = (ctx.fullscreen != nullptr);
        bool sessionActive = hasSession && ctx.fullscreen->isActive();
        bool canRequestFullscreen = (current_game_ != nullptr) && hasSession && !sessionActive;

        if (resume_pref_autostart_pending_) {
            if (canRequestFullscreen) {
                fullscreen_requested_ = true;
                resume_pref_autostart_pending_ = false;
            } else if (sessionActive) {
                resume_pref_autostart_pending_ = false;
            }
        }

        if (!canRequestFullscreen) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Fullscreen")) {
            fullscreen_requested_ = true;
        }
        if (!canRequestFullscreen) {
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        if (sessionActive) {
            ImGui::TextUnformatted("Press Ctrl+W or Esc to exit");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Leaving fullscreen returns to the editor. Enable resume to auto-start this game on next launch.");
            }
        } else {
            ImGui::TextDisabled("Use game controls (e.g. arrows + space)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Enable resume to re-enter fullscreen automatically when the editor starts.");
            }
        }

        bool resumePref = resume_pref_enabled_;
        if (ImGui::Checkbox("Resume fullscreen on launch", &resumePref)) {
            resume_pref_enabled_ = resumePref;
            ConfigurationManager::set("window::resume_fullscreen", resume_pref_enabled_);
            if (resume_pref_enabled_) {
                syncResumePreferenceGameId();
            } else {
                clearResumePreferenceGameId();
                resume_pref_autostart_pending_ = false;
            }
            ConfigurationManager::save();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("When enabled, the editor will re-enter fullscreen using the last played game on startup.");
        }

        ImGui::Separator();
        ImGui::BeginGroup();
        for (int i = 0; i < (int)games_.size(); ++i) {
            ImGui::PushID(i);
            bool isCurrent = (i == current_game_index_);
            if (isCurrent) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            }
            if (ImGui::SmallButton(games_[i].name.c_str())) {
                switchGame(i);
            }
            if (isCurrent) {
                ImGui::PopStyleColor();
            }
            ImGui::PopID();
            if (i + 1 < (int)games_.size()) {
                ImGui::SameLine();
            }
        }
        ImGui::EndGroup();
        ImGui::Spacing();
    } else {
        ImGui::TextDisabled("No games available");
    }

    if (fullscreen_requested_) {
        if (ctx.fullscreen && current_game_) {
            fullscreen_requested_ = false;
            resume_pref_autostart_pending_ = false;
            ctx.fullscreen->setResetHook([this]() { this->handleFullscreenExit(); });
            if (resume_pref_enabled_ && syncResumePreferenceGameId()) {
                ConfigurationManager::save();
            }
            ctx.fullscreen->requestStart(*current_game_, currentGameId(), rt_w_, rt_h_);
        }
    }

    float dt = GetFrameTime();
    bool acceptInput = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (current_game_) {
        current_game_->update(dt, rt_w_, rt_h_, acceptInput);
    }

    BeginTextureMode(rt_);
    if (current_game_) {
        current_game_->render(rt_w_, rt_h_);
    } else {
        ClearBackground(DARKGRAY);
    }
    EndTextureMode();

    ImGui::BeginChild("game_view", ImVec2(0,0), false, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
    {
        ImVec2 region = ImGui::GetContentRegionAvail();
        float drawW = region.x > 1 ? region.x : (float)rt_w_;
        float drawH = region.y > 1 ? region.y : (float)rt_h_;
        ImTextureID texId = (ImTextureID)(intptr_t)rt_.texture.id;
        ImVec2 uv0(0, 1);
        ImVec2 uv1(1, 0);
        ImGui::Image(texId, ImVec2(drawW, drawH), uv0, uv1);
    }
    ImGui::EndChild();
}

void GameWindow::serialize(nlohmann::json& out) const {
    out["title"] = title_;
    auto gameId = currentGameId();
    if (!gameId.empty()) out["game"] = gameId;
}

void GameWindow::deserialize(const nlohmann::json& in) {
    if (auto it = in.find("title"); it != in.end() && it->is_string()) title_ = *it;
    std::string gameId;
    if (auto it = in.find("game"); it != in.end() && it->is_string()) gameId = *it;

    if (!gameId.empty()) {
        setGameById(gameId);
    }
}

bool GameWindow::setGameById(const std::string& id) {
    for (int i = 0; i < (int)games_.size(); ++i) {
        if (games_[i].id == id) {
            switchGame(i);
            return true;
        }
    }
    return false;
}

std::string GameWindow::currentGameId() const {
    if (current_game_index_ >= 0 && current_game_index_ < (int)games_.size()) {
        return games_[current_game_index_].id;
    }
    return {};
}

std::vector<std::pair<std::string, std::string>> GameWindow::availableGames() {
    std::vector<std::pair<std::string, std::string>> list;
    const auto& descriptors = defaultGameDescriptors();
    list.reserve(descriptors.size());
    for (const auto& desc : descriptors) {
        list.emplace_back(desc.id, desc.name);
    }
    return list;
}

void GameWindow::handleFullscreenExit() {
    game_needs_init_ = true;
}

bool GameWindow::syncResumePreferenceGameId() {
    if (!resume_pref_checked_ || !resume_pref_enabled_) return false;
    std::string id = currentGameId();
    if (id.empty()) return false;
    if (id == resume_pref_last_game_) return false;
    resume_pref_last_game_ = std::move(id);
    ConfigurationManager::set("window::fullscreen_last_game", resume_pref_last_game_);
    return true;
}

bool GameWindow::clearResumePreferenceGameId() {
    if (!resume_pref_checked_) return false;
    if (resume_pref_last_game_.empty()) return false;
    resume_pref_last_game_.clear();
    ConfigurationManager::set("window::fullscreen_last_game", std::string());
    return true;
}

} // namespace gb2d
