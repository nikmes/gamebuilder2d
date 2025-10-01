# TextureManager Developer Guide

The TextureManager centralizes all Raylib texture loading for GameBuilder2d. It owns the filesystem lookup, keeps GPU resources cached, and enforces reference counting so that editor panels and gameplay code can share the same textures without redundant loads.

## Initialization & Shutdown

The manager is a global service. Initialize it after the Raylib context is ready and shut it down before closing the window:

```cpp
#include "services/texture/TextureManager.h"

int main() {
    InitWindow(width, height, "GameBuilder2d");
    gb2d::textures::TextureManager::init();

    // ... run application ...

    gb2d::textures::TextureManager::shutdown();
    CloseWindow();
}
```

The bootstrap in `GameBuilder2d/src/GameBuilder2d.cpp` already calls both methods; other entry points should follow the same pattern.

## Configuration Keys

Texture lookups honour the configuration service. Keys live under the `textures::` namespace in `config.json` (or environment overrides).

| Key | Type | Default | Description |
| --- | --- | --- | --- |
| `textures::search_paths` | string list | `["assets/textures"]` | Directories scanned to resolve relative texture identifiers. |
| `textures::default_filter` | string | `"bilinear"` | Raylib filter applied after load (`nearest`, `bilinear`, `trilinear`, `anisotropic`). |
| `textures::generate_mipmaps` | bool | `false` | If `true`, `GenTextureMipmaps` runs on load. |
| `textures::max_bytes` | int | `0` | Optional VRAM budget in bytes; exceeding it triggers a warning. |
| `textures::placeholder_path` | string | `""` | Optional custom placeholder asset. Falls back to a magenta/black checkerboard when empty or invalid. |

Reload configuration after editing the JSON or supply overrides via environment variables (`GB2D_TEXTURES__SEARCH_PATHS=...`).

## Requesting Textures

Use `acquire` to load (or retrieve) a texture:

```cpp
#include "services/texture/TextureManager.h"

using gb2d::textures::TextureManager;
using AcquireResult = gb2d::textures::AcquireResult;

AcquireResult icon = TextureManager::acquire("ui/game-icons/space-invaders.png", "game-window/icon/space-invaders");
if (!icon.texture) {
    // Placeholder returned – texture failed to load.
}
// icon.texture stays valid until all holders call TextureManager::release(icon.key)
```

- **Identifier**: Any relative or absolute path. Relative paths are resolved against `textures::search_paths`.
- **Alias** (optional): Provide a stable logical key when the file path may change. All `acquire` calls with the same alias reuse the cached texture.

### UI Example (GameWindow icons)

`GameWindow` acquires game icons during registration and releases them when the window shuts down. The window stores the returned key and checks `AcquireResult::placeholder` to expose tooltips when the asset is missing.

### Gameplay Example (Fullscreen Session)

Gameplay systems can request art the same way:

```cpp
struct EnemySprite {
    gb2d::textures::AcquireResult texture;

    void load() {
        texture = TextureManager::acquire("games/galaga/enemy.png");
    }

    void unload() {
        if (!texture.key.empty()) {
            TextureManager::release(texture.key);
        }
        texture = {};
    }
};
```

Call `TextureManager::release` once per successful `acquire`. The manager decrements a reference count and unloads the GPU handle when it reaches zero.

## Placeholder, Errors & Logging

When a file cannot be found or decoded, the manager logs a warning via `LogManager` and returns the placeholder texture. The `AcquireResult::placeholder` flag helps UI call sites set expectations (e.g., tooltips). Placeholders participate in reference counting like any other texture; releasing the key is still required.

## Diagnostics & Metrics

Query cache health at runtime:

```cpp
const auto metrics = TextureManager::metrics();
LOG_INFO("Loaded textures: {} ({} placeholders) – ~{} bytes", metrics.totalTextures,
         metrics.placeholderTextures, metrics.totalBytes);
```

This is ideal for diagnostics overlays or debug consoles. When `textures::max_bytes` is non-zero, the manager logs an extra warning the first time the budget is exceeded.

## Reloading Textures

Call `TextureManager::reloadAll()` to unload and reload every cached entry. Use this when device resources are lost or when iterating on texture assets while the editor runs. The return value reports how many textures were attempted, succeeded, or fell back to placeholders.

```cpp
const auto summary = TextureManager::reloadAll();
LOG_INFO("Reloaded textures: {}/{} ({} placeholders)",
         summary.succeeded, summary.attempted, summary.placeholders);
```

## Future Enhancements

The following improvements are tracked for later iterations:

- Asynchronous/accrual background loading to keep the main thread responsive.
- Hot-reload hooks (filesystem watchers) to auto-refresh assets in place.
- LRU or time-based eviction to keep the cache within memory budgets.
- Optional on-disk compression / streaming for large texture sets.

Keep these ideas in mind when evolving the service or planning next milestones.

## Quick Checklist

1. Call `TextureManager::init()` after creating the Raylib window.
2. Configure search paths and filters in `config.json`.
3. Use `acquire` + `release` to manage lifetime; store the returned key.
4. Handle placeholders gracefully (e.g., fall back UI, log warnings).
5. Surface `metrics()` and `reloadAll()` in diagnostic tooling.
