# Task Breakdown — Configuration Management Window

## Clarifications & Preparations
- [ ] T101 — Approve schema ownership model and metadata format
- [ ] T102 — Finalize Apply vs Save UX copy, prompts, and backup expectations
- [ ] T103 — Inventory existing config sections/fields and identify missing documentation

## Schema & Services
- [ ] T104 — Introduce `ConfigurationSchema` structures and builder utilities in `ConfigurationManager`
- [ ] T105 — Populate schema metadata for current sections (`window`, `input`, `audio`, `debug`, `hotkeys`)
- [ ] T106 — Expose query/validation APIs and change notification hooks for UI consumers

## Editor State Management
- [ ] T107 — Implement editor-side field/section state models with dirty tracking and revert helpers
- [ ] T108 — Wire validation pipeline (on-edit, on-apply) using schema-provided rules
- [ ] T109 — Add lightweight undo/redo (at least single-level revert per field/section)

## UI & UX
- [ ] T110 — Scaffold `ConfigurationWindow` UI, register in WindowManager and menus
- [ ] T111 — Implement navigation pane with section grouping, dirty/invalid badges, and search filtering
- [ ] T112 — Render detail panel controls per field type with inline validation and tooltips
- [ ] T113 — Provide advanced/experimental toggle and generic JSON fallback editor for unknown keys
- [ ] T114 — Implement Apply, Save, Revert controls with confirmation dialogs for unsaved changes

## Integration & Persistence
- [ ] T115 — Connect Apply flow to `ConfigurationManager` runtime updates and hot reload hooks
- [ ] T116 — Implement Save flow with atomic file writes and optional backup file emission
- [ ] T117 — Surface status toasts/log entries for apply/save success, warnings, and failures
- [ ] T118 — Register HotKeyManager shortcut (e.g., `Ctrl+,`) and ensure suppression compliance in text fields

## Quality Assurance
- [ ] T119 — Unit tests for schema validation, apply/save pathways, and revert helpers
- [ ] T120 — Integration test editing fields via UI helpers and verifying persistence across restart
- [ ] T121 — Manual QA checklist (multi-section configs, invalid inputs, search/filter, advanced toggle)

## Documentation & Release
- [ ] T122 — Update user documentation (README, docs/configuration-manager.md) with Configuration window guide
- [ ] T123 — Draft developer notes for extending the schema and adding new field editors
- [ ] T124 — Add changelog entry referencing spec `011` and note backup/diff roadmap
- [ ] T125 — Capture follow-up enhancements (diff preview, profile management, localization of field labels)
