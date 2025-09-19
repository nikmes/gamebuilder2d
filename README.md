# GameBuilder2d

Simple cross-platform CMake C++ starter.

## Build (Windows, MSVC)

Requirements: Visual Studio 2022 with C++ build tools, CMake, Ninja (optional for presets).

```powershell
# Configure + build with Visual Studio generator
cmake -S . -B build-vs-2022-x64 -G "Visual Studio 17 2022" -A x64
cmake --build build-vs-2022-x64 --config Debug --parallel

# Or use presets (MSVC with Ninja via cl.exe)
cmake --preset x64-debug
cmake --build --preset x64-debug

# Run
./build-vs-2022-x64/GameBuilder2d/Debug/GameBuilder2d.exe
```

## Build (Linux or WSL)

Requirements: GCC or Clang, CMake, Ninja (recommended).

```bash
# On Linux/WSL
cmake --preset linux-debug
cmake --build --preset linux-debug
# Run
./out/build/linux-debug/GameBuilder2d/GameBuilder2d
```

## Notes
- The code is portable C++ and should compile on Windows, Linux, and macOS.
- Presets live in `CMakePresets.json`. Use `cmake --list-presets` to view them.
- For Release builds, use `linux-release` or `x64-release` accordingly.
