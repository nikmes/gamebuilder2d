# Task Breakdown — HotKeyManager Service

## Clarifications & Preparations
- [x] T001 — Approve conflict resolution policy (blocking saves; UI messaging copy)
- [x] T002 — Confirm list of initial actions requiring shortcuts (File menu, window toggles, play/stop, etc.)
- [x] T003 — Inventory existing input-handling code paths that must route through the new manager

## Engineering
- [x] T004 — Vendor ImHotKey dependency under `GameBuilder2d/thirdparty` with license notice
- [x] T005 — Scaffold `HotKeyManager` interface and implementation files in the new static library
- [x] T006 — Define `HotKeyAction` catalog and registration helpers
- [x] T007 — Implement shortcut normalization, hashing, and equality utilities
- [x] T008 — Implement runtime polling and trigger detection aligned with current input frame loop
- [x] T009 — Add APIs for runtime updates (set/clear/restore) including validation feedback structures
- [x] T010 — Integrate context suppression hooks for text input or modal dialogs

## Configuration & Persistence
- [x] T011 — Extend `config.json` schema (and defaults) to include `input.hotkeys`
- [x] T012 — Implement load/merge with defaults and conflict detection
- [x] T013 — Implement save/export path with stable ordering and comment preservation
- [x] T014 — Add migration/backfill handling for missing or invalid entries

## UI & UX
- [x] T015 — Build Hotkeys settings panel using ImHotKey widget, grouped by action category
- [x] T016 — Display conflict/invalid states inline with actions and disable apply/save until resolved
- [x] T017 — Provide controls for restoring defaults, clearing bindings, and applying/canceling changes
- [x] T018 — Update existing menus/toolbars to reference HotKeyManager for shortcut labels/tooltips

## Integration
- [x] T019 — Replace hard-coded shortcut checks throughout the editor with HotKeyManager queries/events
- [x] T020 — Ensure global shortcuts respect modal suppression (e.g., text editor focus)
- [x] T021 — Expose command registration utilities so new features can declare shortcuts easily

## Quality Assurance
- [ ] T022 — Unit tests for normalization, conflict detection, load/save round-trips
- [ ] T023 — Integration test verifying edited shortcuts persist and trigger expected commands
- [ ] T024 — Manual QA checklist covering Windows/Linux/macOS modifier behavior, persistence, and conflict UX

## Documentation & Release
- [x] T025 — Update user-facing documentation (docs/settings, README, changelog)
- [x] T026 — Produce developer guide for adding new actions and using HotKeyManager APIs
- [ ] T027 — Note follow-up enhancements (secondary bindings, profile export/import, macro support)
