# GameBuilder2d (Editor)

A minimal editor shell built with C++20, raylib, rlImGui, and ImGui (docking branch).

- Components:
  - `src/GameBuilder2d.cpp`/`GameBuilder2d.h`: App entry. Initializes raylib + rlImGui, enables ImGui docking, runs the UI loop, saves layout on shutdown.
  - `src/window/WindowManager.{h,cpp}`: Dockable window system.
    - Full-screen DockSpace; default layout (Scene / Inspector / Console).
    - Drag handle per window + on-screen dock targets overlay for DnD docking.
    - Explicit docking API (Left/Right/Top/Bottom/Center-as-tab).
    - Split min-size guards (defaults 200×120) with per-window overrides.
    - Layouts menu: Save/Load/Delete (+ confirmation), auto-load "last", toast notifications.
    - File menu: ImGuiFileDialog (remembers last folder), Open Recent MRU, text/image previews.
  - `src/ui/FullscreenSession.{h,cpp}`: Manages fullscreen gameplay sessions (enter/exit hooks, tick loop, window restoration).
  - `src/ui/Windows/GameWindow.{h,cpp}`: Launches embedded games, exposes the fullscreen toggle, and surfaces the resume-on-launch preference.
  - `src/window/Window.{h,cpp}`: Window model (id, title, state, optional `minSize`).
  - `src/window/DockRegion.{h,cpp}`, `src/window/Layout.{h,cpp}`: Current placeholders for future region/layout modeling beyond ImGui.

- Build:
  - CMake-based; links raylib, ImGui (docking), rlImGui, ImGuiFileDialog.

  ## Documentation

  - Audio subsystem configuration: `docs/audio-manager.md`
  - Audio Manager window workflow: `docs/audio-manager-window.md`

## Editor window configuration

Use these keys to control how the editor window starts up:

| Key | Type | Purpose |
| --- | --- | --- |
| `window::width` | int | Preferred window width when launching in windowed mode. |
| `window::height` | int | Preferred window height when launching in windowed mode. |
| `window::fullscreen` | bool | If `true`, the editor toggles into fullscreen right after initialization. |

## Fullscreen session configuration

The embedded games can target their own fullscreen resolution and resume behaviour:

| Key | Type | Purpose |
| --- | --- | --- |
| `fullscreen::width` | int | Target width when the Game Window enters fullscreen playback. |
| `fullscreen::height` | int | Target height when the Game Window enters fullscreen playback. |
| `window::resume_fullscreen` | bool | When enabled (via the Game Window checkbox) the editor automatically re-enters fullscreen on startup. |
| `window::fullscreen_last_game` | string | Tracks the last game id to resume in fullscreen. |

All configuration values are stored via `ConfigurationManager::save()` / `ConfigurationManager::load()`.

- Roadmap (short):
  - Tab grouping + active tab switching; keyboard-only navigation.
  - Enforce min sizes during user drags; configurable defaults UI.