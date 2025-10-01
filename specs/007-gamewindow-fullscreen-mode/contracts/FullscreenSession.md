# Contract: FullscreenSession Controller

## Purpose
Own fullscreen game playback lifecycle, bridging `GameWindow` and the main loop while avoiding tight coupling between UI and runtime.

## Responsibilities
- Track whether fullscreen mode is active and which game is being displayed.
- Handle entering/exiting fullscreen, including window flag transitions and dimension changes.
- Update and render the active game directly when fullscreen is active.
- Notify interested parties (`GameWindow`, `WindowManager`) when mode changes occur.

## Interface Sketch
```cpp
class FullscreenSession {
public:
    struct Callbacks {
        std::function<void()> onEnter;
        std::function<void()> onExit;
        std::function<void()> requestGameTextureReset; // informs GameWindow to rebuild render target
    };

    explicit FullscreenSession(Callbacks cb);

    bool isActive() const;
    void requestStart(games::Game& game, const std::string& gameId, int width, int height);
    void requestStop();

    void tick(float dt);  // call from main loop when active
};
```

## Invariants
- `tick` is only called when `isActive() == true`.
- `requestStart` no-ops if already active (or transitions to new game after cleanup).
- Restores original window state before invoking `onExit` callback.

## Error Handling
- If the Raylib window fails to enter fullscreen, session aborts and reports via logging/toast.
- Null game pointers are rejected (assert/log error).
