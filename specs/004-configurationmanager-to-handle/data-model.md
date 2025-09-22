# Data Model: ConfigurationManager

## Entities

- Configuration
  - Fields
    - version: integer (>=1)
    - settings: object (key/value)
      - Supported value types: bool, int64, double, string, list<string>

## Validation Rules
- File size <= 1 MB
- Keys are dot-delimited segments `[a-zA-Z0-9_]+(\.[a-zA-Z0-9_]+)*`
- Values must be of supported types

## Defaults (examples)
- window.width: 1280
- window.height: 720
- ui.theme: "dark"
- logging.level: "info"
