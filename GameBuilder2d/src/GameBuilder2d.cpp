// GameBuilder2d.cpp : Defines the entry point for the application.
//

#include "GameBuilder2d.h"
#include "raylib.h"
#include "rlImGui.h"
#include "imgui.h"
#include "window/WindowManager.h"
#include <memory>
#include <lwlog.h>

using namespace std;

int main()
{
    auto logger = std::make_shared<lwlog::console_logger>("GB2D");
    logger->set_pattern("[%T] [%n] [%l]: %v");
    logger->info("GameBuilder2d starting up");
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1000, 700, "GameBuilder2d + rlImGui");
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
        logger->debug("Frame rendered");
    }
    // Save layout before shutting down ImGui
    wm.saveLayout();
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
