# Quickstart: GameWindow Fullscreen Mode

1. **Build the app** using the existing preset:
   ```pwsh
   cmake --build --preset windows-vs2022-x64-debug
   ```
2. **Launch GameBuilder2d** and open the `Game Window` if it is not already visible.
3. Select any built-in game (e.g., Space Invaders or Plarformer).
4. Click the new **Fullscreen** button in the Game window toolbar.
5. Verify that:
   - The editor UI disappears.
   - The game renders across the entire screen at monitor resolution.
   - Input (keyboard, mouse) continues to work.
6. Press **Ctrl+W** (or Esc) to exit fullscreen and confirm the previous layout is restored.
7. Toggle the configuration flag `window::resume_fullscreen` (via config file or settings UI when available) to test auto-resume on restart.
