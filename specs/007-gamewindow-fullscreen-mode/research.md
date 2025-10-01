# Research Notes: GameWindow Fullscreen Mode

## Raylib APIs
- `ToggleFullscreen()` toggles between windowed and fullscreen modes; ensure window flags include `FLAG_FULLSCREEN_MODE` when entering.
- `SetWindowState()` / `ClearWindowState()` allow forcing flags without relying on toggle state.
- `SetWindowMonitor(int monitor)` can be used for multi-monitor support; default to primary monitor for v1.
- `GetScreenWidth()` / `GetScreenHeight()` return current framebuffer size after toggling.
- `SetWindowSize(int width, int height)` restores window dimensions when leaving fullscreen.

## ImGui Integration
- `rlImGuiBegin()` / `rlImGuiEnd()` wrap the ImGui frame; calling `rlImGuiEnd()` without `rlImGuiBegin()` crashes, so we must branch early.
- Input capture: ImGui consumes keyboard events when hovered/focused; during fullscreen mode ImGui is disabled, so Raylib receives all inputs.

## Configuration Manager
- Stores key/value pairs; existing usage (`window::fullscreen`) demonstrates persistence flow.
- Need to avoid rewriting unrelated keys when toggling fullscreen preference.

## UX Considerations
- Provide visual confirmation when entering fullscreen (overlay text such as "Ctrl+W to exit").
- Ensure hotkeys do not conflict with existing ImGui shortcuts; `Ctrl+W` is unused by default but check docking operations.
- Pausing other windows/tasks while fullscreen is active can prevent unexpected state changes.
