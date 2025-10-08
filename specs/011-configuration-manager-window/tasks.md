# Task Breakdown — Configuration Management Window

## Clarifications & Preparations
- [x] T101 — Approve schema ownership model and metadata format
- [x] T102 — Finalize Apply vs Save UX copy, prompts, and backup expectations
- [x] T103 — Inventory existing config sections/fields and identify missing documentation

## Schema & Services
- [x] T104 — Introduce `ConfigurationSchema` structures and builder utilities in `ConfigurationManager`
- [x] T105 — Populate schema metadata for current sections (`window`, `input`, `audio`, `debug`, `hotkeys`)
- [x] T106 — Expose query/validation APIs and change notification hooks for UI consumers

## Editor State Management
- [x] T107 — Implement editor-side field/section state models with dirty tracking and revert helpers
- [x] T108 — Wire validation pipeline (on-edit, on-apply) using schema-provided rules
- [x] T109 — Add lightweight undo/redo (at least single-level revert per field/section)

## UI & UX
- [x] T110 — Scaffold `ConfigurationWindow` UI, register in WindowManager and menus
- [x] T111 — Implement navigation pane with section grouping, dirty/invalid badges, and search filtering
- [x] T112 — Render detail panel controls per field type with inline validation and tooltips
- [x] T113 — Provide advanced/experimental toggle and generic JSON fallback editor for unknown keys
- [x] T114 — Implement Apply, Save, Revert controls with confirmation dialogs for unsaved changes

## Integration & Persistence
- [x] T115 — Connect Apply flow to `ConfigurationManager` runtime updates and hot reload hooks
- [x] T116 — Implement Save flow with atomic file writes and optional backup file emission
- [x] T117 — Surface status toasts/log entries for apply/save success, warnings, and failures
- [x] T118 — Register HotKeyManager shortcut (e.g., `Ctrl+,`) and ensure suppression compliance in text fields

## Quality Assurance
- [x] T119 — Unit tests for schema validation, apply/save pathways, and revert helpers
- [x] T120 — Integration test editing fields via UI helpers and verifying persistence across restart
- [ ] T121 — Manual QA checklist (multi-section configs, invalid inputs, search/filter, advanced toggle)

## Documentation & Release
- [x] T122 — Update user documentation (README, docs/configuration-manager.md) with Configuration window guide
- [x] T123 — Draft developer notes for extending the schema and adding new field editors
- [ ] T124 — Add changelog entry referencing spec `011` and note backup/diff roadmap
- [ ] T125 — Capture follow-up enhancements (diff preview, profile management, localization of field labels)
