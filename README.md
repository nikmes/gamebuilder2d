# GameBuilder2d

Cross-platform C++ app using raylib + rlImGui + Dear ImGui, built with CMake. Windows and WSL/Linux are supported out of the box.

## Repository

- Repo: https://github.com/nikmes/gamebuilder2d
- Default branch: `main`

Clone:
```powershell
git clone https://github.com/nikmes/gamebuilder2d.git
cd gamebuilder2d
```

## Prerequisites

- Windows: Visual Studio 2022 (Desktop C++), CMake 3.16+, Git
- WSL (Ubuntu): build-essential, cmake, X11/OpenGL/audio dev packages
	- Install (Ubuntu):
		```bash
		sudo apt update
		sudo apt install -y build-essential cmake git \
			xorg-dev libxi-dev libxrandr-dev libxinerama-dev libxcursor-dev \
			libgl1-mesa-dev libglu1-mesa-dev libasound2-dev libpulse-dev
		```
- GPU/Display on WSL: enable an X server if running WSL1 or older; WSLg on Windows 11 works out of the box.

## Build on Windows (VS 2022)

Using CMake preset (recommended):
```powershell
cmake --preset windows-vs2022-x64-debug
cmake --build --preset windows-vs2022-x64-debug --config Debug

# Run
./build-vs-2022-x64-debug/GameBuilder2d/Debug/GameBuilder2d.exe
```

Notes:
- MSVC builds may use parallel compilation; if you hit PDB contention, add `/FS` or reduce parallelism.
- All dependencies are fetched at configure time via CMake FetchContent.
- Presets generate a single configuration (Debug or Release) for Visual Studio to speed up configure.

## Build on WSL/Linux

Using CMake Presets (recommended):

From Windows PowerShell (invoking WSL):
```powershell
wsl.exe -e bash -lc "cd /mnt/c/Users/<your-user>/source/repos/GameBuilder2d; cmake --preset wsl-debug"
wsl.exe -e bash -lc "cd /mnt/c/Users/<your-user>/source/repos/GameBuilder2d; cmake --build --preset wsl-debug -j"

# Run (Debug)
wsl.exe -e bash -lc "cd /mnt/c/Users/<your-user>/source/repos/GameBuilder2d/build-linux-debug/GameBuilder2d; ./GameBuilder2d"
```

From a WSL shell directly:
```bash
cmake --preset wsl-debug
cmake --build --preset wsl-debug -j

# Run (Debug)
./build-linux-debug/GameBuilder2d/GameBuilder2d
```

Release variant:
```bash
cmake --preset wsl-release
cmake --build --preset wsl-release -j

# Run (Release)
./build-linux-release/GameBuilder2d/GameBuilder2d
```

## VS Code Tasks & Debug

- Tasks: see `.vscode/tasks.json` for WSL Configure/Build/Run tasks. Tasks now use the `wsl-debug` and `wsl-release` presets.
- Debugging (WSL): a gdb-based launch configuration is recommended (requires `gdb` in WSL).

## VS Code: CMake Tools

The repo includes CMake Presets and VS Code settings for [CMake Tools]. On open, the extension will auto-configure with the preset `windows-vs2022-x64-debug`.

- Configure/Build: use the CMake Tools status bar to select preset and build, or run:
	- Configure: `CMake: Configure`
	- Build: `CMake: Build`
- Presets: change via `CMake: Select Configure Preset`.

Settings are in `.vscode/settings.json`:
- `cmake.useCMakePresets`: `"always"`
- `cmake.configurePreset`: `windows-vs2022-x64-debug`
- `cmake.buildPreset`: `windows-vs2022-x64-debug`

Debugging on Windows:
- Use VS Code launch config: "Debug GameBuilder2d (VS)".
- It will build with the preset and start the executable.

Alternative: Debug the selected CMake target
- Select the target in the CMake Tools status bar (e.g., GameBuilder2d).
- Use the launch config: "CMake: Debug current target (Windows)".
- This uses `${command:cmake.launchTargetPath}` so it tracks whichever target is active.

## Project structure

- `CMakeLists.txt` (root): project, MSVC config, subdirectory include
- `GameBuilder2d/CMakeLists.txt`: app target, FetchContent for `raylib`, `imgui`, `rlImGui`
- `GameBuilder2d/GameBuilder2d.cpp`: rlImGui demo window loop
- `CMakePresets.json`: presets for Windows (VS 2022) and WSL (Unix Makefiles)
- `.vscode/tasks.json`: WSL tasks for configure/build/run

## Target
- `GameBuilder2d`: Raylib + ImGui prototype app.

### Run (WSL/Linux)
After building with the debug preset:

```powershell
wsl.exe -e bash -lc "cd /mnt/c/Users/<your-user>/source/repos/GameBuilder2d/build-linux-debug/GameBuilder2d; ./GameBuilder2d"
```

## Dependency notes

- raylib: built as a static lib; GLFW and platform specifics are handled automatically.
- rlImGui: fetched with CMake FetchContent and built locally as a small static lib target `rlImGui`.
- Dear ImGui: sources are fetched and compiled into a local static lib.
- Logging: spdlog-based logging is included (`spdlog::spdlog`).
- GLFW extras disabled: `GLFW_BUILD_DOCS=OFF`, `GLFW_BUILD_TESTS=OFF`, `GLFW_BUILD_EXAMPLES=OFF`, and `GLFW_INSTALL=OFF` are set before fetching raylib to slim dependency builds.
- raylib extras disabled: `BUILD_EXAMPLES=OFF`, `BUILD_GAMES=OFF`, `BUILD_TESTING=OFF` (and `RAYLIB_BUILD_*` equivalents) are forced OFF so we only build the library.
- We use shallow clones and avoid configuring unnecessary subprojects to speed up first-time configure.

## Troubleshooting

- Windows link/PDB issues during parallel build
	- We set `/FS` on MSVC; if issues persist, try `--parallel 1` or clean the build dir.
- WSL X11/OpenGL errors
	- Ensure the listed X11/OpenGL/audio packages are installed.
	- For WSL1, run a local X server and export `DISPLAY` appropriately.
- Fetch/clone failures
	- Verify Internet access and that `git` is installed on both Windows and WSL.
- Preset changes not taking effect or slow configure
	- Delete stale build folders after changing presets/options, then re-configure:
		```powershell
		Remove-Item -Recurse -Force .\build-vs-2022-x64-debug, .\build-vs-2022-x64-release -ErrorAction SilentlyContinue
		```

## Presets overview

List available presets:
```powershell
cmake --list-presets
```

Key presets:
- `windows-vs2022-x64-debug`: Visual Studio 2022 x64, binary dir `build-vs-2022-x64-debug`
- `windows-vs2022-x64-release`: Visual Studio 2022 x64, binary dir `build-vs-2022-x64-release`
- `wsl-debug`: Unix Makefiles (Debug), binary dir `build-linux-debug`
- `wsl-release`: Unix Makefiles (Release), binary dir `build-linux-release`

