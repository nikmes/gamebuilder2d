#pragma once
#include "ui/Window.h"
#include "services/texture/TextureManager.h"
#include <nlohmann/json_fwd.hpp>
#include <raylib.h>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace gb2d {

namespace games { class Game; }

class GameWindow : public IWindow {
public:
    GameWindow();
    ~GameWindow() override;

    const char* typeId() const override { return "game-window"; }
    const char* displayName() const override { return "Game Window"; }

    std::string title() const override { return title_; }
    void setTitle(std::string t) override { title_ = std::move(t); }

    void render(WindowContext& ctx) override;

    void serialize(nlohmann::json& out) const override;
    void deserialize(const nlohmann::json& in) override;

    bool setGameById(const std::string& id);
    std::string currentGameId() const;
    games::Game* currentGame() const noexcept { return current_game_.get(); }

    static std::vector<std::pair<std::string, std::string>> availableGames();

private:
    struct GameEntry {
        std::string id;
        std::string name;
        std::function<std::unique_ptr<games::Game>()> factory;
        std::optional<gb2d::textures::AcquireResult> icon;
    };

    void unloadRenderTarget();
    void ensureRenderTarget(int w, int h);
    void registerDefaultGames();
    void loadIconForEntry(GameEntry& entry);
    void releaseGameIcons();
    void ensureGameSelected();
    void switchGame(int index);
    void resetCurrentGame();
    void ensureGameInitialized();
    void handleFullscreenExit();

    std::string title_ { "Game Window" };
    RenderTexture2D rt_{};
    int rt_w_{0};
    int rt_h_{0};

    std::vector<GameEntry> games_{};
    int current_game_index_{-1};
    std::unique_ptr<games::Game> current_game_{};
    bool game_needs_init_{false};
    bool fullscreen_requested_{false};
};

} // namespace gb2d
