# ConfigurationManager

The `ConfigurationManager` owns runtime settings for the editor, tooling, and samples. It loads a JSON configuration file, applies environment overrides, and exposes typed accessors for the rest of the application.

## Key capabilities

- Bootstrap with `loadOrDefault()` to populate in-memory defaults for all managers.
- Persist and restore settings via `load()` / `save()`.
- Retrieve strongly-typed values using `getBool`, `getInt`, `getDouble`, `getString`, and `getStringList`.
- Override values at runtime through the `set(...)` overloads, then call `save()` to persist.
- Subscribe to changes with `subscribeOnChange` to keep dependent systems in sync.
- Export diagnostics snapshots using `exportCompact()`.

## Configuration file layout

The default config lives at `config.json` (see `paths::configFilePath()` for platform-specific locations). `loadOrDefault()` seeds sensible defaults if the file is missing or corrupted, covering:

- Window dimensions (`window.width`, `window.height`, fullscreen fields).
- UI theme selection (`ui.theme`).
- Texture service configuration (`textures.*`).
- Audio service configuration (`audio.*`).
- Hotkey overrides (`input.hotkeys` array) used by the editor to persist custom shortcuts.

Feel free to extend the schema—new keys automatically participate in typed getters/setters.

### Hotkey bindings block

The HotKeyManager reads and writes the `input.hotkeys` array. Each element tracks one action and its shortcut in canonical text form:

```jsonc
"input": {
    "hotkeys": [
        { "action": "global.openFileDialog", "shortcut": "Ctrl+O" },
        { "action": "codeEditor.saveFile", "shortcut": "Cmd+S" },
        { "action": "gameWindow.resetGame", "shortcut": null } // disabled until reassigned
    ]
}
```

- Missing or `null` shortcuts disable the action.
- Unknown action IDs are ignored during load with a warning, preserving forward compatibility across branches.
- The Hotkeys UI rewrites this block when you click **Save**, preserving comments or non-action entries (e.g., separator objects) where possible.

## Environment overrides

The manager reads environment variables prefixed with `GB2D_`. Each override maps to a dotted key using double underscores as separators:

- `GB2D_WINDOW__FULLSCREEN=true` → `window.fullscreen`
- `GB2D_AUDIO__MASTER_VOLUME=0.5` → `audio.master_volume`

Values are parsed into booleans, integers, doubles, or strings depending on the literal. Overrides apply after the configuration file is loaded, so they win over disk content.

## Configuration window

Open the in-app configuration UI from **Window → Configuration** or with the `Ctrl+,` shortcut. The window reflects the live schema provided by `ConfigurationManager::schema()` and keeps runtime state in sync with edits.

### Navigation & search

- A tree on the left groups sections in schema order. Advanced sections are hidden until you enable **Show advanced settings**.
- Use the search box to filter sections and highlight matching fields. Clearing the search returns to the full hierarchy.
- Each section carries dirty and error badges. Expanding a section reveals per-field badges so you can quickly spot unsaved or invalid edits.

### Editing & validation

- Field editors are generated from schema metadata (type, ranges, enum values, hints). Specialized widgets exist for hotkeys, file paths, lists, and numeric ranges.
- Validation runs on every edit. Inline messages explain failures and the toolbar disables **Apply** and **Save** until all errors are resolved or reverted.
- Unknown or custom keys fall back to an embedded JSON text editor so power users can still manage bespoke settings.

### Apply, Save, and backups

- **Apply** commits staged edits to the running `ConfigurationManager`, triggering change notifications for dependent systems without touching disk.
- **Save** performs Apply and then writes `config.json` atomically. The first successful save of the session also creates a `config.backup.json` snapshot in the same directory.
- When edits diverge from disk, closing the window prompts you to apply, save, or discard. Field, section, and global revert actions are available at any time.
- The window honors `GB2D_CONFIG_DIR`; any runtime override affects which configuration directory receives writes.

### Advanced toggles & JSON fallback

- Enable **Show advanced settings** to expose fields flagged as advanced or experimental in the schema. Use this view when tuning developer-only knobs.
- Sections that contain unsupported field types (or schema extensions still awaiting bespoke widgets) render a JSON blob editor. Changes there participate in dirty tracking and validation just like standard fields.

For guidance on extending the schema or adding new field editors, see [Configuration window developer notes](configuration-window-developer.md).

## Runtime updates & notifications

Any of the `set(key, value)` overloads mutate the in-memory document. Calling `save()` writes the JSON atomically and notifies subscribers registered through `subscribeOnChange`. Use the subscription system for live settings editors or hot-reload tooling.

```cpp
const int subscriptionId = ConfigurationManager::subscribeOnChange([](){
    gb2d::logging::LogManager::info("Configuration saved. Reloading audio cache...");
    gb2d::audio::AudioManager::reloadAll();
});

ConfigurationManager::set("audio.master_volume", 0.65);
ConfigurationManager::save();
ConfigurationManager::unsubscribe(subscriptionId);
```

`exportCompact()` produces a minified JSON string. This is helpful when piping the active configuration into logs or bug reports.
