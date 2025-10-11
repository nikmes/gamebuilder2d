# Audio Manager Window

The Audio Manager window gives the editor a dedicated control surface for browsing loaded audio assets, previewing sounds and music, editing runtime audio settings, and monitoring recent audio activity. This guide walks through the main workflows so you can get productive quickly.

## Opening the window

- **Menu**: Navigate to `Settings → Audio Manager…` from the main menu bar.
- **Hotkey**: Bind the `global.openAudioManager` action inside the Hotkeys window (`Settings → Hotkeys…`) to summon it instantly.
- When opened for the first time it appears as a dockable pane sized for readability (512×512). Drag the title bar or use the dock targets to place it alongside other tools. Your layout persists when layouts are saved via the `Layouts` menu.

## Layout overview

The window is divided into a left-hand asset browser and a right-hand tab strip with three functional areas:

1. **Asset list** — shows all registered sounds and music tracks along with metadata and inline preview controls.
2. **Preview tab** — detailed playback controls and status for the currently selected asset.
3. **Config tab** — live editor for the `audio` block in `config.json`, including device state readouts.
4. **Diagnostics tab** — recent event log and health indicators for the audio subsystem.

Use the split view to keep the asset list visible while working through the detail tabs. Selecting a sound or music record in the list updates the Preview tab automatically.

## Asset list basics

- Sounds and music are grouped separately with item counts.
- Placeholder rows (assets the project expects but has not imported) render in gray, while actively previewed items highlight in green.
- Each entry exposes **Play** / **Stop** buttons. Clicking Play loads and starts the preview immediately; Stop halts playback and releases any live handles.
- Use the **Refresh** button at the top of the list to pull a fresh inventory snapshot after importing new audio or changing search paths.

## Preview tab controls

Once an asset is selected:

- The header displays key metadata (length, sample rate, channels, file path).
- For **sounds**, adjust volume, pitch, and stereo pan while the clip plays; changes apply in real time through the playback handle.
- For **music streams**, additional controls provide pause/resume, seek/progress display, and independent volume adjustment.
- Status banners surface common issues such as “device not initialized” or “silent mode active,” letting you diagnose why a preview might not produce audio.
- When playback completes or you start previewing another asset, the UI resets automatically.

## Config tab workflow

- The top panel mirrors the current device state (initialized flag, backend name, concurrent sound budget, etc.).
- Editable fields map directly to the `audio` block:
  - Master, music, and SFX volume sliders (`0.0`–`1.0`).
  - Enable/disable toggle for the entire subsystem.
  - Max concurrent sounds integer.
  - Search path editor with add/remove controls and inline validation.
- Pending changes raise a **dirty banner** and add a *•* badge to the tab so you always know when edits need attention.
- Use **Apply** to persist through `ConfigurationManager`; this reinitializes the `AudioManager`, reloads active assets, and clears the dirty state.
- Use **Discard** to revert to the last applied configuration. Closing the window with unapplied edits triggers a confirmation modal that forces Apply or Discard to avoid accidental loss.

## Diagnostics & event log

- The diagnostics summary shows live counters (loaded sounds/music, active previews) and device flags so you can confirm the subsystem’s health at a glance.
- The event log captures recent audio events—loads, unloads, preview start/stop, device errors—with timestamps in `HH:MM:SS` format. Use **Clear Log** to wipe the list when focusing on new activity.
- The log auto-truncates after 100 entries, so older events roll off without bloating memory.

## Closing & cleanup

The Audio Manager window unsubscribes from events and stops any active preview when closed. Because window layout persistence is handled by the Window Manager, once you save your layout the window will reopen docked exactly where you left it. If you close it manually, it stays closed until you reopen it through the menu or your custom hotkey.
