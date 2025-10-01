# Plan: GameWindow Fullscreen Playback Mode

## Phase 0 – Discovery & Spike
- Verify Raylib fullscreen API behaviour across monitors (`ToggleFullscreen`, `SetWindowState`).
- Confirm each game behaves correctly when rendering straight to the backbuffer at desktop resolution.

## Phase 1 – Infrastructure
- Implement `FullscreenSession` controller (header + source under `src/ui` or `src/runtime`).
- Extend `GameWindow` to issue start/stop requests and expose active game pointer + render texture reset hook.
- Add configuration keys (`window::resume_fullscreen`, `window::fullscreen_last_game`).

## Phase 2 – Main Loop Integration
- Update `GameBuilder2d.cpp` main loop to branch between editor mode and fullscreen session.
- Ensure input polling works in both paths and guard ImGui calls.
- Persist window size and restore on exit.

## Phase 3 – UX & Polish
- Add toolbar button + stateful tooltip / status hint in `GameWindow`.
- Display temporary overlay instructions while in fullscreen.
- Optionally add menu command and shortcut listing.

## Phase 4 – Testing & Validation
- Manual test matrix:
  - Enter/exit fullscreen using button and shortcuts.
  - Switch games before/after fullscreen.
  - Resize window, then toggle fullscreen and back.
- Add unit/integration tests if feasible (e.g., configuration persistence round-trip).
- Update README/release notes.

## Phase 5 – Documentation & Handoff
- Document usage in `docs/` (likely `README.md` or dedicated guide).
- Record known limitations (multi-monitor behaviour, vsync requirements).
