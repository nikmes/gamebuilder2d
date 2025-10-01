# Task List: GameWindow Fullscreen Mode

## Must Have
- [x] Implement `FullscreenSession` controller (state machine, window restore logic).
		- [x] Scaffolding created (controller stub, callbacks, tracked state).
		- [x] Implement enter/exit lifecycle hooks and ticking logic.
- [x] Add fullscreen toggle button to `GameWindow` UI and wire to session.
	- [x] Add UI button and request flag in `GameWindow`.
	- [x] Connect button to `FullscreenSession` via manager integration.
- [x] Branch main loop to defer ImGui rendering when fullscreen is active.
- [x] Forward update/render calls directly to active game while fullscreen.
- [x] Handle exit shortcuts (Ctrl+W, Esc) and cleanup.
- [x] Persist configuration values for auto-resume.
- [x] Update documentation (README and in-app tooltip).

## Nice to Have
- [ ] Provide on-screen overlay instructions when entering fullscreen.
- [ ] Remember monitor selection in config.
- [ ] Add automated test covering configuration round-trip.
