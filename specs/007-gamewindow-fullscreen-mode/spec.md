# Spec: GameWindow Fullscreen Playback Mode

## Summary
Add a fullscreen playback mode that allows any embedded game to temporarily take over the Raylib backbuffer, hide the editor UI, and run at the monitor resolution. The mode can be entered from the `GameWindow` and exited with a dedicated shortcut (Ctrl+W), restoring the previous ImGui layout and window state.

## Non-Goals
- Replacing the ImGui-based editor shell with a stand-alone launcher.
- Implementing per-game bespoke fullscreen overlays or HUDs.
- Hot-reloading game binaries or assets while in fullscreen.

## Requirements
1. Provide a UI affordance in `GameWindow` to enter fullscreen for the currently selected game.
2. While fullscreen is active:
   - Skip rendering the ImGui UI.
   - Resize the game viewport to the desktop resolution (or active monitor resolution).
   - Forward raw input directly to the game (no ImGui focus gating).
3. Support exiting fullscreen with `Ctrl+W` (and Esc as backup) to restore the editor layout.
4. Preserve and restore window size/flags when toggling fullscreen.
5. Persist the "resume in fullscreen" preference using `ConfigurationManager` (optional default false).
6. Ensure the feature works across all bundled games without code duplication.
7. Maintain 60 FPS target and VSYNC behaviour consistent with pre-existing flags.

## Functional Details

### Mode Controller
- Introduce a `FullscreenSession` controller responsible for tracking:
  - `active` flag.
  - Pointer to the active `games::Game` instance.
  - Cached window dimensions and monitor index.
- Expose interface:
  - `bool isActive() const;`
  - `void requestStart(GameWindow& source);`
  - `void requestStop();`
  - `void tick(float dt);` (called from main loop when active)

### GameWindow Integration
- Add a toolbar button (icon or text "Fullscreen") and optional menu command.
- When clicked:
  - Call `FullscreenSession::requestStart`, passing pointers to the current `Game` and dimension data.
  - Hide the regular render texture overlay; the session will call `Game::render` directly.
- When fullscreen is inactive, existing behaviour remains unchanged.

### WindowManager / Main Loop Changes
- `main()` keeps owning the Raylib frame; add branch:
  ```cpp
  if (fullscreenSession.isActive()) {
      fullscreenSession.tick(dt);
  } else {
      rlImGuiBegin();
      wm.renderUI();
      rlImGuiEnd();
  }
  ```
- `FullscreenSession::tick` will:
  1. Ensure the Raylib window is in fullscreen (`ToggleFullscreen` if needed) and sized to monitor.
  2. Update the selected game with `acceptInput = true`.
  3. Clear the backbuffer and call `game->render(width, height)`.
  4. Poll exit hotkeys (Ctrl+W, Esc). If triggered, stop the session.

### Lifecycle & Resizing
- On start: store `GetScreenWidth/Height`, `IsWindowState(FLAG_FULLSCREEN_MODE)`, and whether ImGui render loop was active.
- Call `game->onResize` and `game->reset` if target size changes.
- On stop: restore window size/flags using `ClearWindowState/SetWindowSize`, reinitialize render target in `GameWindow`, and mark `game_needs_init_` so the off-screen texture is rebuilt.

### Configuration
- Key: `window::resume_fullscreen` (bool). If true, the app re-enters fullscreen automatically for the last game on launch.
- Key: `window::fullscreen_last_game` (string). Optional, ID of game to auto-open.

### UX Outline
- Toolbar controls inside `GameWindow`:
  - `[Fullscreen]` button
  - Shortcut hint text ("Ctrl+W to exit") when active.
- While fullscreen: display a small overlay with instructions (bottom-left) for 2 seconds.

## Risks & Mitigations
- **ImGui context state drift**: ensure `rlImGuiEnd` is not called when `rlImGuiBegin` hasn't run. Solution: branch early in main loop.
- **Window decorations flicker**: Raylib toggling may momentarily resize; store previous size to restore cleanly.
- **Game assuming off-screen framebuffer**: verify all bundled games operate correctly when drawing straight to backbuffer. Add optional compatibility flag to fall back to texture if needed.
- **Keyboard shortcut conflicts**: Document and allow rebind later; use ImGui input capture to suppress toggling while typing (only when editor visible).

## Acceptance Criteria
- Clicking "Fullscreen" swaps to fullscreen display of the current game without editor chrome.
- While fullscreen, UI hotkeys (Ctrl+W/Esc) exit the mode and restore the exact prior layout.
- All bundled games render correctly at full resolution.
- Build passes with no new warnings, and configuration persistence toggles resume behaviour.
