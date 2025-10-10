# Feature Specification: Audio Manager Window

**Feature Branch**: `012-audio-manager-window`

**Created**: 2025-10-10  
**Status**: Draft  
**Input**: "Spec an AudioManagerWindow to manage and preview all audio, sounds, parameters, configuration of AudioManager, similar way we did for ConfigurationManagerWindow."

---

## Execution Flow *(authoring guardrail)*
```
1. Capture the user intent and articulate desired outcomes
2. Identify actors, actions, data, constraints, and success criteria
3. Surface ambiguities as [NEEDS CLARIFICATION: question]
4. Describe user scenarios and acceptance tests focused on observable behavior
5. List functional requirements; add non-functional where relevant
6. Model key entities / data that must exist to support the feature
7. Document edge cases, risks, and assumptions
8. Run self-check: requirements are testable and avoid premature implementation bias
```

---

## User Scenarios & Testing *(mandatory)*

### Primary User Story
As a GameBuilder2d user, I can open an Audio Manager window that presents loaded audio assets (sounds and music), previews them on demand, adjusts global and per-asset parameters (volumes, pitch, pan), and manages configuration settings without editing raw JSON.

### Acceptance Scenarios
1. **Asset inventory & preview** — Given AudioManager has loaded sounds and music, when the user opens the Audio Manager window, then the UI displays a list/tree of assets grouped by type (sounds/music), showing metadata like duration, ref count, and a play button to preview each asset.
2. **Preview controls** — Given a selected asset, when the user clicks play, then the audio plays with configurable parameters (volume, pitch, pan), and the UI shows playback progress, stop/pause controls, and loop toggle.
3. **Parameter editing** — Given global settings (master volume, music volume, SFX volume), when the user adjusts sliders, then changes apply immediately to running audio and persist on save.
4. **Configuration management** — Given config.json audio section, when the user edits search paths or preload lists, then the UI validates inputs and allows applying/reloading without restart.
5. **Diagnostics & metrics** — Given real-time audio state, when the user views diagnostics, then the UI shows active voices, memory usage, buffer underruns, and throttle counters.
6. **Event logging** — Given audio events (load, play, stop), when the user enables event sink, then the UI displays a scrollable log of events with filtering by channel (assets, playback, preview, diagnostics).

### Edge Cases & Decisions
- Placeholder assets should be visually distinguished (e.g., grayed out) with tooltips explaining why they can't be previewed.
- Preview playback should use a dedicated context to avoid interfering with gameplay audio.
- Large inventories (>100 assets) require search/filtering and pagination.
- Silent mode disables previews and shows appropriate messaging.
- Unsaved parameter changes should prompt on close, similar to ConfigurationManagerWindow.
- Preview sessions must clean up automatically to prevent resource leaks.

### Clarification Outcomes *(2025-10-10)*
- **Preview mechanism**: Use AudioManager's beginPreview/endPreview APIs for isolated playback.
- **UI layout**: Split-pane: left panel for asset list/config, right for preview controls/diagnostics.
- **Persistence**: Changes to volumes/config save to config.json; per-asset params are runtime-only unless specified.
- **Integration**: Window registers with WindowManager like ConfigurationManagerWindow.

#### Audio inventory & preview APIs *(T201)*
- AudioManager provides captureInventorySnapshot() returning SoundInventoryRecord and MusicInventoryRecord vectors.
- Preview uses beginPreview(PreviewRequest) returning PreviewPlaybackHandle for control.
- Events subscribe via AudioEventSubscription for real-time updates.

#### UI components & controls *(T202)*
- Asset list: Tree view with icons for sound/music, columns for key, duration, ref count.
- Preview panel: Waveform visualization (if possible), progress bar, volume/pitch/pan sliders, play/stop/pause/loop buttons.
- Config section: Sliders for volumes, text inputs for paths, list editors for preloads.
- Diagnostics: Read-only gauges for voices, memory, underruns.

#### Event handling & state management *(T203)*
- Window subscribes to audio events on open, unsubscribes on close.
- Dirty state for config changes, prompt on close.
- Preview handles managed per session, auto-cleanup on window close.

## Functional Requirements *(mandatory)*

### Core Features
- FR-001: Display audio inventory from AudioManager snapshots.
- FR-002: Provide preview playback controls for individual assets.
- FR-003: Allow editing of global audio parameters (volumes, etc.).
- FR-004: Integrate configuration editing for audio section.
- FR-005: Show real-time diagnostics and metrics.
- FR-006: Log and filter audio events.

### Non-Functional
- NFR-001: UI responsive, no blocking on preview operations.
- NFR-002: Previews isolated from gameplay audio.
- NFR-003: Changes persist correctly on save.

## Data Model *(mandatory)*

### Key Entities
- **AudioAssetItem**: Wraps inventory record with UI state (selected, playing).
- **PreviewSession**: Tracks active preview handle, progress, params.
- **ConfigSection**: Mirrors ConfigurationManager schema for audio.
- **EventLogEntry**: Timestamp, type, details for display.

### Relationships
- Window owns list of AudioAssetItem, current PreviewSession, ConfigSection edits.
- Subscribes to AudioEventSink for updates.

## Edge Cases, Risks, Assumptions *(mandatory)*
- Assumption: AudioManager APIs are stable and provide required snapshots/events.
- Risk: Preview resource leaks if handles not cleaned up.
- Edge: Silent mode disables interactive features.

## Self-Check *(mandatory)*
- Requirements are testable via UI interactions and AudioManager state checks.
- No premature implementation bias; focused on user behavior.</content>
<parameter name="filePath">c:\Users\nikme\source\repos\GameBuilder2d\specs\012-audio-manager-window\spec.md