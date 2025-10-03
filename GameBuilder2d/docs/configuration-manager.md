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

Feel free to extend the schema—new keys automatically participate in typed getters/setters.

## Environment overrides

The manager reads environment variables prefixed with `GB2D_`. Each override maps to a dotted key using double underscores as separators:

- `GB2D_WINDOW__FULLSCREEN=true` → `window.fullscreen`
- `GB2D_AUDIO__MASTER_VOLUME=0.5` → `audio.master_volume`

Values are parsed into booleans, integers, doubles, or strings depending on the literal. Overrides apply after the configuration file is loaded, so they win over disk content.

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
