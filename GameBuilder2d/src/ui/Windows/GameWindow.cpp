#include "ui/Windows/GameWindow.h"
#include "ui/WindowContext.h"
#include "ui/FullscreenSession.h"
#include "ui/ImGuiTextureHelpers.h"
#include "games/Game.h"
#include "games/SpaceInvaders.h"
#include "games/Galaga.h"
#include "games/HarrierAttack.h"
#include "games/PacMan.h"
#include "games/PlarformerGame.h"
#include "services/logger/LogManager.h"
#include "services/hotkey/HotKeyManager.h"
#include "services/hotkey/HotKeyActions.h"
#include "ui/ImGuiAuto/ImGuiAuto.h"
#include "ui/ImGuiAuto/ImGuiAutoDemo.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdint>
#include <type_traits>
#include <utility>
#include "ui/FullscreenSession.h"
#include "ui/ImGuiTextureHelpers.h"
#include "games/Game.h"
#include "games/SpaceInvaders.h"
#include "games/Galaga.h"
#include "games/HarrierAttack.h"
#include "games/PacMan.h"
#include "games/PlarformerGame.h"
#include "services/logger/LogManager.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdint>
#include <type_traits>
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

    std::string hotkeyShortcutLabel(const char* actionId) {
        using gb2d::hotkeys::HotKeyManager;
        if (actionId == nullptr || !HotKeyManager::isInitialized()) {
            return {};
        }
        const auto* binding = HotKeyManager::binding(actionId);
        if (binding == nullptr || !binding->valid || binding->humanReadable.empty()) {
            return {};
        }
        return binding->humanReadable;
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
    releaseGameIcons();
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
        loadIconForEntry(entry);
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
}

void GameWindow::cycleGame(int delta) {
    if (games_.empty() || delta == 0) {
        return;
    }
    if (current_game_index_ < 0) {
        switchGame(0);
        return;
    }

    const int count = static_cast<int>(games_.size());
    int nextIndex = (current_game_index_ + delta) % count;
    if (nextIndex < 0) {
        nextIndex += count;
    }
    if (nextIndex != current_game_index_) {
        switchGame(nextIndex);
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

    const std::string resetShortcut = hotkeyShortcutLabel(hotkeys::actions::GameReset);
    const std::string fullscreenShortcut = hotkeyShortcutLabel(hotkeys::actions::GameToggleFullscreen);
    const std::string exitSessionShortcut = hotkeyShortcutLabel(hotkeys::actions::FullscreenExit);

    const bool hasSession = (ctx.fullscreen != nullptr);
    const bool sessionActive = hasSession && ctx.fullscreen->isActive();
    const bool canRequestFullscreen = (current_game_ != nullptr) && hasSession && !sessionActive;
    const bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    if (windowFocused && gb2d::hotkeys::HotKeyManager::isInitialized()) {
        if (gb2d::hotkeys::HotKeyManager::consumeTriggered(hotkeys::actions::GameReset)) {
            resetCurrentGame();
        }
        if (canRequestFullscreen && gb2d::hotkeys::HotKeyManager::consumeTriggered(hotkeys::actions::GameToggleFullscreen)) {
            fullscreen_requested_ = true;
        }
        if (gb2d::hotkeys::HotKeyManager::consumeTriggered(hotkeys::actions::GameCycleNext)) {
            cycleGame(1);
        }
        if (gb2d::hotkeys::HotKeyManager::consumeTriggered(hotkeys::actions::GameCyclePrev)) {
            cycleGame(-1);
        }
    }

    // Add a button to toggle the ImGui::Auto demo
    if (ImGui::Button("ImGui::Auto Demo")) {
        show_imgui_auto_demo_ = !show_imgui_auto_demo_;
    }
    ImGui::SameLine();
    
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
        if (!resetShortcut.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            ImGui::SetTooltip("Shortcut: %s", resetShortcut.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button(show_imgui_auto_demo_ ? "Hide ImGui::Auto Demo" : "Show ImGui::Auto Demo")) {
            show_imgui_auto_demo_ = !show_imgui_auto_demo_;
        }
        ImGui::SameLine();
        bool fullscreenClicked = false;
        if (!canRequestFullscreen) {
            ImGui::BeginDisabled();
        }
        if (ImGui::Button("Fullscreen")) {
            fullscreenClicked = true;
        }
        if (!canRequestFullscreen) {
            ImGui::EndDisabled();
        }
        if (fullscreenClicked && canRequestFullscreen) {
            fullscreen_requested_ = true;
        }
        if (canRequestFullscreen && !fullscreenShortcut.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
            ImGui::SetTooltip("Shortcut: %s", fullscreenShortcut.c_str());
        }
        ImGui::SameLine();
        if (sessionActive) {
            std::string exitHint = "Ctrl+W";
            if (!exitSessionShortcut.empty()) {
                exitHint += " or " + exitSessionShortcut;
            } else {
                exitHint += " or Esc";
            }
            const std::string exitText = "Press " + exitHint + " to exit";
            ImGui::TextUnformatted(exitText.c_str());
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Leaving fullscreen returns to the editor.");
            }
        } else {
            ImGui::TextDisabled("Use game controls (e.g. arrows + space)");
        }

        ImGui::Separator();
        ImGui::BeginGroup();
        for (int i = 0; i < (int)games_.size(); ++i) {
            ImGui::PushID(i);
            bool isCurrent = (i == current_game_index_);
            if (isCurrent) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            }
            ImGui::BeginGroup();
            const Texture2D* iconTexture = nullptr;
            bool iconPlaceholder = false;
            if (games_[i].icon) {
                iconTexture = gb2d::textures::TextureManager::tryGet(games_[i].icon->key);
                iconPlaceholder = games_[i].icon->placeholder;
            }
            if (iconTexture) {
                constexpr float kIconSize = 36.0f;
                ImTextureID iconId = gb2d::ui::makeImTextureId<ImTextureID>(iconTexture->id);
                ImGui::Image(iconId, ImVec2(kIconSize, kIconSize));
                if (iconPlaceholder && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone)) {
                    ImGui::SetTooltip("Placeholder icon (asset missing)");
                }
                ImGui::Spacing();
            }
            if (ImGui::SmallButton(games_[i].name.c_str())) {
                switchGame(i);
            }
            ImGui::EndGroup();
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
            ctx.fullscreen->setResetHook([this]() { this->handleFullscreenExit(); });
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
        
        // Display the ImGui::Auto demo if enabled
        if (show_imgui_auto_demo_) {
            ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("ImGui::Auto Demo", &show_imgui_auto_demo_)) {
                ImGui::Auto::Init(); // Initialize the ImGui::Auto system
                ImGui::AutoDemo::ShowDemo();
            }
            ImGui::End();
        }
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

void GameWindow::loadIconForEntry(GameEntry& entry) {
    using gb2d::logging::LogManager;
    const std::string relativePath = "ui/game-icons/" + entry.id + ".png";
    const std::string alias = "game-window/icon/" + entry.id;
    auto acquired = gb2d::textures::TextureManager::acquire(relativePath, alias);
    if (!acquired.texture) {
        LogManager::warn("GameWindow icon '{}' failed to acquire texture (placeholder unavailable)", relativePath);
    } else if (acquired.placeholder) {
        LogManager::debug("GameWindow icon '{}' using placeholder texture", relativePath);
    } else {
        LogManager::debug("GameWindow icon '{}' loaded", relativePath);
    }
    entry.icon = std::move(acquired);
}

void GameWindow::releaseGameIcons() {
    using gb2d::logging::LogManager;
    for (auto& entry : games_) {
        if (!entry.icon || entry.icon->key.empty()) {
            entry.icon.reset();
            continue;
        }
        if (!gb2d::textures::TextureManager::release(entry.icon->key)) {
            LogManager::warn("GameWindow failed to release icon '{}'", entry.icon->key);
        }
        entry.icon.reset();
    }
}

} // namespace gb2d
