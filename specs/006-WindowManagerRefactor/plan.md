# WindowManager Refactor Plan

Goal: Stop `WindowManager.{h,cpp}` from growing every time a new window type is added. Make each window self-contained and pluggable while keeping docking, layout, and app-level chrome in a slim manager.

## Objectives

- New window types require adding only a new class (and a single registry entry).
- No new per-window members or switch/if branches in `WindowManager`.
- Window-specific state is serialized by the window itself (JSON). Manager persists only generic metadata and ImGui INI.
- Maintain/improve current UX: docking, min-size constraints, menus, recent files, console search/filters, editor tabs.

## Architecture Overview

- IWindow (interface): every window implements this minimal contract.
- WindowContext: a small bag of services/callbacks passed into `IWindow::render`.
- WindowRegistry: registration and factory for window types.
- WindowManager: an orchestrator; owns list of windows, renders the dockspace, menus, and delegates to window instances.

### IWindow (contract)

- Identity and UX
  - `typeId()` stable machine id for persistence and menus (e.g. "console-log").
  - `displayName()` human-friendly name for menus (e.g. "Console Log").
  - `title()` current title shown in tab; `setTitle()` to rename.
  - `minSize()` optional minimum size constraint for docking splits.
- Runtime
  - `render(WindowContext&)` draws the ImGui content and internal menus.
  - `onFocus/onClose` hooks (optional).
- Persistence
  - `serialize(json&) const` and `deserialize(const json&)` for window-specific state.

See contracts for signatures.

### WindowContext (services)

A slim bundle passed to `IWindow::render` with:
- Logging facade
- File dialog access (wrapper over ImGuiFileDialog)
- Recent files service
- Toast notifications
- Focus/undock/close helpers for the calling window
- Access to configuration

Windows should not reach back into `WindowManager` directly.

### WindowRegistry (plugin-style)

- Register types with `{ typeId, displayName, factory }`.
- Create instances by `typeId` (used in menus and layout restore).
- Enumerate registered types to build the "Windows" menu.

### WindowManager (slim)

Responsibilities:
- Dockspace/overlay and default layout builder
- Menus: File/GameBuilder/Windows/Layouts (entries for windows sourced from registry)
- Persistence orchestration:
  - Save ImGui INI as before.
  - Save JSON with window list + per-window state.
- Window lifecycle: spawn/close/focus/undock with `ManagedWindow{ id, open, unique_ptr<IWindow> impl }`.
- Global toasts and truly app-wide state.

## Mapping current features

Move the following into their own windows under `src/ui/Windows/`:

- ConsoleLogWindow
  - Holds all console settings and `TextEditor` state.
  - Implements the current filtering, incremental append, palette, search.
  - Persists: autoscroll, max_lines, buffer_cap, level_mask, filter_text, search prefs.

- CodeEditorWindow
  - Holds editor tabs list, `TextEditor` instances, language selection, open/save.
  - Renders its own local menu bar (Open/Save/Save As).
  - Persists: tabs (paths), current tab index, last folder; optionally caret/scroll positions.

- FilePreviewWindow
  - Holds preview state for text/image files, manages texture lifetime.
  - Persists: last opened path.

What stays in `WindowManager`:
- Dockspace, overlay, default layout
- Windows menu via registry, recent files menu (or a tiny service)
- Layout save/load coordination
- Min-size enforcement using `impl->minSize()`

## Persistence: JSON per window

Keep the ImGui ini, add a JSON file next to it.

Example `out/layouts/<name>.layout.json`:

```
{
  "version": 1,
  "next_id": 42,
  "last_folder": "C:/projects",
  "recent_files": ["C:/a.txt", "C:/b.cpp"],
  "windows": [
    {
      "id": "win-1",
      "type": "console-log",
      "title": "Console",
      "open": true,
      "state": {
        "autoscroll": true,
        "max_lines": 1000,
        "buffer_cap": 5000,
        "level_mask": 63,
        "text_filter": "",
        "search": { "query": "", "case": false }
      }
    },
    {
      "id": "win-2",
      "type": "code-editor",
      "title": "Editor",
      "open": true,
      "state": { "tabs": ["C:/main.cpp"], "current": 0 }
    }
  ]
}
```

Load flow:
- Manager reads JSON, iterates `windows[]`.
- For each: registry.create(type), set id/title/open, call `impl->deserialize(state)`.
- Unknown type: skip with a toast.

Save flow:
- Build the structure above by iterating open windows and calling `serialize`.

## Menus and default layout

- Windows menu items are generated from `WindowRegistry::enumerate()`. Selecting one calls `spawnWindow(typeId)`.
- Default layout uses spawned windows and docks them.
- Console/Editor/Preview menus embedded in each window; top-level menus can remain for global actions.

## Migration Plan (incremental)

1) Introduce skeleton: `IWindow`, `WindowContext`, `WindowRegistry` with no behavior changes.
2) Refactor Console into `ConsoleLogWindow`; route menus to the registry; remove console fields from manager.
3) Refactor Code Editor into `CodeEditorWindow`.
4) Refactor File Preview into `FilePreviewWindow`.
5) Switch layout metadata to JSON, keep current text loader for one release; write both, then read JSON preferentially.
6) Delete old per-window members from `WindowManager`.
7) Optional: extract `Notifications` and `FileDialog` tiny services accessed via `WindowContext`.

## Edge Cases & Considerations

- Unknown/removed types on load: skip; show toast; keep layout ini (docking still restored for remaining windows).
- Min-size constraints: consult `impl->minSize()` when splitting docks.
- Focus/undock: either keep manager flags or offer `WindowContext` helpers to request focus/undock by id.
- Versioning: include a `version` field in JSON; handle upgrades if/when state schema evolves.
- Testing: write minimal unit tests for serialization round-trip of each window type.

## Deliverables

- New headers: `src/ui/Window.h`, `src/ui/WindowContext.h`, `src/ui/WindowRegistry.h/.cpp`.
- Windows: `src/ui/Windows/ConsoleLogWindow.*`, `CodeEditorWindow.*`, `FilePreviewWindow.*`.
- Manager updates: spawn/close/focus API, JSON save/load, menu sourced from registry, min-size via window.

---

This plan ensures `WindowManager` remains stable in size and complexity as new windows are added; all window-specific state and UI live with the window.
