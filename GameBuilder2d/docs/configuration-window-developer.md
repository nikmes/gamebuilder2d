# Configuration window — developer notes

This guide describes how to evolve the configuration schema and keep the Configuration window in sync. Read this after finishing the user-facing overview in [`configuration-manager.md`](configuration-manager.md).

## Touchpoints at a glance

- Schema source of truth: `services/configuration/ConfigurationManager.cpp` (`buildConfigurationSchema()`)
- Runtime state & validation: `services/configuration/ConfigurationEditorState.*`
- UI rendering: `ui/Windows/ConfigurationWindow.*`
- Tests: `tests/unit/config`, `tests/integration/test_config_window_ui.cpp`

Keep these layers aligned—schema metadata drives the editor state, which drives the UI. Avoid bypassing `ConfigurationManager` when mutating config data.

## Extending the configuration schema

1. **Update defaults** — Add the new field (or section) to the default document assembled in `ConfigurationManager::loadOrDefault()` so fresh installs pick it up.
2. **Describe it in the schema** — Use `ConfigurationSchemaBuilder` inside `buildConfigurationSchema()` (also in `ConfigurationManager.cpp`) to define the field. Provide:
   - `label` / `description` for the UI
   - `type` (`Boolean`, `Integer`, `Float`, `Enum`, `String`, `Path`, `List`, `JsonBlob`, `Hotkeys`)
   - Validation metadata (`min`, `max`, `enumValues`, etc.) and UI hints (`placeholder`, `enumLabels`, `fileFilters`, …)
   - Flags (`advanced`, `experimental`, `hidden`) when appropriate
3. **Wire runtime behavior** — If the new field affects a subsystem, subscribe via `ConfigurationManager::subscribeOnChange` or extend the logic in `ConfigurationManager::applyRuntime()` / relevant hooks so the runtime reacts when Apply/Save runs.
4. **Cover tests** — Add assertions to the relevant unit test (e.g., `tests/unit/config/test_apply_save_runtime.cpp`) or integration flow to guarantee the new field appears with defaults, validates correctly, and persists across Apply/Save.
5. **Document it** — Update user docs (`README.md`, `configuration-manager.md`) so end-users understand the field.

> 💡 The schema and default JSON must stay in lock-step to prevent "unknown field" warnings in the UI.

## Adding a bespoke field editor

1. **Decide on reuse** — Does an existing editor handle the field type? If so, enrich the schema with `uiHints` instead of writing new code (e.g., `step`, `precision`, `enumLabels`).
2. **Augment rendering** — When a new widget is required:
   - Update the switch in `ConfigurationWindow::renderFieldEditor` (or the helper it calls) to handle the new `ConfigFieldType`.
   - Add any helper functions alongside existing ones (search for `render*Field` helpers in `ConfigurationWindow.cpp`).
   - Respect `ConfigFieldState` APIs (`value()`, `setValue(...)`, `validation()`), so dirtiness and validation propagate correctly.
3. **Editor state support** — Extend `ConfigurationEditorState::applyFieldEdit` or related helpers if the new widget needs bespoke bookkeeping (e.g., hotkey tables, list editing).
4. **Validation** — Keep validation centralized in `services/configuration/validate.*`. The UI should request validation via `ConfigurationEditorState` instead of duplicating rules.
5. **Tests** —
   - Add unit coverage to ensure the field serializes/deserializes correctly.
   - If the widget affects apply/save flows, enhance `test_config_window_ui.cpp` (or add a new scenario) to drive it end-to-end.

## Handling advanced & experimental flags

- Mark sections/fields with `.advanced()` or `.experimental()` in the schema builders.
- The window automatically hides these unless users toggle **Show advanced settings** or **Show experimental settings**.
- Ensure descriptions explain why a field is advanced and what side-effects to expect.

## Supporting JSON fallback editors

Fields that cannot be expressed through the existing widgets can be flagged with `ConfigFieldType::JsonBlob`. The window will expose a text editor with validation against `nlohmann::json`. Consider this a stopgap until a bespoke UI exists.

## Checklist before opening a PR

- [ ] Defaults and schema updated together
- [ ] Apply/Save hooks reflect new behavior (if needed)
- [ ] Unit + integration tests added or updated
- [ ] User docs refreshed
- [ ] QA checklist re-run when new sections/flags are introduced

Keeping these steps consistent ensures the Configuration window remains declarative, testable, and pleasant to use.
