# Feature Specification: HotKeyManager Service & Customizable Shortcuts

**Feature Branch**: `010-hotkey-manager-service`

**Created**: 2025-10-05  
**Status**: Draft  
**Input**: "Introduce a centralized HotKeyManager that lets GameBuilder2d define, edit, persist, and dispatch user-customizable keyboard shortcuts (leveraging ImHotKey for UI authoring) so that editor commands like opening the File Dialog can be rebound per user."

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
8. Run self-check: no implementation-level prescriptions, requirements are testable
```

---

## User Scenarios & Testing *(mandatory)*

### Primary User Story
As a GameBuilder2d user, I can configure and save custom keyboard shortcuts through an in-app Hotkeys editor so that commands like opening the File Dialog respond to my preferred bindings across sessions.

### Acceptance Scenarios
1. **Default mapping availability** — Given a fresh configuration, when GameBuilder2d starts, then the HotKeyManager loads built-in defaults for all registered actions and exposes them to callers before any UI interaction occurs.
2. **Shortcut invocation** — Given an action is bound to a shortcut (e.g., `Ctrl+F` for File Dialog), when the user presses the combination during an active frame, then the manager reports the action as triggered exactly once and the corresponding command executes.
3. **UI editing & validation** — Given the user opens the Hotkeys settings panel, when they focus an action row and record a new key combination using ImHotKey, then the manager updates the in-memory binding, resolves modifier names consistently, and disallows or warns about direct conflicts.
4. **Persistence & reload** — Given the user applied changes, when the configuration is saved and GameBuilder2d restarts, then the custom shortcuts reload from `config.json` (or companion file) and produce the same behavior without reconfiguration.
5. **Conflict handling** — Given two actions are assigned the same shortcut, when the conflict is detected, then the UI surfaces an error state and the manager either prevents the save or marks the later binding inactive until resolved, according to configured policy.
6. **Platform adaptation** — Given the application runs on macOS, when shortcuts reference the Command key, then the manager maps the shortcut using platform-appropriate modifier labels and dispatches it correctly during runtime input polling.
7. **Fallback safety** — Given a stored binding references an unknown or deprecated action, when the manager loads configuration, then it logs a warning, ignores the orphan entry, and retains the default mapping for known actions.

### Edge Cases & Decisions
- Multiple shortcuts per action (e.g., primary + alternate) are optional for v1; scope decision recorded below.
- Empty shortcut ("Unassigned") disables the action until reconfigured; default mapping remains available for restoration.
- Modal contexts (e.g., text input fields) must suppress global shortcut dispatch to avoid interfering with typing.
- Mouse buttons and function keys are supported if ImHotKey and underlying input layer expose them consistently.
- Runtime changes apply immediately—no restart required—while persistence happens on explicit save (e.g., hitting "Apply" or closing the settings dialog when changes are valid).

### Clarification Outcomes *(2025-10-05)*
- **Multiple bindings**: v1 supports exactly one active shortcut per action. Future enhancement may add secondary bindings.
- **Conflict policy**: Saving is blocked while conflicts exist. The UI highlights conflicting actions and provides guidance to resolve them (e.g., clear or reassign). Auto-unbinding is out of scope. The settings panel presents the message *"Resolve shortcut conflicts before applying changes."* above the action list and shows inline error chips reading *"Shortcut in use by {otherAction}"* next to each conflicting entry; the Apply button stays disabled until all conflicts are cleared or the shortcuts are restored to defaults.
- **Storage format**: Hotkey mappings live under `("input" -> "hotkeys")` inside the primary `config.json`, serialized as arrays of `{"action":"File.OpenDialog","shortcut":"Ctrl+F"}` entries using canonical key names. The manager validates input on load and reverts invalid entries to defaults.
- **ImHotKey usage**: ImHotKey renders each editable shortcut cell; the manager owns conversion between ImHotKey's `ImHotKeyData` struct and the serialized format.
- **Tick integration**: HotKeyManager polls input every frame within the existing main loop, prior to UI dispatch, ensuring command handlers observe consistent state.

---

## Requirements *(mandatory)*

### Functional Requirements
- **FR-001**: The system MUST expose a HotKeyManager service registered alongside existing managers, providing lifecycle methods (`initialize`, `shutdown`, `tick`).
- **FR-002**: The manager MUST maintain a registry of known actions, each with a default shortcut, description, and optional grouping metadata for UI organization.
- **FR-003**: The manager MUST load user-defined shortcuts from configuration, validate them, and merge them with defaults at startup.
- **FR-004**: The manager MUST detect active shortcuts during runtime input polling and provide a query/dispatch API (`isTriggered`, event callbacks, or observer pattern) for other subsystems.
- **FR-005**: The manager MUST provide APIs to update shortcuts at runtime (set, clear, restore defaults) with immediate effect and validation feedback.
- **FR-006**: The manager MUST persist shortcut changes back to configuration when requested, preserving canonical ordering and comments where feasible.
- **FR-007**: The manager MUST expose conflict detection, returning actionable metadata so the UI can highlight and block invalid states.
- **FR-008**: The Hotkeys settings UI MUST utilize ImHotKey to edit individual bindings and surface status (default/custom, conflicts, unassigned).
- **FR-009**: The manager MUST respect context suppression flags (e.g., when text inputs are focused) to avoid triggering global actions inadvertently.
- **FR-010**: The manager MUST emit structured logs for load/save operations, conflicts, invalid shortcuts, and configuration migrations.
- **FR-011**: The manager MUST support platform-specific modifier aliases (e.g., Ctrl vs. Cmd) while keeping serialized data platform-neutral.

### Non-Functional Requirements
- **NFR-001**: Shortcut lookup and dispatch SHOULD be O(1) per frame via hash sets keyed on normalized key combinations.
- **NFR-002**: The service SHOULD avoid allocations during hotpath polling by precomputing normalized shortcut hashes/state.
- **NFR-003**: UI updates SHOULD debounce to prevent excessive config writes while the user is recording a shortcut.
- **NFR-004**: Serialization/deserialization SHOULD gracefully handle missing or extra fields to maintain backward compatibility.

### Out of Scope (initial release)
- Assigning macros or multi-step actions to a single shortcut.
- Recording chorded sequences (e.g., `Ctrl+K, Ctrl+F`).
- Per-project or per-workspace hotkey profiles; only per-user global settings are supported.
- Syncing shortcuts across devices via cloud services.

---

## Key Entities & Data
- **HotKeyManager Service**: Singleton-style interface for registration, polling, updates, persistence, and diagnostics.
- **HotKeyAction**: Data record containing action identifier, human-readable label, default shortcut, category, and optional tooltip/help text.
- **ShortcutBinding**: Normalized representation of a key combination (keycode + modifier mask), with serialization helpers to/from strings.
- **HotKeyConfig**: Aggregate structure loaded from `config.json` containing user overrides, version info, and migration notes.
- **ConflictReport**: Result structure enumerating conflicting action IDs, duplicated shortcuts, and suggested resolutions for the UI.

## Initial Shortcut Catalog *(v1 scope)*
| Action ID | Category | Default Shortcut | Context | Notes |
| --- | --- | --- | --- | --- |
| `global.openFileDialog` | Global | `Ctrl+O` | Available anywhere | Opens the File → "Open File…" dialog (user can rebind, e.g., to `Ctrl+F`). |
| `global.openImageDialog` | Global | `Ctrl+Shift+O` | Available anywhere | Launches the File → "Open Image…" picker with image filters applied. |
| `global.toggleEditorFullscreen` | Global | `F11` | Available anywhere | Mirrors GameBuilder → "Editor Fullscreen" menu toggle; no-ops while game fullscreen session is active. |
| `global.focusTextEditor` | Global | `Ctrl+Shift+E` | Available anywhere | Spawns or focuses the Text Editor window. |
| `global.showConsole` | Global | `Ctrl+Shift+C` | Available anywhere | Spawns or focuses the Console window. |
| `global.spawnDockWindow` | Global | `Ctrl+Shift+N` | Available anywhere | Matches Windows → "New Window" (generic dockable panel). |
| `global.openHotkeySettings` | Global | `Ctrl+Alt+K` | Available anywhere | Opens the Hotkeys settings panel introduced with this feature. |
| `layouts.saveLayout` | Global | `Ctrl+Alt+S` | Available anywhere | Triggers Layouts → "Save" using the current name entry if valid. |
| `layouts.openManager` | Global | `Ctrl+Alt+L` | Available anywhere | Focuses the Layouts management menu/popup for quick load/delete actions. |
| `codeEditor.newFile` | Code Editor | `Ctrl+N` | Active when Code Editor window focused | Invokes "New" tab creation. |
| `codeEditor.openFile` | Code Editor | `Ctrl+Shift+O` | Code Editor focused | Opens the editor-specific file chooser; shares implementation with toolbar button. Context scoping allows this to coexist with the global `Ctrl+Shift+O` binding. |
| `codeEditor.saveFile` | Code Editor | `Ctrl+S` | Code Editor focused | Saves the active tab if dirty. |
| `codeEditor.saveFileAs` | Code Editor | `Ctrl+Shift+S` | Code Editor focused | Forces save-as flow for the active tab. |
| `codeEditor.saveAll` | Code Editor | `Ctrl+Alt+S` | Code Editor focused | Persists all open tabs that are dirty; context scoping avoids conflicts with the global layout save binding. |
| `codeEditor.closeTab` | Code Editor | `Ctrl+W` | Code Editor focused | Closes the current tab (with dirty confirmation). |
| `codeEditor.closeAllTabs` | Code Editor | `Ctrl+Shift+W` | Code Editor focused | Closes all tabs after confirmations. |
| `gameWindow.toggleFullscreen` | Game Window | `Alt+Enter` | Game Window focused | Requests fullscreen session (no-op if already active or unsupported). |
| `gameWindow.resetGame` | Game Window | `Ctrl+R` | Game Window focused | Calls the embedded game's reset routine. |
| `gameWindow.cycleNextGame` | Game Window | `Ctrl+Tab` | Game Window focused | Advances to the next registered game in the dropdown. |
| `gameWindow.cyclePrevGame` | Game Window | `Ctrl+Shift+Tab` | Game Window focused | Moves to the previous registered game. |
| `fullscreen.exitSession` | Fullscreen Session | `Esc` (also `Ctrl+W`) | Only while fullscreen session active | Signals the session to exit back to the editor. |

---

## Risks & Mitigations


## Existing Input Handling Audit
| Action(s) impacted | Current trigger & location | Migration notes |
| --- | --- | --- |
| `global.openFileDialog`, `global.openImageDialog` | `WindowManager::renderUI` (`GameBuilder2d/src/services/window/WindowManager.cpp`, File menu around lines 630–660) open dialogs when the user clicks the menu entries. | HotKeyManager should invoke the same `launchFileDialog` lambda so keyboard shortcuts mirror menu clicks; ensure availability irrespective of dock focus. |
| `global.focusTextEditor`, `global.showConsole`, `global.spawnDockWindow` | `WindowManager::renderUI` (`WindowManager.cpp`, GameBuilder/Windows menus around lines 690–740) drive window spawning/focus via `MenuItem` clicks. | Provide HotKeyManager callbacks that request/focus the corresponding windows by reusing the `spawnWindowByType` helpers. |
| `global.toggleEditorFullscreen` | `WindowManager::renderUI` (`WindowManager.cpp`, GameBuilder menu around line 702) toggles fullscreen when the menu item is activated. | Hotkey dispatch should call `toggleEditorFullscreen()` with the same guard that prevents activation while a game fullscreen session is active. |
| `layouts.saveLayout`, `layouts.openManager` | `WindowManager::renderUI` (`WindowManager.cpp`, Layouts menu around lines 750–820) exposes buttons for saving/loading layouts. | Keyboard shortcuts should toggle the existing ImGui controls: trigger `saveLayout(name)` if the buffer is valid, and open/focus the layouts list modal. |
| `codeEditor.*` actions (`newFile`, `openFile`, `saveFile`, `saveFileAs`, `saveAll`, `closeTab`, `closeAllTabs`) | `CodeEditorWindow::render` (`GameBuilder2d/src/ui/Windows/CodeEditorWindow.cpp`, menu + toolbar around lines 80–140) respond exclusively to button/menu clicks today. | HotKeyManager dispatch can call the same member functions (`newUntitled`, `saveCurrent`, `saveAll`, `closeCurrent`, etc.) after verifying an editor tab is available; ensure focus gating so shortcuts only fire when the Code Editor window is active. |
| `gameWindow.toggleFullscreen`, `gameWindow.resetGame`, `gameWindow.cycleNextGame`, `gameWindow.cyclePrevGame` | `GameWindow::render` (`GameBuilder2d/src/ui/Windows/GameWindow.cpp`, toolbar around lines 80–140) currently expose ImGui buttons and combos. | HotKeyManager should synthesize the same behavior by invoking `fullscreen_requested_ = true`, `resetCurrentGame()`, and `switchGame()` variants; guard so shortcuts only fire when the Game Window has input focus. |
| `fullscreen.exitSession` | `FullscreenSession::tick` (`GameBuilder2d/src/ui/FullscreenSession.cpp`, lines 138–147) polls Raylib keys `Ctrl+W`/`Esc` directly each frame. | Replace raw key polling with HotKeyManager context queries while maintaining the `requestStop()` logic; keep a fallback for emergency exit if the manager is inactive. |


## Review & Acceptance Checklist
- [x] Intent captured
- [x] Scenarios authored
- [x] Requirements drafted
- [x] Entities enumerated
- [x] Risks documented
- [ ] Clarifications outstanding (none currently)
- [ ] Ready for planning (plan TBD)

---

## Execution Status
- [x] Intent captured
- [x] Scenarios authored
- [x] Requirements drafted
- [x] Entities enumerated
- [x] Risks documented
- [ ] Clarifications resolved
- [ ] Ready for implementation plan
