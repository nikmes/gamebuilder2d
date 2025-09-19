# CMakeList.txt : CMake project for RunTime2d, raylib-based minimal app.

# Keep policy consistent with sibling project
cmake_policy(SET CMP0077 NEW)

add_executable(RunTime2d
    main.cpp
)

set_property(TARGET RunTime2d PROPERTY CXX_STANDARD 20)

# Fetch raylib if not already provided by a sibling subproject
include(FetchContent)
if (NOT TARGET raylib)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
    )
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(raylib)
endif()

target_link_libraries(RunTime2d PRIVATE raylib)

if (WIN32)
    target_link_libraries(RunTime2d PRIVATE winmm)
endif()
