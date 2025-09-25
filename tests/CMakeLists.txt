cmake_minimum_required(VERSION 3.16)

include(FetchContent)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.6.0
)
FetchContent_MakeAvailable(Catch2)

add_executable(gb2d_tests
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
  unit/config/test_validate.cpp
  # Interop unit edge tests (Phase 3.2)
  unit/test_window_invalid_id.cpp
  unit/test_utf8_validation.cpp
  unit/test_max_contexts.cpp
  unit/test_dynamic_max_contexts.cpp
  integration/test_config_e2e.cpp
  # Interop contract tests (Phase 3.2)
  contract/test_status_codes.cpp
  contract/test_window_basic.cpp
  contract/test_window_title_validation.cpp
  contract/test_logging_basic.cpp
  contract/test_script_duplicate.cpp
  contract/test_not_initialized.cpp
  # Integration tests (Phase 3.2 user scenarios)
  integration/test_window_lifecycle.cpp
  integration/test_window_multi_close.cpp
  integration/test_logging_counters.cpp
  integration/test_reload_debounce.cpp
  integration/test_reload_isolation.cpp
  integration/test_unload_race.cpp
)
target_include_directories(gb2d_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/GameBuilder2d/src
  ${CMAKE_SOURCE_DIR}/GameBuilder2d/include
)
target_link_libraries(gb2d_tests PRIVATE Catch2::Catch2WithMain gb2d_configuration gb2d_interop gb2d_window gb2d_logging)
set_property(TARGET gb2d_tests PROPERTY CXX_STANDARD 20)
target_compile_definitions(gb2d_tests PRIVATE GB2D_INTERNAL_TESTING=1)

include(Catch)
catch_discover_tests(gb2d_tests)