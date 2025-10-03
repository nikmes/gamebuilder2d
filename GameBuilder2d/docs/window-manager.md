# WindowManager

`gb2d::WindowManager` coordinates the editor dockspace, modular ImGui windows, and saved layouts. It is responsible for spawning windows, docking them into regions, and persisting layout preferences between runs.

## Responsibilities

- Build and maintain the master dockspace for the editor UI.
- Spawn windows on demand from registered modular types (`WindowRegistry`).
- Manage fullscreen sessions so game previews can take over the viewport.
- Load and save named layouts (`out/layouts/<name>.layout.json`).
- Provide utility actions such as docking, undocking, resizing regions, and tab reordering.

## Creating & docking windows

```cpp
gb2d::WindowManager wm;

// Spawn a modular window that was registered with WindowRegistry
const std::string id = wm.spawnWindowByType("FilePreviewWindow");

// Optionally dock it relative to existing regions
wm.dockWindow(id, /* target region id */ "dock_main", gb2d::DockPosition::Right);
```

Each managed window keeps its own metadata (`ManagedWindow`):

- `id` – unique identifier assigned by the manager.
- `title` – user-facing caption (can be overridden by the window implementation).
- `open` – tracks whether ImGui should render the window.
- `impl` – optional modular window implementation with its own render logic.

Use `undockWindow`, `closeWindow`, and `reorderTabs` to script the layout programmatically.

## Layout persistence

Call `saveLayout(name)` and `loadLayout(name)` to persist user arrangements. Layout files live under `out/layouts/` and can be surfaced in custom UI (the default layout picker already does this). Layout saves are idempotent so you can safely call them when quitting the editor.

```cpp
wm.saveLayout("my-artist-setup");
wm.loadLayout("programmer-default");
```

## Fullscreen sessions

For game previews, pass a `FullscreenSession*` via `setFullscreenSession`. The manager blends the runtime session into the dockspace while preserving editor controls. `toggleEditorFullscreen()` and `setEditorFullscreen(bool)` are used internally to switch modes during play sessions.

## Toasts & notifications

The manager includes a lightweight toast system used to notify users about layout operations (e.g., successful saves). `addToast(text, seconds)` queues messages; they are rendered during `renderUI()` and expire after their timer elapses.

## Shutdown considerations

Call `shutdown()` before tearing down ImGui or raylib. This ensures modular windows can release their resources and avoids dangling references during global/static destruction.
