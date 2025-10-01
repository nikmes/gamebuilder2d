#pragma once
#include "ui/Window.h"
#include <nlohmann/json_fwd.hpp>
#include <raylib.h>
#include <functional>
#include <memory>
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
    };

    void unloadRenderTarget();
    void ensureRenderTarget(int w, int h);
    void registerDefaultGames();
    void ensureGameSelected();
    void switchGame(int index);
    void resetCurrentGame();
    void ensureGameInitialized();
    void handleFullscreenExit();
    bool syncResumePreferenceGameId();
    bool clearResumePreferenceGameId();

    std::string title_ { "Game Window" };
    RenderTexture2D rt_{};
    int rt_w_{0};
    int rt_h_{0};

    std::vector<GameEntry> games_{};
    int current_game_index_{-1};
    std::unique_ptr<games::Game> current_game_{};
    bool game_needs_init_{false};
    bool fullscreen_requested_{false};
    bool resume_pref_checked_{false};
    bool resume_pref_enabled_{false};
    bool resume_pref_autostart_pending_{false};
    std::string resume_pref_last_game_{};
};

} // namespace gb2d
