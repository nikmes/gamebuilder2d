# GameBuilder2d

Cross-platform C++ workspace app powered by raylib, rlImGui, and Dear ImGui. GameBuilder2d launches into a dockable editor surface with built-in windows (console, code editor, file preview, and a Space Invaders mini-game). Windows and WSL/Linux builds are first-class through CMake presets.

## Repository quick facts

- Repo: <https://github.com/nikmes/gamebuilder2d>
- Default branch: `main`
- License: MIT

Clone the project:

```powershell
git clone https://github.com/nikmes/gamebuilder2d.git
cd gamebuilder2d
```

## Feature highlights

- **Dockable ImGui workspace** with layouts saved to `out/layouts/*.layout.json`. Layouts restore automatically on startup.
- **Modular window registry** with the following built-ins:
  - Console log (spdlog tail with filtering/search/autoscroll)
  - Syntax-highlighted code editor with multi-tab support and ImGuiFileDialog integration
  - File previewer for text and common image formats
  - Space Invaders playground rendered into a raylib `RenderTexture`
- **Configuration service** backed by JSON, env overrides (`GB2D_*`), validation, and change notifications.
- **Logging service** (spdlog + ImGui sink) reused by the UI and runtime systems.
- **Composable static libraries**: `gb2d_window`, `gb2d_configuration`, and `gb2d_logging` feed into the main executable.
- **Extensive tests** covering configuration logic and window layout serialization (Catch2).

## Prerequisites

### Windows

- Visual Studio 2022 with Desktop C++ workload
- CMake ≥ 3.16 (VS 2022 bundle is fine)
- Git

### WSL (Ubuntu)

- Packages: `build-essential cmake git xorg-dev libxi-dev libxrandr-dev libxinerama-dev libxcursor-dev libgl1-mesa-dev libglu1-mesa-dev libasound2-dev libpulse-dev`

Install from PowerShell:

```powershell
wsl.exe -e bash -lc "sudo apt update"
wsl.exe -e bash -lc "sudo apt install -y build-essential cmake git xorg-dev libxi-dev libxrandr-dev libxinerama-dev libxcursor-dev libgl1-mesa-dev libglu1-mesa-dev libasound2-dev libpulse-dev"
```

> ℹ️ WSLg on Windows 11 works out of the box. For WSL1 or legacy setups, start an X server and export `DISPLAY` before launching the app.

## Building

### Windows (Visual Studio generator)

```powershell
cmake --preset windows-vs2022-x64-debug
cmake --build --preset windows-vs2022-x64-debug --config Debug
```

Run the executable:

```powershell
./build-vs-2022-x64-debug/GameBuilder2d/Debug/GameBuilder2d.exe
```

Use `windows-vs2022-x64-release` for an optimized build. All dependencies are fetched during configure via CMake `FetchContent` (shallow clones, samples/tests disabled).

### WSL / Linux (Unix Makefiles)

From Windows PowerShell:

```powershell
wsl.exe -e bash -lc "cd /mnt/c/Users/<your-user>/source/repos/GameBuilder2d && cmake --preset wsl-debug"
wsl.exe -e bash -lc "cd /mnt/c/Users/<your-user>/source/repos/GameBuilder2d && cmake --build --preset wsl-debug -j"
wsl.exe -e bash -lc "cd /mnt/c/Users/<your-user>/source/repos/GameBuilder2d/build-linux-debug/GameBuilder2d && ./GameBuilder2d"
```

Working directly in WSL:

```bash
cmake --preset wsl-debug
cmake --build --preset wsl-debug -j
./build-linux-debug/GameBuilder2d/GameBuilder2d
```

Switch to `wsl-release` for optimized builds.

### VS Code integration

- `.vscode/tasks.json` exposes `WSL: Configure/Build/Run` wrappers that call the presets above.
- The CMake Tools extension auto-selects `windows-vs2022-x64-debug`; adjust with **CMake: Select Configure Preset**.
- Launch configs include "Debug GameBuilder2d (VS)" and a GDB-based WSL configuration.

## Running tests

Catch2-based tests verify configuration edge cases and window layout round-trips.

```powershell
cmake --preset windows-vs2022-x64-debug-tests
cmake --build --preset windows-vs2022-x64-debug-tests --config Debug
ctest --preset windows-vs2022-x64-debug-tests --output-on-failure
```

On WSL, reconfigure with `-DBUILD_TESTING=ON` or use the `windows-vs2022-x64-release-tests` / `wsl-release` equivalents. Test executables (`config_tests`, `window_tests`) also live in the preset build folders.

## Configuration & layouts

- Default config path: `config.json` in the repo root. Override with `GB2D_CONFIG_DIR`.
- Keys use dotted syntax (`window.fullscreen`). Callers can also pass `window::fullscreen`; the service normalizes namespaces.
- Environment overrides: `GB2D_<SECTION>__<KEY>=value` (double underscore becomes dot). Values auto-detect booleans, integers, and floats.
- Layout exports live in `out/layouts/<name>.{wm.txt,imgui.ini,layout.json}`. The manager restores `last` automatically on startup and backs up corrupted layouts.

## TextureManager quickstart

- Initialized once during app bootstrap (`TextureManager::init` / `shutdown` already wired in `GameBuilder2d.cpp`).
- Configure search paths, filters, mipmap behaviour, and placeholder assets via the `textures::` section in `config.json`.
- Request textures through `TextureManager::acquire(identifier, alias)` and pair every successful call with `TextureManager::release(key)`.
- Handle placeholder returns with `AcquireResult::placeholder` to signal missing art in the UI.
- Use `TextureManager::metrics()` and `reloadAll()` to power diagnostics overlays or asset-refresh workflows.

👉 See the [TextureManager developer guide](GameBuilder2d/docs/texture-manager.md) for end‑to‑end examples, configuration tables, and the current roadmap.

## AudioManager quickstart

- `AudioManager::init()`/`shutdown()` already run during app bootstrap; call `AudioManager::tick()` each frame to keep music streams updated (wired in `GameBuilder2d.cpp`).
- The `audio` block in `config.json` controls enablement, global volumes, alias slot limits, search paths, and optional preload lists for sounds and music.
- Disabled or unavailable devices put the manager into silent mode—playback requests no-op but still resolve handles so callers can proceed safely.
- Use `AudioManager::reloadAll()` after adjusting search paths or preload lists at runtime to refresh the caches.

👉 See the [AudioManager configuration guide](GameBuilder2d/docs/audio-manager.md) for detailed key descriptions, defaults, and environment override examples.

## Dependencies

- **raylib 5.5** (graphics/input) with GLFW extras disabled for faster builds
- **rlImGui** (Dear ImGui renderer for raylib)
- **Dear ImGui (docking branch)** packaged as a static lib; demo widgets omitted in Release
- **ImGuiFileDialog** & **ImGuiColorTextEdit** for file dialogs + code editor
- **spdlog 1.12.0** for logging (plus custom ImGui sink)
- **nlohmann/json 3.11.3** for configuration and layout persistence

All third-party code is fetched via CMake `FetchContent` with shallow clones.

## Project structure overview

- `CMakeLists.txt` (root) – top-level project, adds the app and optional tests
- `GameBuilder2d/CMakeLists.txt` – static library targets + app wiring, dependency setup
- `GameBuilder2d/src/GameBuilder2d.cpp` – main loop (raylib + rlImGui + modular window manager)
- `GameBuilder2d/src/services/*` – configuration, logging, window manager implementations
- `GameBuilder2d/src/ui/Windows/*` – modular ImGui windows (console, editor, preview, space invaders)
- `tests/` – Catch2 suites for configuration and window serialization
- `specs/` – design docs and work-in-progress specs

## Troubleshooting & tips

- **MSVC PDB contention**: `/FS` is enabled globally; if issues persist, build with `--parallel 1` or clean the build folder.
- **WSL rendering issues**: ensure the package list above is installed and `DISPLAY` is set (WSLg handles this automatically).
- **Preset drift**: remove stale build directories (`build-vs-2022-*`, `build-linux-*`) when switching presets or branches.
- **Config recovery**: corrupted configs are renamed to `config.json.bak`; defaults reload automatically.
- **Log history**: adjust UI buffer capacity from the Console window (or via `gb2d::logging::set_log_buffer_capacity`).

## Preset catalog

List all presets:

```powershell
cmake --list-presets
```

Key presets:

- `windows-vs2022-x64-debug` / `windows-vs2022-x64-release`
- `windows-vs2022-x64-debug-tests` / `windows-vs2022-x64-release-tests`
- `wsl-debug` / `wsl-release`

Each preset creates a single-configuration build directory, keeping configure times low and avoiding Visual Studio multi-config overhead.

