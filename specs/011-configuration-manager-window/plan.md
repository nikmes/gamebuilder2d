# Implementation Plan — Configuration Management Window

## Phase 0 · Alignment & Clarifications *(Not Started)*
- [ ] Confirm schema ownership (single source in `ConfigurationManager` vs. external metadata file)
- [ ] Decide on minimum viable set of field controls (file pickers, enum dropdowns, path lists)
- [ ] Approve Apply vs Save workflow copy, prompts, and backup expectations

## Phase 1 · Schema Foundation & API Surface
- Outline:
	- Extend `ConfigurationManager` with schema metadata (`ConfigurationSchema`, field descriptors, validation callbacks).
	- Provide runtime accessors for current values, defaults, and validation results per field/section.
	- Introduce change notification hooks (`onConfigApplied`, `onConfigSaved`).
- [ ] Define schema structures and builder utilities
- [ ] Populate schema for existing sections (`window`, `input`, `audio`, `debug`, etc.)
- [ ] Expose schema/query APIs to UI layer via service locator

## Phase 2 · Editor State Management
- Contract outline:
	- Maintain `ConfigFieldState` & `ConfigSectionState` mirroring schema definitions.
	- Track dirty/invalid state, original vs staged values, revert helpers (field, section, global).
	- Provide diff artifacts for optional preview/export.
- [ ] Implement editor-side state models decoupled from ImGui widgets
- [ ] Wire validation pipeline (invoked on edit, on apply)
- [ ] Implement undo/redo buffer (minimum single-level revert)

## Phase 3 · UI Composition (ImGui)
- Outline:
	- Build layout with left navigation (tree/accordion) + right detail panel (cards or tabs).
	- Render field controls per type (bool, number, enum, string, path, list editor, JSON fallback).
	- Provide global toolbar with search, Apply, Save, Revert, Advanced toggle.
- [ ] Scaffold `ConfigurationWindow` files and register with WindowManager menu/shortcut
- [ ] Implement navigation pane w/ dirty badges and search filtering
- [ ] Implement detail panel rendering w/ inline validation messaging
- [ ] Add advanced/experimental toggle and generic fallback editor for unknown keys

## Phase 4 · Persistence & Runtime Integration
- [ ] Implement Apply behavior invoking ConfigurationManager updates and broadcasting results
- [ ] Implement Save behavior performing atomic write+backup, updating runtime state on success
- [ ] Surface status toasts/logs for apply/save success and failures
- [ ] Add confirmation dialogs for closing with unsaved changes or resetting sections

## Phase 5 · Hotkey & Suppression Integration
- [ ] Add shortcut (e.g., `Ctrl+,`) to open the window via HotKeyManager
- [ ] Ensure window participates in hotkey suppression while editing text inputs
- [ ] Update menus/tooltips with new shortcut labels

## Phase 6 · Testing & Validation
- [ ] Unit tests for schema validation, apply/save flows, revert helpers
- [ ] Integration test covering UI-driven edit → apply → save → reload
- [ ] Manual QA checklist (multi-section configs, invalid inputs, platform-specific paths, advanced toggle)

## Phase 7 · Documentation & Release Notes
- [ ] Update README and docs with Configuration window overview and usage guide
- [ ] Document schema extension process for developers
- [ ] Add changelog entry referencing spec `011` and note backup behavior
- [ ] Capture follow-up backlog (diff preview, multi-profile support, schema-driven localization)
