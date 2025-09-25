# Tasks: Embedded .NET Runtime (Managed → Native Interop Only)

**Input**: Design docs in `specs/006-embed-dotnet-runtime/` (spec.md, plan.md, research.md, data-model.md, contracts/)
**Prerequisites**: plan & research locked; scope excludes native→managed invocation.

## Execution Flow (summary)
Setup → Tests (contract + integration) → Core implementation → Integration glue → Polish & docs.

## Phase 3.1: Setup
- [x] T001 Create `GameBuilder2d/src/bridge/` directory (if absent) and placeholder headers (`gb2d_window_api.h`, `gb2d_logging_api.h`). (gb2d_interop.h already created) (DONE: directory + headers exist; logging API implemented in T022; window API still pending functional impl under T023)
- [x] T002 Add interop build integration to root `GameBuilder2d/CMakeLists.txt` (export symbols, visibility settings).
- [x] T003 [P] Introduce `include/gb2d/interop/gb2d_status_codes.h` with enum and version macro `GB2D_INTEROP_API_VERSION`.
- [x] T004 [P] Add config keys parsing scaffold (`scripting.runtimeSearchPaths`, `scripting.maxContexts`, `scripting.reload.debounceMs`). (DONE: defaults + getters in ConfigurationManager::loadOrDefault; tests cover defaults.)

## Phase 3.2: Tests First (TDD) – MUST FAIL INITIALLY
Contract Tests (from contracts directory)
- [x] T005 [P] Contract test: status codes mapping coverage in `tests/contract/test_status_codes.cpp`. (Implemented & passing)
- [x] T006 [P] Contract test: window create/set/close happy path in `tests/contract/test_window_basic.cpp` (now asserts behavior; failing until impl).
- [x] T007 [P] Contract test: logging info/warn/error path in `tests/contract/test_logging_basic.cpp` (now asserts behavior; failing until impl).
- [x] T008 [P] Contract test: duplicate script load returns ALREADY_LOADED in `tests/contract/test_script_duplicate.cpp` (asserts behavior; first load passes now, duplicate returns ALREADY_LOADED).
- [x] T009 [P] Contract test: NOT_INITIALIZED behavior before runtime init in `tests/contract/test_not_initialized.cpp` (needs update to call reset API).

Integration Tests (User Scenarios)
- [ ] T010 [P] Integration test: create window then title change then close; script unload auto-closes in `tests/integration/test_window_lifecycle.cpp` (placeholder scaffold; needs real assertions once APIs exist).
- [ ] T011 [P] Integration test: logging from multiple scripts increments per-script counters in `tests/integration/test_logging_counters.cpp` (placeholder).
- [x] T012 [P] Integration test: hot reload debounce (two rapid reloads) in `tests/integration/test_reload_debounce.cpp` (DONE: real debounce logic validated; added SUPPRESSED status expectation + zero‑debounce variant test folded into this task).
- [ ] T013 [P] Integration test: reload replaces assembly and preserves other scripts unaffected in `tests/integration/test_reload_isolation.cpp` (placeholder).
- [ ] T014 [P] Integration test: unload returns CONTEXT_UNLOADING for in-flight subsequent calls in `tests/integration/test_unload_race.cpp` (placeholder).

Negative / Edge Tests
- [ ] T015 [P] Edge test: invalid window id operations return INVALID_ID in `tests/unit/test_window_invalid_id.cpp`.
- [ ] T016 [P] Edge test: UTF-8 bad input returns BAD_FORMAT in `tests/unit/test_utf8_validation.cpp`.
- [ ] T017 [P] Edge test: exceeding max contexts (65th load) in `tests/unit/test_max_contexts.cpp`.

## Phase 3.3: Core Implementation (AFTER tests exist & fail)
- [x] T018 Implement status code enum & helper to string in `src/bridge/gb2d_status_codes.cpp` (completed early to support T005).
- [x] T019 Implement runtime bootstrap & initialization guard in `src/bridge/gb2d_interop.cpp`.
- [x] T020 Implement script context manager (load/unload, duplication check, max contexts) `src/bridge/gb2d_script_manager.cpp`.
- [x] T021 Implement debounce reload logic (500 ms) in script manager. (DONE: includes logging suppressed reloads, clamping invalid values, new SUPPRESSED status code, synchronous unload+load cycle scaffold, zero‑debounce behavior covered by test.)
- [x] T022 Implement logging interop C API (`gb2d_logging_api.*`) mapping to LoggerManager.
- [ ] T023 Implement window interop C API (`gb2d_window_api.*`) mapping to WindowManager + main thread dispatch.
- [x] T024 Implement per-script log counters & retrieval hook.
- [x] T025 Implement auto-close windows on unload logic. (DONE: window API tracks script ownership; ScriptManager::unload invokes close-all helper; integration test T010 updated and now passes.)
- [x] T026 Implement UTF-8 validation utility `src/bridge/utf8_validation.cpp` (DONE: added utf8_validation.h/cpp and integrated into window & logging APIs; tests T016 pass using new validator).
- [x] T027 Wire configuration parsing into existing configuration manager (add new keys). (DONE: keys integrated with sensible defaults + env override path; script manager consuming maxContexts & reload.debounceMs.)

## Phase 3.4: Integration
- [ ] T028 Integrate script manager lifecycle into engine startup/shutdown sequences.
- [ ] T029 Add metrics logging (counts: loads, unloads, reloads, log messages) at shutdown.
- [ ] T030 Add enumeration API for scripts (exposed to managed) returning status snapshot.

## Phase 3.5: Polish
- [ ] T031 [P] Add documentation section in `quickstart.md` for status codes & reload behavior.
- [ ] T032 [P] Add performance micro-benchmark harness (optional) `tests/perf/test_interop_latency.cpp` (skipped if build type != Release).
- [ ] T033 [P] Add unit tests for UTF-8 validation edge cases (supplement) `tests/unit/test_utf8_additional.cpp`.
- [ ] T034 Audit thread dispatch correctness; add comment blocks documenting guarantees.
- [ ] T035 Review logging output formatting & align with engine style guide.
- [ ] T036 Update `README.md` (root or feature) with one-way interop design explanation.
- [ ] T037 Final pass: remove any leftover TODO markers in bridge code.

## Dependencies
- T003 before any implementation referencing status codes (T018+).
- T005–T017 (tests) must be written before T018–T027 implementations.
- T019 (bootstrap) before T020–T024 which rely on initialized state.
- T020 before T021 (reload logic) & T025 (auto-close) & T030 (enumeration).
- T022/T023 (interop APIs) before integration tests T010/T011 passing.
- T024 (log counters) before T011.
- T026 (UTF-8) before T016 & used by T023/T022.

## Parallel Examples
Initial contract tests in parallel:
T005 T006 T007 T008 T009

Integration tests in parallel (after contract tests authored):
T010 T011 T012 T013 T014

Core implementation parallel batch (after tests):
T018 T019 (sequential dependency: actually start with T018 then T019) then T022 T023 (parallel) while T020 starts after T019.

## Validation Checklist
- [ ] All contract files have a test (status codes, window, logging, duplication, init state)
- [ ] All entities (ScriptAssembly, ScriptContext, WindowHandle, LogMessage, StatusCode) have corresponding implementation tasks
- [ ] Tests authored before related implementation tasks
- [ ] Parallel tasks do not share the same file
- [ ] Status code usage consistent across APIs

## Notes
- BAD_FORMAT introduced (UTF-8 validation) ensure test coverage before implementation.
- No native→managed invocation tasks; any reference should be removed during code review.
- Performance harness optional; can defer if early p95 < target.

### Progress Notes (auto-updated)
- Deviations: T018 implemented before remaining contract tests converted to failing state (acceptable bootstrap compromise). Will revisit once window/logging APIs are added.
- Placeholders (T006–T014) currently succeed; they must be tightened to fail or assert real behavior after APIs are defined.
- Pending: Create `gb2d_window_api.h` & `gb2d_logging_api.h` (remaining part of T001) then convert placeholder tests.
