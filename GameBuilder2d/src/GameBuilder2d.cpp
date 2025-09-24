// GameBuilder2d.cpp : Defines the entry point for the application.
//

#include "GameBuilder2d.h"
#include "raylib.h"
#include "rlImGui.h"
#include "imgui.h"
#include "services/window/WindowManager.h"
#include <memory>
#include "services/configuration/ConfigurationManager.h"

using namespace std;

int main()
{
    // Basic startup notice (no external logging dependency)
    // printf("GameBuilder2d starting up\n");

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

    // printf("Exiting GameBuilder2d\n");
    // Save layout before shutting down ImGui
    wm.saveLayout();
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
