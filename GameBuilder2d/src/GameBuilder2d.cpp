// GameBuilder2d.cpp : Defines the entry point for the application.
//

#include "GameBuilder2d.h"
#include "raylib.h"
#include "rlImGui.h"
#include "imgui.h"
#include "services/window/WindowManager.h"
#include "services/hotkey/HotKeyManager.h"
#include "ui/FullscreenSession.h"
#include <memory>
#include <algorithm>
#include <string>
#include "services/configuration/ConfigurationManager.h"
#include "services/logger/LogManager.h"
#include "services/texture/TextureManager.h"
#include "services/audio/AudioManager.h"

using namespace std;

int main()
{
    // Basic startup notice (no external logging dependency)
    // printf("GameBuilder2d starting up\n");

    gb2d::logging::LogManager::init({"GameBuilder2d", gb2d::logging::Level::info, "[%H:%M:%S] [%^%l%$] %v"});
    gb2d::logging::LogManager::info("Starting GameBuilder2d");
    bool configLoaded = gb2d::ConfigurationManager::load();

    if (!configLoaded) 
    {
        gb2d::logging::LogManager::warn("Configuration file missing or invalid; using defaults");
    }

    constexpr int kDefaultWidth = 1280;
    constexpr int kDefaultHeight = 720;
    constexpr int kDefaultFullscreenWidth = 1920;
    constexpr int kDefaultFullscreenHeight = 1080;

    int configWidth = static_cast<int>(gb2d::ConfigurationManager::getInt("window::width", kDefaultWidth));
    int configHeight = static_cast<int>(gb2d::ConfigurationManager::getInt("window::height", kDefaultHeight));
    bool startFullscreen = gb2d::ConfigurationManager::getBool("window::fullscreen", false);
    int fullscreenWidth = static_cast<int>(gb2d::ConfigurationManager::getInt("fullscreen::width", kDefaultFullscreenWidth));
    int fullscreenHeight = static_cast<int>(gb2d::ConfigurationManager::getInt("fullscreen::height", kDefaultFullscreenHeight));

    configWidth = std::max(configWidth, 320);
    configHeight = std::max(configHeight, 240);
    fullscreenWidth = std::max(fullscreenWidth, 320);
    fullscreenHeight = std::max(fullscreenHeight, 240);

    unsigned int flags = FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT;

    if (startFullscreen) {
        flags |= FLAG_FULLSCREEN_MODE;
    }

    SetConfigFlags(flags);
    InitWindow(configWidth, configHeight, "GameBuilder2d + rlImGui");

    if (startFullscreen && !IsWindowFullscreen()) {
        ToggleFullscreen();
    }

    if (startFullscreen) {
        SetWindowSize(fullscreenWidth, fullscreenHeight);
    } else {
        SetWindowSize(configWidth, configHeight);
    }

    gb2d::logging::LogManager::info(
        "Window initialized: {}x{} (fullscreen={}, editor={}x{}, session={}x{})",
        GetScreenWidth(),
        GetScreenHeight(),
        startFullscreen ? "true" : "false",
        configWidth,
        configHeight,
        fullscreenWidth,
        fullscreenHeight);
    SetTargetFPS(60);

    rlImGuiSetup(true);

    gb2d::textures::TextureManager::init();
    if (!gb2d::audio::AudioManager::init()) {
        gb2d::logging::LogManager::warn("AudioManager failed to initialize");
    }

    if (!gb2d::hotkeys::HotKeyManager::initialize()) {
        gb2d::logging::LogManager::error("HotKeyManager failed to initialize; shortcuts will be unavailable.");
    }

    // Enable docking in ImGui (after context is created)
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    gb2d::WindowManager wm{};
    gb2d::FullscreenSession fullscreenSession{};
    wm.setFullscreenSession(&fullscreenSession);

    // For testing: automatically open AudioManagerWindow
    wm.spawnWindowByType("audio_manager");

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        gb2d::audio::AudioManager::tick(dt);

        BeginDrawing();

        if (fullscreenSession.isActive()) {
            wm.syncHotkeySuppression(nullptr, false);
            gb2d::hotkeys::HotKeyManager::tick();
            fullscreenSession.tick(dt);
        } else {
            ClearBackground(DARKGRAY);
            rlImGuiBegin();
            ImGuiIO& io = ImGui::GetIO();
            wm.syncHotkeySuppression(&io, true);
            gb2d::hotkeys::HotKeyManager::tick();
            wm.renderUI();
            rlImGuiEnd();
        }

        const int screenWidth = GetScreenWidth();
        const int screenHeight = GetScreenHeight();
        std::string resolutionText = "Resolution: " + std::to_string(screenWidth) + "x" + std::to_string(screenHeight);
        constexpr int fontSize = 20;
        constexpr int padding = 6;
        const int textWidth = MeasureText(resolutionText.c_str(), fontSize);
        const int boxWidth = textWidth + padding * 2;
        const int boxHeight = fontSize + padding * 2;
        const int boxX = 10;
        const int boxY = 10;
        //DrawRectangle(boxX, boxY, boxWidth, boxHeight, Fade(BLACK, 0.6f));
        //DrawText(resolutionText.c_str(), boxX + padding, boxY + padding, fontSize, RAYWHITE);

        EndDrawing();
    }

    // printf("Exiting GameBuilder2d\n");
    // Save layout before shutting down ImGui
    fullscreenSession.requestStop();
    wm.saveLayout();
    gb2d::hotkeys::HotKeyManager::shutdown();
    gb2d::audio::AudioManager::shutdown();
    gb2d::textures::TextureManager::shutdown();
    rlImGuiShutdown();
    CloseWindow();
    gb2d::logging::LogManager::info("Shutting down GameBuilder2d");
    gb2d::logging::LogManager::shutdown();
    return 0;
}
