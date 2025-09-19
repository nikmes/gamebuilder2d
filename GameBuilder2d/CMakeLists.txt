# CMakeList.txt : CMake project for GameBuilder2d, include source and define
# project specific logic here.
#

# Add source to this project's executable.
add_executable (GameBuilder2d "GameBuilder2d.cpp" "GameBuilder2d.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET GameBuilder2d PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
