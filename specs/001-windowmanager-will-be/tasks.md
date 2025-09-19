# Tasks: WindowManager (Dockable Windows)

**Input**: Design documents from `/specs/001-windowmanager-will-be/`
**Prerequisites**: plan.md (required), research.md, data-model.md, contracts/

## Execution Flow (main)
```
1. Load plan.md from feature directory
   → Extract: tech stack (C++20), libraries (raylib, rlImGui), structure (single project)
2. Load design documents:
   → data-model.md: Window, DockRegion, Layout
   → contracts/: window-manager-contract.md → action behaviors
   → research.md: decisions for docking, sizing, persistence
3. Generate tasks by category with TDD ordering
4. Apply parallelization rules and specify exact file paths
5. Validate coverage and dependencies
```

## Format: `[ID] [P?] Description`

## Phase 3.1: Setup
- [x] T001 Ensure build targets exist or add stubs for WindowManager in `GameBuilder2d/src` and `RunTime2d/src` as needed
- [ ] T002 [P] Add test scaffolding directories `tests/contract/`, `tests/integration/`, `tests/unit/` under repo root (docs-only placeholders if code tests not used)
- [ ] T003 [P] Add developer docs link in `README.md` to quickstart at `specs/001-windowmanager-will-be/quickstart.md`

## Phase 3.2: Tests First (TDD)
- [ ] T004 [P] Contract test: createWindow/dock/undock/close behavior in `tests/contract/window_manager_contract.test.md` (executable checklist)
- [ ] T005 [P] Contract test: tab ordering and overflow behavior in `tests/contract/window_tabs_contract.test.md`
- [ ] T006 [P] Integration test scenario from quickstart in `tests/integration/window_manager_quickstart.test.md`

## Phase 3.3: Core Implementation
- [x] T007 [P] Define `Window` struct/class header in `GameBuilder2d/src/window/Window.h`
- [x] T008 [P] Define `DockRegion` struct/class header in `GameBuilder2d/src/window/DockRegion.h`
- [x] T009 [P] Define `Layout` struct/class header in `GameBuilder2d/src/window/Layout.h`
- [x] T010 Implement `WindowManager` interface header in `GameBuilder2d/src/window/WindowManager.h` covering actions from contract
- [x] T011 Implement minimal `.cpp` stubs for above types with no-op logic to satisfy linker (failing tests expected)

## Phase 3.4: Integration
- [x] T013 Implement createWindow → floating state and registration in layout
 - [x] T014 Implement docking with regions (left/right/top/bottom/center-as-tab)
- [x] T015 Implement undock → floating with state retention
- [ ] T016 Implement tab grouping and active tab switching
 - [ ] T017 Implement region resizing with minimum sizes (200×120 logical units; window minima override)
- [x] T018 Implement closeWindow removing layout artifacts
- [x] T019 Implement persistence: save/load last-used layout; setting to disable
- [x] T020 Implement named layout save/load (10+ entries)
- [ ] T021 Implement keyboard-only navigation and actions (focus cycle, dock/undock, close)
- [ ] T022 Multi-monitor/DPI adjustments on restore

## Phase 3.5: Polish
- [ ] T023 [P] Unit tests (where feasible) for pure layout operations (serialize/deserialize, constraints)
- [ ] T024 [P] Performance validation with 50 windows (manual or scripted)
- [ ] T025 [P] Update user documentation and help overlay reference for keyboard mappings
- [ ] T026 Cleanup, error handling, guard against invalid operations

## Dependencies
- Tests (T004–T006) before Core (T007–T011) and Integration (T013+)
- T007–T011 enable T013–T018
- T019 depends on data model; T020 depends on T019
- Keyboard support (T021) depends on core actions (T013–T018)

## Parallel Example
```
Run in parallel:
- T004, T005, T006 (contract + integration tests)
- T007, T008, T009 (header definitions)
```

## Validation Checklist
- [ ] All contracts have corresponding tests (T004–T005)
- [ ] All entities have model tasks (T007–T009)
- [ ] All tests come before implementation
- [ ] Parallel tasks operate on different files
- [ ] Each task specifies exact file paths
- [ ] No [P] tasks modify the same file concurrently

## Progress Notes
- Default docked layout implemented via ImGui DockBuilder (Scene/Inspector/Console).
- `renderUI()` integrates full-screen dockspace and window creation menu.
- Persistence: auto-load `last` on startup; auto-save on shutdown; named layouts supported.
- `RunTime2d` left unchanged per constraint; main loop lives in `GameBuilder2d`.
 - Docking:
    - Explicit region docking implemented in `dockWindow` (Left/Right/Top/Bottom/Center-as-tab).
    - Visual dock targets overlay with window drag handle for drag-and-drop docking.
 - Constraints:
    - Min-size guards applied to programmatic splits to prevent panes below 200×120 (per-window override supported).
    - Remaining for T017: enforce min sizes during user-initiated region resizing and expose configurable defaults.
