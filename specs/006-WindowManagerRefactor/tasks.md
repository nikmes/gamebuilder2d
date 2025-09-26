# Tasks: WindowManager Refactor

## Phase 0 – Scaffolding
- [x] Add `src/ui/Window.h` (IWindow) and `src/ui/WindowContext.h`
- [x] Add `src/ui/WindowRegistry.h/.cpp`
- [x] Wire up registry initialization at app start; no behavior change (WindowManager member added)

## Phase 1 – ConsoleLogWindow
- [x] Create `src/ui/Windows/ConsoleLogWindow.{h,cpp}`
- [x] Move console state and logic from `WindowManager` into the new class
- [x] Expose `typeId="console-log"`, `displayName="Console Log"`
- [x] Implement JSON `serialize/deserialize`
- [x] Register in `WindowRegistry`
 - [x] Replace manager menu entry to spawn via registry
- [x] Remove console fields/methods from `WindowManager`

## Phase 2 – CodeEditorWindow
- [x] Create `src/ui/Windows/CodeEditorWindow.{h,cpp}`
- [x] Move editor state (tabs, language mapping, open/save)
- [x] Implement JSON `serialize/deserialize`
- [x] Register in registry and adjust menus
- [x] Remove editor fields/methods from `WindowManager`

## Phase 3 – FilePreviewWindow
- [x] Create `src/ui/Windows/FilePreviewWindow.{h,cpp}`
- [x] Move preview state and texture lifetime handling
- [x] Implement JSON `serialize/deserialize`
- [x] Register in registry and adjust menus
- [x] Remove preview map and helpers from `WindowManager`

## Phase 4 – Persistence (JSON)
- [x] Implement manager-level JSON save/load (`*.layout.json`)
- [x] Keep reading legacy `.wm.txt` for one release; write JSON in parallel
 - [x] Switch menus and default layout builder to use registry-spawned windows

## Phase 5 – Cleanup and Tests
- [x] Delete unused fields and methods from `WindowManager`
- [x] Remove legacy `src/window/*` duplicates from tree (archived under `src/_legacy/window`)
- [x] Prune unused local thirdparty shims and empty folders (e.g., `thirdparty/imguicolortextedit_compat.cpp`, `thirdparty/ImGuiColorTextEdit/TextEditor_shim.cpp`, `thirdparty/ImGuiColorTextEdit/TextEditor.cpp`, `thirdparty/rlimgui_stub/`, empty `src/services/texture/`, empty `src/gamebuilder/`)
- [ ] Add unit tests for window state round-trip via JSON for each window type
- [ ] Optional: Extract `Notifications` and `FileDialog` services

## Stretch
- [ ] Add version upgrades in layout JSON
- [ ] Persist per-window UI geometry (split decision is still in ImGui ini)
- [ ] Hot-reload window types (if ever needed)
