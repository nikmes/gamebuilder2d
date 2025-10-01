# Data Model: GameWindow Fullscreen Mode

## Configuration Entries
| Key                              | Type   | Default | Description |
|----------------------------------|--------|---------|-------------|
| `window::width`                  | int    | `1280` | Editor window width in windowed mode. |
| `window::height`                 | int    | `720`  | Editor window height in windowed mode. |
| `window::fullscreen`             | bool   | `false` | If true, the editor launches directly in fullscreen. |
| `fullscreen::width`              | int    | `1920` | Desired width when entering the gameplay fullscreen session. |
| `fullscreen::height`             | int    | `1080` | Desired height when entering the gameplay fullscreen session. |
| `window::resume_fullscreen`      | bool   | `false` | Whether the app should automatically enter fullscreen mode on startup. |
| `window::fullscreen_last_game`   | string | `""`   | ID of the game to resume in fullscreen mode. Empty string indicates "none". |

## FullscreenSession State
| Field                | Type      | Notes |
|----------------------|-----------|-------|
| `active`             | bool      | True while fullscreen is active. |
| `game`               | Game*     | Pointer to currently running game instance (owned by `GameWindow`). |
| `previousWidth`      | int       | Window width before entering fullscreen. |
| `previousHeight`     | int       | Window height before entering fullscreen. |
| `wasFullscreen`      | bool      | Tracks whether we were already in fullscreen to avoid redundant toggles. |
| `monitorIndex`       | int       | Selected monitor (default 0). |
| `overlayTimer`       | float     | Seconds remaining for on-screen instructions. |

## Events & Signals
- `FullscreenRequested(gameId)` – emitted by `GameWindow` when user clicks the button.
- `FullscreenExited()` – emitted when shortcut/escape triggered; allows UI to refresh render target.
