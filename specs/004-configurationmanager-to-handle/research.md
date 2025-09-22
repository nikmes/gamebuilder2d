# Research: ConfigurationManager

- JSON vs alternatives
  - Decision: JSON
  - Rationale: human-readable, tooling support, simple mapping to supported types
  - Alternatives: INI (limited types), TOML/YAML (heavier)

- Per-user paths
  - Decision: Windows %APPDATA%/GameBuilder2d/config.json; Linux $XDG_CONFIG_HOME or ~/.config; macOS ~/Library/Application Support
  - Rationale: OS conventions, avoids elevated permissions

- Atomic writes
  - Decision: write to temp file in same directory, then replace
  - Rationale: reduces risk of corruption on crash

- Versioning/migrations
  - Decision: include `version` field, forward migrations with .bak backup
  - Rationale: safe upgrades; preserve original

- Environment overrides
  - Decision: `GB2D_` prefix; double underscore maps to dot segments
  - Rationale: simple opt-in overrides without CLI parsing
