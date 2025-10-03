# Implementation Plan — AudioManager Service

## Phase 0 · Alignment & Clarifications *(Complete · see spec §Clarification Outcomes)*
- [x] Confirm audio device initialization fallback policy (silent mode vs. hard error)
- [x] Define default configuration keys (master volume, channel caps, search paths, preload list)
- [x] Decide on placeholder behavior for missing audio (silent handle vs. synthetic beep)

## Phase 1 · Service Scaffolding
- [x] Declare public AudioManager API (initialize, shutdown, tick, acquireSound, releaseSound, acquireMusic, releaseMusic, playSound, playMusic, stop, pause, resume, reloadAll, metrics)
- [x] Introduce `AudioConfig` struct with sensible defaults
- [x] Register AudioManager within the same bootstrap pathway as existing services

## Phase 2 · Resource Management Core
- [x] Implement sound cache map with canonical alias normalization and reference counting
- [x] Implement music registry handling streamed assets and playback state
- [x] Implement tick loop managing `UpdateMusicStream` and cleaning completed playback handles
- [x] Implement release semantics that unload assets when reference counts reach zero

## Phase 3 · Playback & Concurrency Controls
- Contract outline:
	- `playSound(key, params)` → returns `PlaybackHandle{slot,generation}` when a non-placeholder asset is available and slots remain; returns invalid handle otherwise.
	- `stopSound(handle)` → succeeds only if the handle generation matches the active slot, ensuring expired handles are ignored.
	- `stopAllSounds()` → stops every active slot and resets counters; safe in silent mode.
	- Music control APIs (`playMusic`, `pauseMusic`, `resumeMusic`, `stopMusic`, `seekMusic`, `setMusicVolume`) operate on canonical keys, no-op when assets are placeholders or manager is silent.
	- Edge cases: silent mode forces all playback requests to succeed with no audible output; missing keys log warnings and return failure/invalid handles; channel exhaustion logs throttling and drops the newest request; reloading assets invalidates active handles.
- [x] Implement multi-channel sound playback using Raylib sound aliases, tracking active handles and enforcing configurable limits
- [x] Provide playback control APIs for music (play/pause/resume/seek/loop)
- [x] Add per-channel and master volume control, propagating changes to active audio
- [x] Support `reloadAll` to stop playback, reload assets, and report errors

## Phase 4 · Resilience & Diagnostics
- [x] Implement failure handling for missing/corrupt assets with silent placeholder handles and logging
- [x] Add metrics collection (loaded sounds, active channels, memory usage, device status)
- [ ] Expose diagnostics hooks (structured data model for UI overlays, logging summaries)
- [x] Ensure silent mode operation when audio device initialization fails

## Phase 5 · Integration & Tooling
- [ ] Wire AudioManager init/shutdown into application lifecycle alongside other managers
- [ ] Provide editor tooling support (e.g., Audio Preview window leveraging the manager)
- [ ] Update configuration files and documentation to describe new audio settings
- [ ] Create developer guide for requesting playback, managing handles, and using diagnostics

## Phase 6 · Testing & Validation
- [ ] Unit tests for cache hits, reference counting, channel limit enforcement, reloadAll behavior, and silent fallback
- [ ] Integration test or smoke harness exercising simultaneous sound playback and music streaming
- [ ] Manual QA checklist covering device failure, volume adjustments, and diagnostics output

## Phase 7 · Wrap-up & Future Work
- [ ] Document limitations (no 3D audio, no hot-reload, main-thread requirement)
- [ ] Capture backlog items (async loading, priority-based channel management, DSP effects)
- [ ] Final review, retrospective, and merge readiness checklist
