cmake_minimum_required(VERSION 3.16)

include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.6.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(config_tests
  unit/config/test_placeholder.cpp
  unit/config/test_load_defaults.cpp
  unit/config/test_load_valid.cpp
  unit/config/test_save_reload.cpp
  unit/config/test_corrupt_fallback.cpp
  unit/config/test_env_overrides.cpp
  unit/config/test_atomic_save.cpp
  unit/config/test_change_notifications.cpp
  unit/config/test_export.cpp
  unit/config/test_boundaries.cpp
  unit/config/test_validate.cpp
  integration/test_config_e2e.cpp
)
target_include_directories(config_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/GameBuilder2d/src
)
target_link_libraries(config_tests PRIVATE Catch2::Catch2WithMain gb2d_configuration)
set_property(TARGET config_tests PROPERTY CXX_STANDARD 20)
target_compile_definitions(config_tests PRIVATE GB2D_INTERNAL_TESTING=1)

include(Catch)
catch_discover_tests(config_tests)