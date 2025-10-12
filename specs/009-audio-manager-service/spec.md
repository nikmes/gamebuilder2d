# Feature Specification: AudioManager Service for GameBuilder2d

**Feature Branch**: `009-audio-manager-service`

**Created**: 2025-10-01  
**Status**: In Review  
**Input**: "Introduce a centralized AudioManager that owns Raylib audio device lifecycle, loads and caches sound/music assets, supports concurrent sound effects, manages streaming music playback, and exposes tooling-friendly diagnostics consistent with other engine services."

---

## Execution Flow (authoring guardrail)
```
1. Capture the user intent and articulate desired outcomes
2. Identify actors, actions, data, constraints, and success criteria
3. Surface ambiguities as [NEEDS CLARIFICATION: question]
4. Describe user scenarios and acceptance tests focused on observable behavior
5. List functional requirements; add non-functional where relevant
6. Model key entities / data that must exist to support the feature
7. Document edge cases, risks, and assumptions
8. Run self-check: no implementation-level prescriptions, requirements are testable
```

---

## User Scenarios & Testing *(mandatory)*

### Primary User Story
As a GameBuilder2d developer, I can request audio playback (sound effects or music) through a shared AudioManager so that gameplay systems and editor tooling can reuse cached assets, stream long tracks, and manage lifetime without manual Raylib bookkeeping.

### Acceptance Scenarios
1. **Device bootstrap** — Given audio is disabled by default, when the application calls `AudioManager::initialize`, then the Raylib audio device opens, configuration defaults apply, and the manager reports readiness.
2. **Sound acquisition & caching** — Given a WAV/OGG sample is available and not yet cached, when a caller acquires it through the manager, the asset loads once, is cached by alias, and subsequent acquisitions reuse the cached data while ref counts increase.
3. **Concurrent sound playback** — Given the same sound is triggered rapidly, when callers play it through the manager, then it is mixed concurrently using available alias slots without reloading the asset, up to the configured maximum.
4. **Streaming music control** — Given a long-form music file is registered, when the caller starts playback, then the manager streams it, exposes play/pause/stop APIs, and continues feeding `UpdateMusicStream` during ticks until stopped or completed.
5. **Graceful unload** — Given multiple systems reference the same sound, when each releases its handle, then the manager unloads the asset only after the final release and frees associated Raylib resources.
6. **Failure fallback** — Given a request references a missing or corrupt audio file, when the manager attempts to load it, then it logs the error, returns a placeholder/no-op handle, and avoids crashing playback calls that follow.
7. **Diagnostics visibility** — Given the developer opens an audio diagnostics overlay, when the manager is queried, then it returns metrics (loaded sounds, active streams, channel utilization, memory footprint) for display.

### Edge Cases & Decisions
- Missing assets provide a silent fallback handle (plays nothing) while logging warnings; optional future configuration may swap in a default beep.
- The manager enforces a configurable cap on concurrent alias slots; excess requests log a throttling warning and are dropped or queued per policy.
- Reloading configuration at runtime (e.g., master volume changes) propagates to active sounds and music immediately where Raylib APIs allow.
- Hot-reloading audio files from disk is out of scope; manual `reloadAll` triggers stop current playback before reloading.
- Audio initialization failure (e.g., no device) keeps the manager in a "degraded" state where APIs succeed but perform no playback, and diagnostics expose the reason.

### Clarification Outcomes *(2025-10-01)*
- **Silent mode policy**: When audio device initialization fails (or `audio.core.enabled` is `false`), the manager enters silent mode. All public APIs remain callable, but they return placeholder handles and skip Raylib playback. Initialization logs the failure reason, diagnostics expose `silentMode=true`, and follow-up calls avoid reopening the device until `resetForTesting()` or process restart.
- **Configuration schema**:
	- `audio.core.enabled` *(bool, default `true`)* — toggles the entire manager and places the system in silent mode when `false`.
	- `audio.volumes.master` *(double 0.0–1.0, default `1.0`)* — forwarded to `SetMasterVolume` when the device is ready.
	- `audio.volumes.music` *(double 0.0–1.0, default `1.0`)* — multiplier applied per music stream.
	- `audio.volumes.sfx` *(double 0.0–1.0, default `1.0`)* — multiplier applied per active sound slot.
	- `audio.engine.max_concurrent_sounds` *(int ≥0, default `16`)* — caps simultaneous sound slots; overflow requests are dropped with throttling logs.
	- `audio.engine.search_paths` *(string list, default `["assets/audio"]`)* — ordered lookup roots for relative identifiers.
	- `audio.preload.sounds` *(string list, default `[]`)* — canonical identifiers preloaded during init when the device is ready.
	- `audio.preload.music` *(string list, default `[]`)* — canonical identifiers preloaded as streaming music.
	- `audio.preload.sound_aliases` / `audio.preload.music_aliases` *(object maps, default `{}`)* — optional alias tables wired into the preload UI.
- **Placeholder strategy**: Missing or failed sound loads create `Sound` entries flagged as placeholders; playback returns invalid/quiet handles that succeed operationally but emit warnings. Music behaves similarly, staying paused with a placeholder `Music` struct. Diagnostics expose placeholder counts so developers can spot unresolved assets. No synthetic beep is played in v1.

---

## Requirements *(mandatory)*

### Functional Requirements
- **FR-001**: The system MUST expose a globally accessible AudioManager service aligned with existing manager registration patterns.
- **FR-002**: The manager MUST initialize and shutdown the Raylib audio device exactly once, preventing duplicate init calls and ensuring teardown occurs before application exit.
- **FR-003**: The manager MUST load and cache short-form audio assets (`Sound`) on demand, keyed by canonical alias, with reference counting for lifetime management.
- **FR-004**: The manager MUST support playing the same sound concurrently via Raylib aliases (or equivalent) while respecting a configurable channel limit.
- **FR-005**: The manager MUST provide APIs for acquiring, playing, pausing, resuming, seeking, and stopping streamed music (`Music`) assets.
- **FR-006**: The manager MUST offer non-owning lookup APIs that return read-only handles for already-loaded sounds or music without altering reference counts.
- **FR-007**: The manager MUST expose release APIs that decrement reference counts and unload assets once zero references remain.
- **FR-008**: The manager MUST provide a `reloadAll` operation that stops active playback, reloads cached assets, and reports aggregate success/failure.
- **FR-009**: The manager MUST emit log entries for initialization, playback, throttling, load/unload events, and error conditions.
- **FR-010**: The manager MUST surface metrics and state suitable for diagnostics (e.g., total sounds, active channels, master/channel volumes).
- **FR-011**: The manager MUST allow configuration (via `config.json`) of master volume, per-channel volumes, search paths, preload lists, and max concurrent sounds.

### Non-Functional Requirements
- **NFR-001**: Sound lookup SHOULD be O(1) on average using hash maps with stable aliases.
- **NFR-002**: Playback APIs SHOULD be safe to call from both main thread and gameplay scripts, assuming Raylib thread-safety constraints are respected.
- **NFR-003**: Audio device initialization SHOULD fail gracefully without crashing, keeping the manager usable in "silent" mode.
- **NFR-004**: Logging SHOULD avoid spamming repeated throttling messages by rate-limiting warnings per interval.

### Out of Scope (initial release)
- Real-time DSP effects (reverb, echo), positional audio, or 3D spatialization.
- Automatic hot-reload of audio files when they change on disk.
- Persistent cross-session playback state (e.g., remembering playhead positions between editor runs).

---

## Key Entities & Data
- **AudioManager Service**: Singleton façade providing initialization, tick, acquire/release, playback control, diagnostics, and configuration update APIs.
- **SoundRecord**: Internal struct containing Raylib `Sound`, canonical path, reference count, playback usage metrics, and configuration flags.
- **MusicRecord**: Internal struct wrapping Raylib `Music`, loop settings, volume, playback state, and observers waiting on completion events.
- **PlaybackHandle**: Lightweight identifier returned to callers for managing individual concurrent playbacks (e.g., channel slot index plus generation counter).
- **AudioConfig**: Structured settings describing default volumes, channel caps, search directories, preload lists, failure policies, and diagnostics toggles.

---

## Risks & Mitigations
- **Risk**: Exhausting available multi-sound channels leads to dropped effects. → Mitigation: configurable channel cap, logging, optional LRU stop of least-recent slot.
- **Risk**: Forgetting to call `tick` prevents music streams from updating. → Mitigation: integrate manager tick into the main loop via `WindowManager` or app scheduler, add debug assertions when streams starve.
- **Risk**: Audio device initialization fails on headless/CI environments. → Mitigation: allow "silent mode" fallback; wrap Raylib calls in guards and expose status via diagnostics.
- **Risk**: Reference leaks keep audio assets resident. → Mitigation: diagnostics window surfaces ref counts; tests cover release semantics; optional debug guard to detect non-zero refs on shutdown.
- **Risk**: Concurrent modifications to playback state cause race conditions. → Mitigation: funnel all manager APIs through the main thread (documented requirement) until a thread-safe queue is implemented.

---

## Review & Acceptance Checklist
- [x] Intent captured
- [x] Scenarios authored
- [x] Requirements drafted
- [x] Entities enumerated
- [x] Risks logged
- [x] Clarifications outstanding (device failure handling policy detail)
- [x] Ready for planning once open question addressed

---

## Execution Status
- [x] Intent captured
- [x] Scenarios authored
- [x] Requirements drafted
- [x] Entities enumerated
- [x] Risks documented
- [x] Clarifications resolved
- [x] Ready for implementation plan (pending clarification)
