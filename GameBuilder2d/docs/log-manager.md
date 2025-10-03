# LogManager

`gb2d::logging::LogManager` wraps spdlog and feeds both text files/console output and the editor's ImGui log window. Use it to configure global logging and to surface messages to tooling.

## Quick start

```cpp
using namespace gb2d::logging;

LogManager::init({
    .name = "GameBuilder2D",
    .level = Level::debug,
    .pattern = "[%H:%M:%S.%e] [%^%l%$] %v"
});

LogManager::info("Boot sequence started: version {}", appVersion);
LogManager::warn("Missing texture: {}", path);
```

Call `LogManager::shutdown()` during teardown to flush sinks cleanly. Reconfiguration is safe while the manager is running:

```cpp
LogManager::reconfigure({ .level = Level::trace });
```

## Log levels

| Level enum        | Typical usage                             |
| ----------------- | ----------------------------------------- |
| `Level::trace`    | Verbose diagnostics and per-frame traces. |
| `Level::debug`    | Development-time status updates.          |
| `Level::info`     | High-level lifecycle, user-facing notices.|
| `Level::warn`     | Recoverable issues worth highlighting.    |
| `Level::err`      | Failures that require attention.          |
| `Level::critical` | Fatal faults; usually followed by abort.  |
| `Level::off`      | Silence the logger entirely.              |

Use the convenience shorthands (`LogManager::info`, `LogManager::error`, etc.) for formatted output. Formatting errors are swallowed, so malformed format strings won’t crash the application.

## ImGui log window helpers

The editor’s console window subscribes to a ring buffer populated by the logging sink. The following helpers make the buffer accessible to other tooling:

- `read_log_lines_snapshot(maxLines)` – returns a copy of the latest lines for custom viewers.
- `clear_log_buffer()` – wipes the in-memory ring; the console window exposes this via its toolbar.
- `set_log_buffer_capacity(capacity)` – adjust how many entries are retained.
- `level_to_label(level)` – maps a `Level` enum to a human-readable string.

Use these when building additional UI widgets (e.g., in modular ImGui windows) that need direct logger integration.
