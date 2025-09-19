# CMakeList.txt : CMake project for GameBuilder2d, include source and define
# project specific logic here.
#

cmake_policy(SET CMP0077 NEW)

# Add source to this project's executable.
add_executable (GameBuilder2d "GameBuilder2d.cpp" "GameBuilder2d.h")

set_property(TARGET GameBuilder2d PROPERTY CXX_STANDARD 20)

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
  GIT_TAG v1.92.1
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

target_link_libraries(GameBuilder2d PRIVATE raylib rlImGui imgui)

if (WIN32)
  target_link_libraries(GameBuilder2d PRIVATE winmm)
endif()

# TODO: Add tests and install targets if needed.
