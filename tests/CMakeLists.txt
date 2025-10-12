cmake_minimum_required(VERSION 3.16)

include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.6.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(config_tests
  test_bootstrap.cpp
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
  unit/config/test_apply_save_runtime.cpp
  unit/config/test_validate.cpp
  unit/config/test_editor_state.cpp
  integration/test_config_e2e.cpp
  integration/test_config_window_ui.cpp
)
target_include_directories(config_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/GameBuilder2d/src
)
target_link_libraries(config_tests PRIVATE Catch2::Catch2WithMain gb2d_configuration)
set_property(TARGET config_tests PROPERTY CXX_STANDARD 20)
target_compile_definitions(config_tests PRIVATE GB2D_INTERNAL_TESTING=1)

include(Catch)
catch_discover_tests(config_tests)

# Window JSON round-trip tests
add_executable(window_tests
  test_bootstrap.cpp
  unit/windows/test_window_json_roundtrip.cpp
  unit/windows/test_audio_manager_window.cpp
)
target_include_directories(window_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/GameBuilder2d/src
)
target_link_libraries(window_tests PRIVATE
  Catch2::Catch2WithMain
  gb2d_window
  gb2d_texture
  gb2d_audio
)
set_property(TARGET window_tests PROPERTY CXX_STANDARD 20)
target_compile_definitions(window_tests PRIVATE GB2D_INTERNAL_TESTING=1)

include(Catch)
catch_discover_tests(window_tests)

# Texture manager tests
add_executable(texture_tests
  test_bootstrap.cpp
  unit/texture/test_texture_manager.cpp
  integration/test_texture_atlas_integration.cpp
)
target_include_directories(texture_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/GameBuilder2d/src
)
target_link_libraries(texture_tests PRIVATE Catch2::Catch2WithMain gb2d_texture gb2d_configuration gb2d_logging)
set_property(TARGET texture_tests PROPERTY CXX_STANDARD 20)
target_compile_definitions(texture_tests PRIVATE GB2D_INTERNAL_TESTING=1)

include(Catch)
catch_discover_tests(texture_tests)

# Audio manager tests
add_executable(audio_tests
  test_bootstrap.cpp
  unit/audio/test_audio_manager_playback.cpp
)
target_include_directories(audio_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/GameBuilder2d/src
)
target_link_libraries(audio_tests PRIVATE Catch2::Catch2WithMain gb2d_audio gb2d_configuration gb2d_logging)
set_property(TARGET audio_tests PROPERTY CXX_STANDARD 20)
target_compile_definitions(audio_tests PRIVATE GB2D_INTERNAL_TESTING=1)

include(Catch)
catch_discover_tests(audio_tests)