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
  - `src/window/Window.{h,cpp}`: Window model (id, title, state, optional `minSize`).
  - `src/window/DockRegion.{h,cpp}`, `src/window/Layout.{h,cpp}`: Current placeholders for future region/layout modeling beyond ImGui.

- Build:
  - CMake-based; links raylib, ImGui (docking), rlImGui, ImGuiFileDialog.

- Roadmap (short):
  - Tab grouping + active tab switching; keyboard-only navigation.
  - Enforce min sizes during user drags; configurable defaults UI.