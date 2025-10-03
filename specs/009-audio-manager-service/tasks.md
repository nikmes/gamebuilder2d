# Task Breakdown — AudioManager Service

## Clarifications & Preparations
- [x] Document silent-mode behavior when audio device init fails *(see spec §Clarification Outcomes)*
- [x] Finalize configuration schema updates (keys, defaults, migration notes)
- [x] Select placeholder handling strategy for missing audio assets *(silent no-op handles + warnings)*

## Engineering
- [x] Scaffold `AudioManager` interface and implementation files
- [x] Add audio service bootstrap wiring to application startup/shutdown
- [x] Implement sound cache with alias normalization and reference counting
- [x] Implement music registry with streamed playback management
- [x] Implement `tick` loop (music updates, cleanup of finished handles)
- [x] Implement multi-channel sound playback with slot tracking and throttling
- [x] Implement release, force-stop, and `reloadAll` behaviors
- [x] Expose metrics and diagnostics data structures

## Integration
- [x] Wire AudioManager usage into editor/gameplay subsystems that need audio
- [ ] Add (or update) editor UI for previewing audio assets using the manager
- [x] Update `config.json` and documentation with audio configuration options

## Quality Assurance
- [ ] Unit tests (load, cache hit, reference release, channel limit enforcement, silent fallback)
- [ ] Integration test/smoke harness covering sound + music playback together
- [ ] Manual QA checklist for device-loss scenarios and diagnostics output

## Documentation & Handoff
- [ ] Draft developer guide for AudioManager usage patterns
- [x] Update README / changelog with feature overview
- [ ] Capture backlog items (async streaming improvements, DSP effects, hot reload)
