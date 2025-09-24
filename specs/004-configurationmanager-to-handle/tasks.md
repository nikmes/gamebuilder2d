# Tasks: ConfigurationManager

**Input**: Design documents from `/specs/004-configurationmanager-to-handle/`
**Prerequisites**: plan.md (required), research.md, data-model.md, contracts/

## Execution Flow (main)
```
1. Load plan.md from feature directory → OK
2. Load optional design documents → OK: data-model.md, research.md, contracts/, quickstart.md
3. Generate tasks by category → below
4. Apply task rules → tests before implementation; [P] for independent files
5. Number tasks sequentially (T001...) → below
6. Dependencies → noted per section
7. Parallel examples → provided
8. Validate completeness → all gates satisfied
9. Return: SUCCESS
```

## Phase 3.1: Setup
- [x] T001 Configure CTest/Catch2 for repo (root `CMakeLists.txt`, `tests/`)
- [x] T002 [P] Add JSON library decision to plan (if needed) and wire build flags
- [x] T003 [P] Create `tests/unit/config/` folder structure

## Phase 3.2: Tests First (TDD)
- [x] T004 Unit test: defaults loadOrDefault in `tests/unit/config/test_load_defaults.cpp`
- [x] T005 Unit test: load existing valid file in `tests/unit/config/test_load_valid.cpp`
- [x] T006 Unit test: save + reload roundtrip in `tests/unit/config/test_save_reload.cpp`
- [x] T007 Unit test: corrupted file fallback + .bak in `tests/unit/config/test_corrupt_fallback.cpp`
- [x] T008 Unit test: env overrides `GB2D_` in `tests/unit/config/test_env_overrides.cpp`
- [x] T009 Unit test: atomic save (temp replace) in `tests/unit/config/test_atomic_save.cpp`
- [x] T010 Unit test: change notifications in `tests/unit/config/test_change_notifications.cpp`

## Phase 3.3: Core Implementation
- [x] T011 Header contract skeleton `src/services/configuration/ConfigurationManager.h`
- [x] T012 Implementation file `src/services/configuration/ConfigurationManager.cpp`
- [x] T013 [P] Path resolver utility `src/services/configuration/paths.h/.cpp`
- [x] T014 [P] JSON I/O utility `src/services/configuration/json_io.h/.cpp`
- [x] T015 Type validation and conversion helpers `src/services/configuration/validate.h/.cpp`
- [x] T016 Apply env overrides at load `src/services/configuration/ConfigurationManager.cpp`
- [x] T017 Implement atomic save with temp-replace `src/services/configuration/json_io.cpp`
- [x] T018 Implement migrations with `.bak` handling `src/services/configuration/ConfigurationManager.cpp`
- [x] T019 Wire change callbacks and invoke post-save `src/services/configuration/ConfigurationManager.cpp`

## Phase 3.4: Integration
- [x] T020 Integrate with startup: call `ConfigurationManager::loadOrDefault()` in `GameBuilder2d/src/GameBuilder2d.cpp`
- [x] T021 Log warnings on fallback/override/migration in `ConfigurationManager.cpp` (removed; ConfigurationManager is silent now)
- [x] T022 End-to-end test using temp directory in `tests/integration/test_config_e2e.cpp`

## Phase 3.5: Polish
- [ ] T023 [P] Unit tests: boundary conditions (1MB size, unsupported types) in `tests/unit/config/test_boundaries.cpp`
- [ ] T024 [P] Performance microbenchmark (<50ms) in `tests/unit/config/test_perf.cpp`
- [ ] T025 Update `specs/004-configurationmanager-to-handle/quickstart.md` with any final API changes
- [ ] T026 Cleanup docs and add examples to `README.md`

## Dependencies
- T001 before all tests
- T004-T010 before T011-T019 (TDD)
- T011 blocks T012; T013, T014 can run in parallel [P]
- T017 depends on T014; T016, T018, T019 depend on T012
- T020 depends on core implementation; T022 after T020

## Parallel Example
```
# Run these in parallel after T011:
Task: "Path resolver utility in src/services/configuration/paths.h/.cpp"  [P]
Task: "JSON I/O utility in src/services/configuration/json_io.h/.cpp"     [P]
```

## Validation Checklist
- [x] All contracts have corresponding tests (unit + integration)
- [x] All entities have model tasks
- [x] All tests come before implementation
- [x] Parallel tasks are independent
- [x] Each task specifies exact file paths
- [x] No [P] tasks modify the same file
