# Spec: Modular Window System and WindowManager Slimming

## Summary
Refactor `WindowManager` into an orchestrator with a plugin-like window system. Each window type implements `IWindow`, is created via `WindowRegistry`, uses `WindowContext` for services, and persists its own state to JSON. The manager owns layout/dockspace and generic chrome.

## Non-Goals
- Rewriting ImGui docking or removing ImGui ini persistence
- Changing logging backends or TextEditor library

## Requirements
1. Introduce `IWindow`, `WindowContext`, and `WindowRegistry`.
2. Convert Console, Code Editor, and File Preview into separate windows.
3. Save and load layout metadata in JSON per window type while retaining ImGui ini for positions.
4. Populate the Windows menu from registry (no hard-coded entries).
5. Maintain per-window min-size constraints during splits.
6. Support reopen/focus/undock/close actions per window.
7. Handle unknown window types gracefully on load.

## Functional Details

### IWindow Interface
- `const char* typeId() const` – stable identifier
- `const char* displayName() const` – for menus
- `std::string title() const` / `void setTitle(std::string)`
- `std::optional<Size> minSize() const`
- `void render(WindowContext&)`
- `void onFocus(WindowContext&)` / `void onClose(WindowContext&)`
- `void serialize(json&) const` / `void deserialize(const json&)`

### WindowContext
- Logging: adapter to LogManager
- FileDialog: wrapper API around ImGuiFileDialog
- RecentFiles: list + add method
- Notifications: `addToast(text, seconds)`
- Manager callbacks scoped to the current window id: `requestFocus()`, `requestUndock()`, `requestClose()`
- Config accessors

### WindowRegistry
- Register types with factory lambdas
- Lookup by `typeId`
- Enumerate for menus

### Persistence
- JSON file `out/layouts/<name>.layout.json`
- ImGui ini remains `out/layouts/<name>.imgui.ini`
- Manager saves: version, next_id, last_folder, recent_files, windows[]
- Window saves: per-window `state` JSON via `serialize`
- Loading: skip unknown types, report via toast

### Menu Integration
- "Windows" menu lists all registered window types; creating a window spawns a new instance and docks it (center by default)
- Optional: per-window internal menu bar stays inside `render`

## Data Model
See `data-model.md` for JSON schema and struct outlines.

## Contracts
See `contracts/` for `IWindow`, `WindowContext`, and `WindowRegistry` signatures and invariants.

## Risks & Mitigations
- Risk: Drift between manager and window serialization – Mitigate with unit tests per window type for round-trip.
- Risk: Texture lifetime in preview windows – Encapsulate in window; add `onClose` to release.
- Risk: Performance regressions – Preserve incremental append in Console; instrument with optional metrics macro.

## Acceptance Criteria
- Adding a new window type does not require modifications to `WindowManager` beyond registration.
- Existing features (Console, Editor, Preview) work as before.
- Layout save/load restores window set and their internal states.
- Windows menu is driven by registry.
