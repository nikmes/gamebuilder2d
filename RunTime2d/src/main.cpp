#include "raylib.h"
#ifdef GB2D_EXPERIMENT_WINDOW_MANAGER
#include "../../GameBuilder2d/src/window/WindowManager.h"
#endif

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 450, "RunTime2d (raylib)");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("RunTime2d: Hello, raylib!", 190, 200, 20, DARKGRAY);
#ifdef GB2D_EXPERIMENT_WINDOW_MANAGER
        static gb2d::WindowManager wm{}; // unused stub instance for now
#endif
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
