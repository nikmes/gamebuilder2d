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

Two options are available; both produce the same app with rlImGui enabled.

1) Visual Studio generator (explicit folder)
```powershell
cmake -S . -B build-vs-2022-x64-rlimgui -G "Visual Studio 17 2022" -A x64
cmake --build build-vs-2022-x64-rlimgui --config Debug --parallel

# Run
./build-vs-2022-x64-rlimgui/GameBuilder2d/Debug/GameBuilder2d.exe
```

2) CMake preset (one-liners)
```powershell
cmake --preset windows-vs2022-x64-debug
cmake --build --preset windows-vs2022-x64-debug

# Run
./build-vs-2022-x64-rlimgui/GameBuilder2d/Debug/GameBuilder2d.exe
```

Notes:
- MSVC builds may use parallel compilation; we set `/FS` to avoid PDB contention.
- All dependencies are fetched at configure time via CMake FetchContent.

## Build on WSL/Linux

Using Unix Makefiles (default tasks use this):
```powershell
wsl.exe -e bash -lc "cd /mnt/c/Users/<your-user>/source/repos/GameBuilder2d; cmake -S . -B build-linux -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Debug"
wsl.exe -e bash -lc "cd /mnt/c/Users/<your-user>/source/repos/GameBuilder2d; cmake --build build-linux -j"

# Run
wsl.exe -e bash -lc "cd /mnt/c/Users/<your-user>/source/repos/GameBuilder2d/build-linux/GameBuilder2d; ./GameBuilder2d"
```

Or with Ninja (optional):
```bash
sudo apt install -y ninja-build
cmake --preset linux-debug
cmake --build --preset linux-debug
```

## VS Code Tasks & Debug

- Tasks: see `.vscode/tasks.json` for WSL Configure/Build/Run tasks.
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
- `CMakePresets.json`: presets for Windows (VS 2022) and Linux
- `.vscode/tasks.json`: WSL tasks for configure/build/run

## Target
- `GameBuilder2d`: Raylib + ImGui prototype app.

### Run (WSL/Linux)
After building with Unix Makefiles:

```powershell
wsl.exe -e bash -lc "cd /mnt/c/Users/<your-user>/source/repos/GameBuilder2d/build-linux/GameBuilder2d; ./GameBuilder2d"
```

## Dependency notes

- raylib: built as a static lib; GLFW and platform specifics are handled automatically.
- rlImGui: brought in via `FetchContent_MakeAvailable`; we alias/export a stable `rlImGui` target.
- Dear ImGui: sources are fetched and compiled into a local static lib.
- Logging: none by default. The app currently avoids external logging dependencies; add your preferred logger if needed.

## Troubleshooting

- Windows link/PDB issues during parallel build
	- We set `/FS` on MSVC; if issues persist, try `--parallel 1` or clean the build dir.
- WSL X11/OpenGL errors
	- Ensure the listed X11/OpenGL/audio packages are installed.
	- For WSL1, run a local X server and export `DISPLAY` appropriately.
- Fetch/clone failures
	- Verify Internet access and that `git` is installed on both Windows and WSL.

## Presets overview

List available presets:
```powershell
cmake --list-presets
```

Key presets:
- `windows-vs2022-x64-debug`: Visual Studio 2022 x64, binary dir `build-vs-2022-x64-rlimgui`
- `linux-debug`: Ninja-based Linux Debug (requires `ninja-build`)

