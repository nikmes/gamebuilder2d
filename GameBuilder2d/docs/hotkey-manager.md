# HotKeyManager & Customizable Shortcuts

The HotKeyManager centralizes every editor shortcut in GameBuilder2d. It coordinates default bindings, lets you record new combinations in-app, persists overrides to `config.json`, and ensures shortcuts respect text input and modal dialogs.

## Opening the Hotkeys editor

You can reach the Hotkeys window from **Window → Hotkeys** or by pressing the default shortcut `Ctrl+Alt+K`. The window lists every registered action grouped by category (Global, Code Editor, Game Window, etc.).

Each row shows:

- **Action** – the human-friendly label, with the internal action ID visible in the tooltip.
- **Shortcut** – the currently staged binding. Click **Edit** to capture a new shortcut with the ImHotKey widget.
- **Status** – badges for *Default*, *Custom*, *Conflicts*, or *Unassigned* states.
- Optional description/context text to help you understand where the shortcut applies.

Use the toolbar buttons to:

- **Restore All Defaults** – revert every action to its built-in binding.
- **Clear All Bindings** – set every shortcut to *Unassigned* (the actions become inactive until you assign them again).
- **Apply** – push staged bindings into the running editor immediately.
- **Save** – persist staged bindings to `config.json`.

> Apply is disabled whenever conflicts or invalid captures exist. Resolve the highlighted rows before applying or saving.

## Recording shortcuts with ImHotKey

When you click **Edit**, a capture popup appears:

1. Press the desired key combination. Hold modifiers (Ctrl, Shift, Alt, Super/Cmd) then tap the trigger key.
2. Release everything to preview the normalized label (e.g., `Ctrl+Shift+E`).
3. Click **Confirm** to stage the change or **Cancel** to discard it.

The capture dialog rejects ambiguous combinations (like modifiers without a trigger key). If that happens, the row is flagged with an error badge and the previous binding remains intact until you set a valid shortcut.

## Conflict detection & suppression

- The manager blocks Apply/Save while two actions share the same shortcut. Conflicting rows show a red badge listing the other action(s) using the binding.
- Text fields, modal dialogs, and ImGui popups automatically suppress global shortcuts. You can safely type in the code editor or Hotkeys window without triggering commands unexpectedly.

## Persistence format (`config.json`)

Customized bindings are stored under the `input.hotkeys` array. Each entry contains an `action` ID and a `shortcut` string using the canonical label returned by the manager.

```jsonc
"input": {
  "hotkeys": [
    { "action": "global.openFileDialog", "shortcut": "Ctrl+O" },
    { "action": "global.openHotkeySettings", "shortcut": "Ctrl+Alt+K" },
    { "action": "codeEditor.saveFile", "shortcut": "Cmd+S" } // macOS example
  ]
}
```

Tips:

- Set `"shortcut": null` (or an empty string) to disable an action until you reassign it.
- Unknown actions are ignored during load with a warning; they stay in the file so you can migrate or delete them later.
- When you restore defaults inside the UI, the file is rewritten with the original bindings the next time you hit **Save**.

## Resetting & exporting

- **Restore Defaults** (per-row or global) pulls from the built-in catalog and clears the *Custom* badge.
- **Export** your current configuration by copying the `input.hotkeys` block to another machine. The manager will merge it with defaults at startup.

## Adding new actions (for developers)

Developers can register additional shortcuts by using the helper in `HotKeyRegistrationBuilder.h`:

```cpp
using namespace gb2d::hotkeys;

HotKeyRegistrationBuilder builder;
builder.withDefaults("My Feature", "Global")
       .addWithDefaults("feature.doThing", "Run Thing", "Ctrl+D");
HotKeyManager::registerActions(std::move(builder).build());
```

Every registered action automatically appears in the Hotkeys window and persists through the same configuration pipeline described above.
