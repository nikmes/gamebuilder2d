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
)
target_include_directories(gb2d_window PUBLIC "src")
set_property(TARGET gb2d_window PROPERTY CXX_STANDARD 20)
target_compile_options(gb2d_window PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ImGuiColorTextEdit/imgui_compat_shim.h">
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-include ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ImGuiColorTextEdit/imgui_compat_shim.h>
)
 # Enable log console instrumentation (Phase 2 T2.1)
 target_compile_definitions(gb2d_window PRIVATE GB2D_LOG_CONSOLE_INSTRUMENT)

# Add source to this project's executable (app entry only)
add_executable (GameBuilder2d
  "src/GameBuilder2d.cpp"
  "src/GameBuilder2d.h"
)

set_property(TARGET GameBuilder2d PROPERTY CXX_STANDARD 20)
target_compile_options(GameBuilder2d PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/FI"${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ImGuiColorTextEdit/imgui_compat_shim.h">
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-include ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/ImGuiColorTextEdit/imgui_compat_shim.h>
)

include(FetchContent)

# Fetch raylib
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# Fetch imgui sources and build as a static library
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)
FetchContent_GetProperties(imgui)
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
target_compile_definitions(imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)

# JSON library (nlohmann/json)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# spdlog (logging backend)
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.12.0
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

## lwlog removed

# ImGuiFileDialog
FetchContent_Declare(
  imguifiledialog
  GIT_REPOSITORY https://github.com/aiekick/ImGuiFileDialog.git
  GIT_TAG master
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
)
FetchContent_MakeAvailable(rlimgui)
FetchContent_GetProperties(rlimgui)
if (rlimgui_SOURCE_DIR)
  target_include_directories(GameBuilder2d PRIVATE ${rlimgui_SOURCE_DIR})
endif()

# Ensure a consistent target name `rlImGui` exists for linking
if (TARGET rlimgui AND NOT TARGET rlImGui)
  add_library(rlImGui ALIAS rlimgui)
endif()

if (NOT TARGET rlimgui AND NOT TARGET rlImGui)
  add_library(rlImGui STATIC
    ${rlimgui_SOURCE_DIR}/rlImGui.cpp
    ${rlimgui_SOURCE_DIR}/rlImGui.h
    ${rlimgui_SOURCE_DIR}/imgui_impl_raylib.h
  )
  target_include_directories(rlImGui PUBLIC ${rlimgui_SOURCE_DIR})
  target_compile_definitions(rlImGui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)
  target_link_libraries(rlImGui PUBLIC raylib imgui)
endif()

# Window library depends on ImGui, file dialog, and raylib
target_link_libraries(gb2d_window PUBLIC imgui imguifiledialog raylib gb2d_logging imguicolortextedit)

target_link_libraries(GameBuilder2d PRIVATE raylib rlImGui imgui imguifiledialog gb2d_configuration gb2d_window gb2d_logging)

if (WIN32)
  target_link_libraries(GameBuilder2d PRIVATE winmm)
endif()

# TODO: Add tests and install targets if needed.
