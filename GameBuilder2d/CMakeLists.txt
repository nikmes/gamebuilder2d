# CMakeList.txt : CMake project for GameBuilder2d, include source and define
# project specific logic here.
#

cmake_policy(SET CMP0077 NEW)

# Add window management as a separate static library
add_library(gb2d_window STATIC
  "src/services/window/Window.h"
  "src/services/window/DockRegion.h"
  "src/services/window/Layout.h"
  "src/services/window/WindowManager.h"
  "src/services/window/Window.cpp"
  "src/services/window/DockRegion.cpp"
  "src/services/window/Layout.cpp"
  "src/services/window/WindowManager.cpp"
  # UI (modular window system scaffolding)
  "src/ui/Window.h"
  "src/ui/WindowContext.h"
  "src/ui/WindowRegistry.h"
  "src/ui/WindowRegistry.cpp"
  "src/ui/Windows/ConsoleLogWindow.h"
  "src/ui/Windows/ConsoleLogWindow.cpp"
  "src/ui/Windows/CodeEditorWindow.h"
  "src/ui/Windows/CodeEditorWindow.cpp"
  "src/ui/Windows/FilePreviewWindow.h"
  "src/ui/Windows/FilePreviewWindow.cpp"
  "src/ui/Windows/GameWindow.h"
  "src/ui/Windows/GameWindow.cpp"
  "src/ui/Windows/HotkeysWindow.h"
  "src/ui/Windows/HotkeysWindow.cpp"
  "src/ui/Windows/ConfigurationWindow.h"
  "src/ui/Windows/ConfigurationWindow.cpp"
  "src/ui/Windows/AudioManagerWindow.h"
  "src/ui/Windows/AudioManagerWindow.cpp"
  "src/ui/FullscreenSession.h"
  "src/ui/FullscreenSession.cpp"
)

# Add ImGui::Auto as a separate static library
add_library(gb2d_imgui_auto STATIC
  "src/ui/ImGuiAuto/ImGuiAuto.h"
  "src/ui/ImGuiAuto/ImGuiAuto.inl"
  "src/ui/ImGuiAuto/ImGuiAuto.cpp"
  "src/ui/ImGuiAuto/ImGuiAutoImpl.cpp"
  "src/ui/ImGuiAuto/ImGuiAutoDemo.h"
  "src/ui/ImGuiAuto/ImGuiAutoDemo.cpp"
)
target_include_directories(gb2d_window PUBLIC
  "src"
  "thirdparty/ImHotKey"
)
set_property(TARGET gb2d_window PROPERTY CXX_STANDARD 20)
target_compile_options(gb2d_window PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ImGuiColorTextEdit/imgui_compat_shim.h">
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-include ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ImGuiColorTextEdit/imgui_compat_shim.h>
)
 # Enable log console instrumentation (Phase 2 T2.1)
 target_compile_definitions(gb2d_window PRIVATE GB2D_LOG_CONSOLE_INSTRUMENT)
 
# Configure ImGui::Auto library
target_include_directories(gb2d_imgui_auto PUBLIC "src")
set_property(TARGET gb2d_imgui_auto PROPERTY CXX_STANDARD 20)
target_compile_options(gb2d_imgui_auto PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ImGuiColorTextEdit/imgui_compat_shim.h">
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-include ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ImGuiColorTextEdit/imgui_compat_shim.h>
)

# Add source to this project's executable (app entry only)
add_executable (GameBuilder2d
  "src/GameBuilder2d.cpp"
  "src/GameBuilder2d.h"
  "src/games/Game.h"
  "src/games/SpaceInvaders.h"
  "src/games/SpaceInvaders.cpp"
  "src/games/Galaga.h"
  "src/games/Galaga.cpp"
  "src/games/HarrierAttack.h"
  "src/games/HarrierAttack.cpp"
  "src/games/PacMan.h"
  "src/games/PacMan.cpp"
  "src/games/PlarformerGame.h"
  "src/games/PlarformerGame.cpp"
)

# Ensure a baseline config.json is available in the build tree
configure_file(
  "${CMAKE_SOURCE_DIR}/config.json"
  "${CMAKE_BINARY_DIR}/config.json"
  COPYONLY
)

set_property(TARGET GameBuilder2d PROPERTY CXX_STANDARD 20)
target_compile_options(GameBuilder2d PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ImGuiColorTextEdit/imgui_compat_shim.h">
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-include ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ImGuiColorTextEdit/imgui_compat_shim.h>
)

# Run the executable from its output directory when debugging in Visual Studio so
# the copied runtime assets (config.json, assets/) resolve via relative paths.
set_target_properties(GameBuilder2d PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:GameBuilder2d>"
)

# Copy config.json next to the built executable for first-run defaults
add_custom_command(
  TARGET GameBuilder2d POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/config.json"
          "$<TARGET_FILE_DIR:GameBuilder2d>/config.json"
  COMMENT "Copying default config.json to output directory"
)

if (EXISTS "${CMAKE_SOURCE_DIR}/assets")
  add_custom_command(
    TARGET GameBuilder2d POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:GameBuilder2d>/assets"
    COMMENT "Copying assets directory to output directory"
  )
endif()

include(FetchContent)

# Speed up dependency fetch & configure:
#  - Shallow clones (no full git history)
#  - Disable building examples/tests/benchmarks for third-party libs
#  These reduce first-time configure substantially, especially raylib & spdlog.

# raylib options
# Prefer turning off all optional builds (examples/games/tests) to reduce configure/build time
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
# Some raylib forks/cmake files also check namespaced options; set them too just in case
set(RAYLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(RAYLIB_BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(RAYLIB_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# Ensure GLFW (pulled by raylib) does not build extras
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

# spdlog options (avoid downloading fmt tests/examples inside spdlog build)
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)

# Fetch raylib (Populate then patch cmake_minimum_required to avoid deprecation warning)
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
if (POLICY CMP0169)
  cmake_policy(PUSH)
  cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_Populate(raylib)
if (POLICY CMP0169)
  cmake_policy(POP)
endif()
# Patch raylib's top-level CMakeLists to use a modern cmake_minimum_required range
if (EXISTS "${raylib_SOURCE_DIR}/CMakeLists.txt")
  file(READ "${raylib_SOURCE_DIR}/CMakeLists.txt" RAYLIB_CMAKELISTS)
  string(REPLACE "cmake_minimum_required(VERSION 3.5)" "cmake_minimum_required(VERSION 3.16...3.29)" RAYLIB_CMAKELISTS "${RAYLIB_CMAKELISTS}")
  file(WRITE "${raylib_SOURCE_DIR}/CMakeLists.txt" "${RAYLIB_CMAKELISTS}")
endif()

# Rename PlaySound symbol to avoid collision with WinMM on Windows; provide inline wrapper to keep API compatibility
if (EXISTS "${raylib_SOURCE_DIR}/src/raylib.h")
  file(READ "${raylib_SOURCE_DIR}/src/raylib.h" RAYLIB_HEADER_CONTENT)
  if (RAYLIB_HEADER_CONTENT MATCHES "RLAPI void PlaySound\\(Sound sound\\);")
    string(REPLACE "RLAPI void PlaySound(Sound sound);"
                   "RLAPI void rlPlaySound(Sound sound);\nstatic inline void PlaySound(Sound sound) { rlPlaySound(sound); }"
                   RAYLIB_HEADER_CONTENT "${RAYLIB_HEADER_CONTENT}")
    file(WRITE "${raylib_SOURCE_DIR}/src/raylib.h" "${RAYLIB_HEADER_CONTENT}")
  endif()
endif()

if (EXISTS "${raylib_SOURCE_DIR}/src/raudio.c")
  file(READ "${raylib_SOURCE_DIR}/src/raudio.c" RAYLIB_AUDIO_CONTENT)
  if (RAYLIB_AUDIO_CONTENT MATCHES "void PlaySound\\(Sound sound\\)")
    string(REPLACE "void PlaySound(Sound sound)" "void rlPlaySound(Sound sound)" RAYLIB_AUDIO_CONTENT "${RAYLIB_AUDIO_CONTENT}")
    file(WRITE "${raylib_SOURCE_DIR}/src/raudio.c" "${RAYLIB_AUDIO_CONTENT}")
  endif()
endif()
add_subdirectory(${raylib_SOURCE_DIR} ${raylib_BINARY_DIR})

# Fetch imgui sources and build as a static library
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG docking
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
# Use Populate instead of MakeAvailable to avoid configuring imgui as a separate CMake project
if (POLICY CMP0169)
  cmake_policy(PUSH)
  cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_Populate(imgui)
if (POLICY CMP0169)
  cmake_policy(POP)
endif()
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
)
option(GB2D_WITH_IMGUI_DEMO "Include Dear ImGui demo & debug windows (omitted in Release automatically)" ON)
# Only add demo source for non-Release builds when option is ON. This avoids compiling a large translation unit in Release and reduces binary size.
if (GB2D_WITH_IMGUI_DEMO)
  target_sources(imgui PRIVATE
    $<$<NOT:$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>>:${imgui_SOURCE_DIR}/imgui_demo.cpp>
  )
endif()
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
# Strip demo & debug tools in optimized configs (even if source added conditionally above, these disable runtime exposure)
target_compile_definitions(imgui PUBLIC
  $<$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>:IMGUI_DISABLE_DEMO_WINDOWS IMGUI_DISABLE_DEBUG_TOOLS>
)

# JSON library (nlohmann/json)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# spdlog (logging backend)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.12.0
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(spdlog)

# Logging service library
add_library(gb2d_logging
  "src/services/logger/LogManager.h"
  "src/services/logger/LogManager.cpp"
  "src/services/logger/ImGuiLogSink.h"
  "src/services/logger/ImGuiLogSink.cpp"
)
target_include_directories(gb2d_logging PUBLIC "src")
set_property(TARGET gb2d_logging PROPERTY CXX_STANDARD 20)
target_link_libraries(gb2d_logging PUBLIC spdlog::spdlog)

# Configuration service library
add_library(gb2d_configuration
  "src/services/configuration/ConfigurationManager.h"
  "src/services/configuration/ConfigurationManager.cpp"
  "src/services/configuration/ConfigurationEditorState.h"
  "src/services/configuration/ConfigurationEditorState.cpp"
  "src/services/configuration/ConfigurationSchema.h"
  "src/services/configuration/ConfigurationSchema.cpp"
  "src/services/configuration/paths.h"
  "src/services/configuration/paths.cpp"
  "src/services/configuration/json_io.h"
  "src/services/configuration/json_io.cpp"
  "src/services/configuration/validate.h"
  "src/services/configuration/validate.cpp"
)
target_include_directories(gb2d_configuration PUBLIC "src")
set_property(TARGET gb2d_configuration PROPERTY CXX_STANDARD 20)
target_link_libraries(gb2d_configuration PUBLIC nlohmann_json::nlohmann_json)

# Texture service library
add_library(gb2d_texture
  "src/services/texture/TextureManager.h"
  "src/services/texture/TextureManager.cpp"
)
target_include_directories(gb2d_texture PUBLIC "src")
set_property(TARGET gb2d_texture PROPERTY CXX_STANDARD 20)
target_link_libraries(gb2d_texture PUBLIC raylib gb2d_configuration gb2d_logging nlohmann_json::nlohmann_json)

# Audio service library
add_library(gb2d_audio
  "src/services/audio/AudioManager.h"
  "src/services/audio/AudioManager.cpp"
)
target_include_directories(gb2d_audio PUBLIC "src")
set_property(TARGET gb2d_audio PROPERTY CXX_STANDARD 20)
target_link_libraries(gb2d_audio PUBLIC raylib gb2d_configuration gb2d_logging)

# HotKey service library
add_library(gb2d_hotkeys
  "src/services/hotkey/HotKeyManager.h"
  "src/services/hotkey/HotKeyManager.cpp"
  "src/services/hotkey/HotKeyCatalog.h"
  "src/services/hotkey/HotKeyCatalog.cpp"
  "src/services/hotkey/HotKeyRegistrationBuilder.h"
  "src/services/hotkey/HotKeyRegistrationBuilder.cpp"
  "src/services/hotkey/HotKeyActions.h"
  "src/services/hotkey/ShortcutUtils.h"
  "src/services/hotkey/ShortcutUtils.cpp"
)
target_include_directories(gb2d_hotkeys PUBLIC
  "src"
  "thirdparty/ImHotKey"
  ${raylib_SOURCE_DIR}/src
  ${raylib_BINARY_DIR}/src
)
set_property(TARGET gb2d_hotkeys PROPERTY CXX_STANDARD 20)
target_link_libraries(gb2d_hotkeys PUBLIC raylib nlohmann_json::nlohmann_json gb2d_configuration gb2d_logging)

# ImGuiFileDialog
FetchContent_Declare(
  imguifiledialog
  GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog.git
  GIT_TAG master
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
if (POLICY CMP0169)
  cmake_policy(PUSH)
  cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_Populate(imguifiledialog)
if (POLICY CMP0169)
  cmake_policy(POP)
endif()
add_library(imguifiledialog STATIC
  ${imguifiledialog_SOURCE_DIR}/ImGuiFileDialog.cpp
)
target_include_directories(imguifiledialog PUBLIC ${imguifiledialog_SOURCE_DIR})
target_link_libraries(imguifiledialog PUBLIC imgui)

# ImGuiColorTextEdit (syntax highlighting editor)
FetchContent_Declare(
  imguicolortextedit
  GIT_REPOSITORY https://github.com/BalazsJako/ImGuiColorTextEdit.git
  GIT_TAG master
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
if (POLICY CMP0169)
  cmake_policy(PUSH)
  cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_Populate(imguicolortextedit)
if (POLICY CMP0169)
  cmake_policy(POP)
endif()
add_library(imguicolortextedit STATIC
  ${imguicolortextedit_SOURCE_DIR}/TextEditor.cpp
)
target_include_directories(imguicolortextedit PUBLIC ${imguicolortextedit_SOURCE_DIR})
# no special defines needed; the shim includes upstream by name
target_link_libraries(imguicolortextedit PUBLIC imgui)

# Force-include a tiny compatibility shim to provide removed ImGui APIs
target_compile_options(imguicolortextedit PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ImGuiColorTextEdit/imgui_compat_shim.h">
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-include ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ImGuiColorTextEdit/imgui_compat_shim.h>
)

# rlImGui via FetchContent_MakeAvailable (upstream provides CMake)
FetchContent_Declare(
  rlimgui
  GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
  GIT_TAG main
  GIT_SHALLOW TRUE
  GIT_PROGRESS TRUE
)
# Avoid configuring rlImGui as a subproject; just populate sources and build our own target
if (POLICY CMP0169)
  cmake_policy(PUSH)
  cmake_policy(SET CMP0169 OLD)
endif()
FetchContent_Populate(rlimgui)
if (POLICY CMP0169)
  cmake_policy(POP)
endif()
target_include_directories(GameBuilder2d PRIVATE ${rlimgui_SOURCE_DIR})

# Provide a consistent target `rlImGui`
add_library(rlImGui STATIC
  ${rlimgui_SOURCE_DIR}/rlImGui.cpp
  ${rlimgui_SOURCE_DIR}/rlImGui.h
  ${rlimgui_SOURCE_DIR}/imgui_impl_raylib.h
)
target_include_directories(rlImGui PUBLIC ${rlimgui_SOURCE_DIR})
target_compile_definitions(rlImGui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(rlImGui PUBLIC raylib imgui)

# ImGui::Auto library depends on ImGui
target_link_libraries(gb2d_imgui_auto PUBLIC imgui nlohmann_json::nlohmann_json)

# Window library depends on ImGui, file dialog, raylib
target_link_libraries(gb2d_window PUBLIC imgui imguifiledialog raylib gb2d_logging imguicolortextedit nlohmann_json::nlohmann_json gb2d_hotkeys)

target_link_libraries(GameBuilder2d PRIVATE raylib rlImGui imgui imguifiledialog gb2d_configuration gb2d_window gb2d_logging gb2d_texture gb2d_audio gb2d_imgui_auto gb2d_hotkeys)

# TODO: Add tests and install targets if needed.
