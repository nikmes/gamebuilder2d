# AudioManager configuration

GameBuilder2d ships with a centralized `AudioManager` service that owns the Raylib audio device, caches short SFX (`Sound`), streams long-form music (`Music`), and exposes playback utilities to the editor and runtime subsystems. This document describes how to configure the manager through `config.json` (and matching environment overrides).

## Configuration block

Add or edit the `audio` block near the root of `config.json`:

```jsonc
"audio": {
  "enabled": true,
  "master_volume": 1.0,
  "music_volume": 1.0,
  "sfx_volume": 1.0,
  "max_concurrent_sounds": 16,
  "search_paths": [
    "assets/audio"
  ],
  "preload_sounds": [],
  "preload_music": []
}
```

All keys are optional; anything omitted falls back to the defaults listed above. Search paths are resolved relative to the process working directory (for VS/Windows builds this is the executable directory thanks to the preset debug configuration). When multiple paths are listed, they are checked in order until a match is found.

## Key reference

| Key | Type / Range | Default | Notes |
| --- | --- | --- | --- |
| `audio.enabled` | `bool` | `true` | Disables all device work when `false`. Playback requests become silent no-ops with log hints. |
| `audio.master_volume` | `float` (`0.0`–`1.0`) | `1.0` | Forwarded to Raylib’s `SetMasterVolume` once the device is ready. |
| `audio.music_volume` | `float` (`0.0`–`1.0`) | `1.0` | Applied to each music stream before playback. |
| `audio.sfx_volume` | `float` (`0.0`–`1.0`) | `1.0` | Multiplies per-sound volume requests. |
| `audio.max_concurrent_sounds` | `int` ≥ `0` | `16` | Caps simultaneously active SFX alias slots; additional requests are throttled with warnings. |
| `audio.search_paths` | `string[]` | `["assets/audio"]` | Ordered list of directories used to resolve relative sound/music identifiers. |
| `audio.preload_sounds` | `string[]` | `[]` | Identifiers to eagerly load as `Sound` during `AudioManager::init`. |
| `audio.preload_music` | `string[]` | `[]` | Identifiers to preload/prepare as streaming `Music` during init. |

## Environment overrides

Every configuration entry can be overridden without touching disk by using the existing `GB2D_` environment convention. Examples:

```powershell
setx GB2D_AUDIO__ENABLED false
setx GB2D_AUDIO__MAX_CONCURRENT_SOUNDS 8
setx GB2D_AUDIO__SEARCH_PATHS "assets/audio;dlc/audio"
```

Lists use the same semicolon-separated syntax as other services. Overrides are read the next time the application boots (or when the configuration manager reloads the active profile).

## Runtime updates

Calling `ConfigurationManager::reload()` or hot-editing `config.json` while the tool is running re-applies master/music/SFX volumes immediately where Raylib allows. For broader changes—like adjusting preload lists—trigger `AudioManager::reloadAll()` to flush caches and repopulate using the latest configuration.

## Real-time playback updates

`AudioManager::playSound` returns a `PlaybackHandle` that remains valid while the sound is active. Use `AudioManager::updateSoundPlayback(handle, params)` to adjust the volume, pitch, or stereo pan without restarting the sample. The editor's File Preview window uses this to reflect slider movements instantly.

```cpp
using gb2d::audio::AudioManager;

auto handle = AudioManager::playSound("ui/click.wav");

gb2d::audio::PlaybackParams params;
params.volume = 0.5f;
params.pan = 0.25f; // bias left
AudioManager::updateSoundPlayback(handle, params);
```

Call `AudioManager::stopSound(handle)` or let the clip finish naturally to release the slot. Handles become invalid automatically when the sound ends.

For additional usage patterns (acquiring sounds, releasing handles, diagnostics), refer to the AudioManager API in `GameBuilder2d/src/services/audio/AudioManager.h`.
