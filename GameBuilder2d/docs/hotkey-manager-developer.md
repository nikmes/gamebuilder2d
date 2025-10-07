# HotKeyManager Developer Guide

This guide covers the APIs and conventions for wiring new editor functionality into the HotKeyManager service. It complements the user-facing overview in `hotkey-manager.md` by focusing on registration helpers, runtime queries, and persistence hooks that developers need when introducing new commands.

## Service lifecycle

The manager lives in the global services layer. Call `HotKeyManager::initialize()` during application bootstrap after Raylib input has been configured, and `HotKeyManager::shutdown()` before closing the window. Invoke `HotKeyManager::tick()` once per frame after polling Raylib input (`BeginDrawing`) and before dispatching application commands. The tick call updates shortcut state, conflict tracking, and staged triggers.

```cpp
#include "services/hotkey/HotKeyManager.h"

void App::boot() {
    HotKeyManager::initialize();
    HotKeyManager::registerActions(buildDefaultCatalog());
}

void App::frame() {
    HotKeyManager::tick();
    dispatchTriggeredHotkeys();
}

void App::shutdown() {
    HotKeyManager::persistBindings();
    HotKeyManager::shutdown();
}
```

`HotKeyManager::isInitialized()` is available if you need guard checks in subsystem constructors. Prefer registering actions immediately after initialization to ensure the Hotkeys window has the full catalog before any configuration overrides are applied.

## Registering actions

Actions describe both the public UI label and the default binding. Use `HotKeyRegistrationBuilder` from `services/hotkey/HotKeyRegistrationBuilder.h` to define actions fluently. The builder keeps category/context defaults and normalizes shortcut strings via `ShortcutUtils`.

```cpp
#include "services/hotkey/HotKeyRegistrationBuilder.h"
#include "services/hotkey/HotKeyManager.h"

using namespace gb2d::hotkeys;

void registerMyFeatureHotkeys() {
    HotKeyRegistrationBuilder builder;
    builder.withDefaults("My Feature", "Global")
           .addWithDefaults("myFeature.run", "Run Feature", "Ctrl+Alt+R", "Execute the currently selected feature")
           .addWithDefaults("myFeature.togglePanel", "Toggle Panel", "Ctrl+Shift+M", "Show or hide the My Feature dock");

    HotKeyManager::registerActions(std::move(builder).build());
}
```

Guidelines:

- **IDs**: Use reverse-DNS style domains (e.g., `codeEditor.saveFile`). IDs must be stable because they are used for persistence.
- **Categories**: Control the grouping inside the Hotkeys window. Categories should be short ("Global", "Layouts", "Code Editor").
- **Contexts**: Describe where the action is meaningful ("Global", "Game Window"). Use them for tooltips and future context-aware filtering.
- **Defaults**: Provide a canonical shortcut string (`Ctrl+Shift+E`). The builder normalizes tokens and fills the `ShortcutBinding` structure for you.
- **Descriptions**: Optional, but they become the helper text shown in the Hotkeys editor.

You can register structured actions via `HotKeyActionDesc`, raw `HotKeyAction` objects, or by supplying pre-built `ShortcutBinding` values when defaults require non-standard keys (function keys, mouse buttons, etc.). Re-registering an existing action ID replaces the metadata and resets the binding to the provided default unless a custom override exists in configuration.

### Catalog utilities

The initial catalog is built by `gb2d::hotkeys::buildDefaultCatalog()` in `HotKeyCatalog.h`. Call it once during bootstrap to load the core editor bindings. Feature modules should register their custom actions after the default catalog has been installed; the manager keeps registrations additive across modules until `clearRegistrations()` is invoked.

## Consuming hotkeys at runtime

During your frame loop, use one of the query helpers:

- `HotKeyManager::isPressed(id)`: Returns `true` while the shortcut is held down (ignores conflicting bindings).
- `HotKeyManager::consumeTriggered(id)`: Returns `true` exactly once per key press and clears the pending trigger flag.
- `HotKeyManager::consumeTriggeredActions()`: Retrieves every action that fired since the last tick; ideal for centralized command dispatchers.

Example command dispatcher:

```cpp
void CommandBus::dispatchFrameHotkeys() {
    for (const HotKeyAction* action : HotKeyManager::consumeTriggeredActions()) {
        if (action->id == gb2d::hotkeys::actions::OpenFileDialog) {
            openFileDialog();
        } else if (action->id == "myFeature.run") {
            runMyFeature();
        }
    }
}
```

Conflicting bindings are suppressed automatically—the manager never reports a trigger when multiple actions share the same shortcut. Use `HotKeyManager::hasConflicts()` or `HotKeyManager::actionHasConflict(id)` to surface warnings in diagnostics panels.

## Working with configuration

Bindings persist under the `input.hotkeys` array in `config.json`. The manager automatically merges overrides during initialization by reading from `ConfigurationManager`. Runtime changes made via the Hotkeys editor or through `setBinding/clearBinding/restoreDefaultBinding` can be serialized by calling `HotKeyManager::persistBindings()`. For custom flows (e.g., configuration export), use `HotKeyManager::exportBindingsJson()` to retrieve the canonical JSON payload.

When adding new actions:

1. Provide sensible defaults so existing users inherit functional shortcuts.
2. Keep `id` stable—changing it invalidates user overrides.
3. If you rename or remove actions, supply migration code that maps legacy IDs before registering new ones (see `ConfigurationManager` migration helpers).

## Suppression and modal contexts

Text input fields and modal dialogs should prevent global hotkeys from firing. Wrap those scopes in `ScopedHotKeySuppression` or push/pop reasons manually.

```cpp
void TextEditor::beginCapture() {
    suppression_ = std::make_optional<ScopedHotKeySuppression>(HotKeySuppressionReason::TextInput);
}

void TextEditor::endCapture() {
    if (suppression_) {
        suppression_->release();
        suppression_.reset();
    }
}
```

Reasons include `TextInput`, `ModalDialog`, and `ExplicitPause`. Multiple suppressions can coexist; the manager resumes normal behavior once all counts reach zero. For transient popups, prefer the RAII helper to avoid mismatched push/pop pairs.

## Validating custom shortcuts

Utility functions in `ShortcutUtils.h` help with tests and custom UI flows:

- `parseShortcut("Ctrl+Shift+P")` → `ShortcutBinding`
- `buildShortcut(KEY_F5, kModifierCtrl)` for programmatic defaults
- `equalsShortcut(a, b)` and `hashShortcut(a)` for container integration

Use them in unit tests (T022/T023) to assert normalization rules—especially when supporting platform-specific modifiers.

## Quick checklist ✅

1. Initialize the manager and register the default catalog at startup.
2. Register feature-specific actions via `HotKeyRegistrationBuilder` right after initialization.
3. Poll `tick()` every frame, then consume triggers through `consumeTriggered*` helpers.
4. Persist changes with `persistBindings()` when users save preferences.
5. Wrap text/modal scopes with `ScopedHotKeySuppression` to avoid accidental shortcuts.
6. Document new action IDs and defaults so QA can cover them in T024.
