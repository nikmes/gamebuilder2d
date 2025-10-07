# Implementation Plan — HotKeyManager Service

## Phase 0 · Alignment & Clarifications *(In Progress)*
- [ ] Confirm final conflict-resolution UX (blocking save vs. auto-unassign)
- [ ] Decide how modal suppression flags are exposed to consumers (e.g., scoped RAII vs. manual calls)
- [ ] Finalize shortcut serialization format and migration strategy from existing configs (if any)

## Phase 1 · Service Scaffolding
- Outline:
	- Define `HotKeyAction` catalog (IDs, labels, defaults) and load order expectations.
	- Introduce `HotKeyManager` interface/header with lifecycle (`initialize`, `shutdown`, `tick`) and query/update APIs.
	- Register manager alongside existing services within application bootstrap.
- [ ] Create data definitions and placeholders for default bindings
- [ ] Register manager in service locator/bootstrap sequence
- [ ] Ensure build system (CMake presets) compiles the new static library

## Phase 2 · Input Normalization & Dispatch Core
- Contract outline:
	- `registerAction(const HotKeyAction&)` populates catalog prior to initialization.
	- `isTriggered(actionId)` returns true exactly once per frame when the shortcut combination transitions from inactive to active.
	- `setShortcut(actionId, ShortcutBinding)` validates and applies overrides at runtime.
	- `restoreDefaults()` reverts to default mappings for all actions.
- [x] Implement shortcut normalization utilities (key enums, modifier masks, hashing)
- [x] Implement per-frame polling logic that updates trigger state and de-bounces repeated presses
- [ ] Provide context suppression API for UI/text input scenarios

## Phase 3 · Configuration Integration & Persistence
- [ ] Extend ConfigManager schema (`input.hotkeys`) with defaults and comments
- [ ] Implement load/merge flow converting serialized entries to `ShortcutBinding`
- [ ] Implement save flow that emits canonical ordering and ignores orphaned/malformed entries
- [ ] Add migration hook for future schema changes (version tagging, fallback)

## Phase 4 · UI Integration (ImHotKey)
- [ ] Vendor ImHotKey under `thirdparty` and expose it through build scripts
- [ ] Build Hotkeys settings panel leveraging ImHotKey cells per action
- [ ] Surface conflict states (duplicate bindings, invalid combos) with clear messaging
- [ ] Provide “Restore Defaults” and “Apply/Cancel” actions with proper dirty-state tracking

## Phase 5 · Diagnostics & Telemetry
- [ ] Add logging hooks for load/save, conflicts, invalid user edits
- [ ] Expose diagnostics snapshot (current bindings, conflicts, frames since trigger) for overlays or dev console
- [ ] Provide optional metrics (number of custom overrides, suppressed triggers count)

## Phase 6 · Testing & Validation
- [ ] Unit tests for normalization, conflict detection, load/save round-trips, trigger de-bounce, context suppression
- [ ] Integration test covering editing a shortcut via ImHotKey wrapper and verifying dispatch on the new binding
- [ ] Manual QA checklist (platform modifiers, persistence across runs, conflict UX)

## Phase 7 · Documentation & Handoff
- [ ] Update README / docs with HotKeyManager overview and configuration instructions
- [ ] Draft developer guide for registering new actions and using context suppression
- [ ] Capture backlog items (secondary bindings, profiles, chorded shortcuts)
- [ ] Update changelog entry referencing spec `010`
