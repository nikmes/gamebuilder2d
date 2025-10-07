# Feature Specification: Configuration Management Window

**Feature Branch**: `011-configuration-manager-window`

**Created**: 2025-10-07  
**Status**: Draft  
**Input**: "Ship an in-app Configuration window that visualizes `config.json`, organizes settings by section, and lets users edit, validate, and persist changes without touching raw JSON."

---

## Execution Flow *(authoring guardrail)*
```
1. Capture the user intent and articulate desired outcomes
2. Identify actors, actions, data, constraints, and success criteria
3. Surface ambiguities as [NEEDS CLARIFICATION: question]
4. Describe user scenarios and acceptance tests focused on observable behavior
5. List functional requirements; add non-functional where relevant
6. Model key entities / data that must exist to support the feature
7. Document edge cases, risks, and assumptions
8. Run self-check: requirements are testable and avoid premature implementation bias
```

---

## User Scenarios & Testing *(mandatory)*

### Primary User Story
As a GameBuilder2d user, I can open a Configuration window that presents settings grouped by section, edit values with appropriate controls (toggles, sliders, text boxes, file pickers, etc.), and apply or save changes with clear validation feedback.

### Acceptance Scenarios
1. **Section navigation & grouping** — Given `config.json` contains `window`, `input`, `audio`, and `hotkeys` sections, when the user opens the Configuration window, then the UI displays a navigation tree (or accordion) with those sections and selecting one reveals the relevant fields grouped with headings and descriptions.
2. **Field editing & validation** — Given a numeric field with a defined range, when the user enters an out-of-range value, then the field is marked invalid with inline messaging and the Apply/Save buttons remain disabled until the issue is resolved or reverted.
3. **Dirty-state handling** — Given the user modifies one or more fields, when they attempt to close the window without applying or saving, then the UI prompts them to discard, apply, or cancel, ensuring no accidental loss of changes.
4. **Runtime application** — Given changes were applied, when the configuration impacts running systems (e.g., toggling VSync or updating search paths), then the ConfigurationManager broadcasts updates and the dependent subsystems react without requiring a restart.
5. **Persistence flow** — Given the user presses Save, when the underlying file is writable, then the updated settings are serialized in canonical order (preserving comments where possible) and reloaded on the next startup; if the write fails, the UI surfaces an error toast/modal with retry guidance.
6. **Schema evolution** — Given new fields are introduced in a future release, when the window loads the schema from ConfigurationManager, then the new fields appear automatically with sensible defaults and descriptive labels without manual UI changes.
7. **Search & filtering** — Given the user enters a keyword in the search box, when matching fields exist across sections, then the navigation narrows to show those matches and highlights them in the editor panel.

### Edge Cases & Decisions
- Nested structures (e.g., `input.hotkeys`) should provide specialized editors or fallback to JSON text blocks with copy/paste support.
- Unknown keys in `config.json` must still be visible in a "Misc" section to avoid silently dropping user customization.
- Large text values (e.g., shader code) may require modal editors to avoid cluttering the panel.
- Unsaved changes should persist while the window remains open, even if configuration reloads occur externally; conflicts prompt a diff/merge dialog.
- Apply vs Save separation: Apply updates runtime state without writing to disk; Save writes to disk (and implies Apply). Users can Apply multiple times before Saving.

### Clarification Outcomes *(2025-10-07)*
- **Schema source**: UI reads structured metadata from `ConfigurationManager::schema()` (to be introduced) so the window remains declarative.
- **Advanced settings**: A toggle reveals fields flagged as `advanced` or `experimental`. Default view focuses on core settings to reduce overwhelm.
- **Diff preview**: Before saving, users can open a side-by-side diff of current vs pending JSON. Out-of-scope for v1, but log an enhancement item.
- **Backup policy**: On first save in a session, the system writes `config.backup.json` to allow manual recovery. Optional for v1; treat as stretch.

#### Schema ownership & metadata format *(T101)*
- `ConfigurationManager` owns the authoritative schema definition. The schema is constructed during service initialization using constexpr-friendly builders and remains immutable at runtime.
- `ConfigurationSchema` exposes a tree of `ConfigSectionDesc` objects, each with:
	- `id` (string key matching the JSON section),
	- `label` (localized display string),
	- `description`,
	- `fields` (vector of `ConfigFieldDesc`),
	- `children` (optional vector for nested sections), and
	- `flags` (bitmask: `Advanced`, `Experimental`, `Hidden`).
- `ConfigFieldDesc` captures field metadata:
	- `id` (JSON key or dotted path),
	- `type` (enum: `Boolean`, `Integer`, `Float`, `Enum`, `String`, `Path`, `List`, `JsonBlob`, `Hotkeys`, etc.),
	- `label` / `description`,
	- `defaultValue` (variant),
	- `validation` (struct with optional `min`, `max`, `regex`, `enumValues`, `pathMode`, `step`, `precision`),
	- `advanced` / `experimental` flags,
	- `uiHints` (map for control-specific hints such as `placeholder`, `multiline`, `fileFilters`).
- The schema API returns a const reference (`const ConfigurationSchema& schema() noexcept`) and field-level helper queries (e.g., `findSection`, `findField`). UI code consumes only the descriptors and delegates validation to the manager.
- Runtime values flow through `ConfigurationManager::valueFor(fieldId)` and `ConfigurationManager::setValue(fieldId, value, ValidationMode)` ensuring validation is centralized. The UI never mutates JSON directly.
- Schema definitions live alongside the default configuration to guarantee parity. Any new config field requires updating both the defaults and the schema descriptor within the same module.

#### Apply vs Save UX & backup expectations *(T102)*
- **Toolbar actions**: Present two primary buttons — **Apply** (tooltip *"Update the running editor without saving to disk"*) and **Save** (tooltip *"Apply changes and write to config.json"*). Both remain disabled while validation errors exist.
- **Dirty prompts**:
	- Closing with unapplied edits triggers *"Discard Configuration Changes?"* modal with actions `Apply`, `Discard`, `Cancel` and body *"You have unapplied configuration changes. Apply them now, discard them, or cancel to keep editing."*
	- Closing with applied-but-unsaved edits triggers *"Save Configuration Changes?"* modal with actions `Save`, `Discard`, `Cancel` and body *"You applied changes that haven't been saved to disk. Save them now, discard them, or cancel to keep editing."*
- **Status messaging**:
	- Apply success toast: *"Configuration applied."*
	- Save success toast: *"Configuration saved to config.json."*
	- Failure toast template: *"Configuration save failed: {reason}. Your changes are still staged."*
- **Backup behavior**: First successful Save per session emits a `config.backup.json` snapshot beside the primary file before writing. Subsequent saves update both files atomically. Toast: *"Backup created: config.backup.json"* on initial backup.
- **Autosave policy**: No automatic saves. Apply updates runtime state but retains dirty markers until Save completes, encouraging explicit persistence.

---

## Requirements *(mandatory)*

### Functional Requirements
- **FR-001**: The application MUST expose a Configuration window accessible via menu/shortcut (`Window → Configuration`).
- **FR-002**: The window MUST present settings grouped by top-level sections defined in the configuration schema, preserving order and hierarchy.
- **FR-003**: Each field MUST render an editor appropriate to its type (boolean toggle, numeric slider/spin box with ranges, enum dropdown, text input, file path picker, custom widget for hotkeys/audio devices/etc.).
- **FR-004**: The UI MUST maintain dirty-state tracking per field, per section, and globally, surfacing which values diverge from persisted configuration or defaults.
- **FR-005**: The system MUST perform validation on edit and block Apply/Save if any field is invalid, while providing actionable inline error messages.
- **FR-006**: The system MUST support Apply (runtime update) and Save (persist to disk) actions independently, including revert options at field, section, and global levels.
- **FR-007**: The window MUST integrate with `ConfigurationManager` so runtime changes broadcast through existing observer hooks (or new events) and affected services can react immediately.
- **FR-008**: The UI MUST offer search/filtering across fields, highlighting matches and allowing quick navigation.
- **FR-009**: The window MUST surface logging/output for apply/save operations (success, warnings, errors) in a status bar or toast system.
- **FR-010**: Unknown or future keys present in `config.json` MUST remain editable via a generic JSON editor section to preserve user customizations.

### Non-Functional Requirements
- **NFR-001**: Rendering the window SHOULD not introduce frame hitches; schema traversal and validation must execute within a few milliseconds for typical configs.
- **NFR-002**: Edits SHOULD be reversible during the session via undo/redo (at least one level) to prevent frustration during experimentation.
- **NFR-003**: The window SHOULD be fully keyboard navigable and support accessibility (focus order, descriptive labels).
- **NFR-004**: The implementation SHOULD avoid direct coupling between UI controls and raw JSON, relying on typed intermediates for safety.

### Out of Scope (initial release)
- Automated diff/merge for concurrent edits coming from external editors.
- Multi-profile management (per-project or per-user profiles beyond the primary `config.json`).
- Remote sync or cloud backup of configuration files.
- Graphical visualization of complex nested data beyond tabular editors (e.g., curves or node graphs).

---

## Key Entities & Data
- **ConfigurationSchema**: Metadata describing sections, groups, field definitions (id, type, label, description, default, validation rules, flags).
- **ConfigFieldState**: Runtime representation of a field within the editor (current value, original value, validation status, dirty flag, error text).
- **ConfigSectionState**: Aggregated state for a section (list of fields, dirty/invalid counts, advanced flag).
- **ApplyResult / SaveResult**: Structures returned by ConfigurationManager operations summarizing applied changes, warnings, and error conditions for UI display.
- **ConfigDiff**: Optional diff artifact capturing original vs pending JSON for preview dialogs and telemetry.

---

## Risks & Mitigations
| Risk | Impact | Mitigation |
| --- | --- | --- |
| Schema drift between ConfigurationManager and UI | Broken or missing fields | Define schema in a single source of truth (ConfigurationManager) and add automated tests ensuring UI wiring matches. |
| Large or nested configs causing clutter | Poor UX, user confusion | Implement grouping, collapsible sections, and search. Provide generic JSON editors for complex arrays/maps until specialized widgets exist. |
| Validation logic duplication | Inconsistent rules | Centralize validation in ConfigurationManager and expose it via schema metadata/validators consumed by the UI. |
| Failed writes corrupting config | User data loss | Write to temp files and replace atomically; optionally create backups before overwriting. |
| Platform-specific paths or file pickers | Inconsistent behavior | Abstract file selection through existing path utilities and ensure fallbacks for headless modes. |

---

## Review & Acceptance Checklist
- [x] Intent captured
- [x] Scenarios authored
- [x] Requirements drafted
- [x] Entities enumerated
- [x] Risks documented
- [ ] Clarifications resolved
- [ ] Ready for planning

---

## Execution Status
- [x] Intent captured
- [x] Scenarios authored
- [x] Requirements drafted
- [x] Entities enumerated
- [x] Risks documented
- [ ] Clarifications resolved
- [ ] Ready for implementation plan
