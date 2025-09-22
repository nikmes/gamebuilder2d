// GameBuilder2d.cpp : Defines the entry point for the application.
//

#include "GameBuilder2d.h"
#include "raylib.h"
#include "rlImGui.h"
#include "imgui.h"
#include "services/window/WindowManager.h"
#include <memory>
#include <lwlog.h>
#include "services/configuration/ConfigurationManager.h"
#include "services/configuration/logging.h"

using namespace std;

int main()
{
    auto logger = std::make_shared<lwlog::console_logger>("GB2D");
    logger->set_pattern("[%T] [%n] [%l]: %v");
    logger->info("GameBuilder2d starting up");
    
    gb2d::cfglog::set_sink([logger](gb2d::cfglog::Level lvl, const char* msg){
        switch (lvl) {
        case gb2d::cfglog::Level::Info: logger->info("%s", msg); break;
        case gb2d::cfglog::Level::Warning: logger->warning("%s", msg); break;
        case gb2d::cfglog::Level::Debug: logger->debug("%s", msg); break;
        }
    });

    gb2d::ConfigurationManager::loadOrDefault();

    // Example: set and persist fullscreen via ConfigurationManager (section-style key)
    gb2d::ConfigurationManager::set("window::fullscreen", true);
    gb2d::ConfigurationManager::save();

    // Read the value back using the same manager
    bool fullscreen = gb2d::ConfigurationManager::getBool("window::fullscreen", false);

    unsigned int flags = FLAG_WINDOW_RESIZABLE;

    if (fullscreen) flags |= FLAG_FULLSCREEN_MODE;

    SetConfigFlags(flags);
    InitWindow(1920, 1080, "GameBuilder2d + rlImGui");
    SetTargetFPS(60);

    rlImGuiSetup(true);

    // Enable docking in ImGui (after context is created)
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    static gb2d::WindowManager wm{};

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(DARKGRAY);

        rlImGuiBegin();
        wm.renderUI();
        rlImGuiEnd();

        EndDrawing();
    }

    logger->debug("Exiting GameBuilder2d");
    // Save layout before shutting down ImGui
    wm.saveLayout();
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
