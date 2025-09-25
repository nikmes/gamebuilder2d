# Interop Status Codes (Draft)

| Code | Name | Description |
|------|------|-------------|
| 0 | OK | Operation succeeded |
| 1 | NOT_INITIALIZED | Runtime or engine not initialized yet |
| 2 | INVALID_ID | Supplied window/script id invalid or expired |
| 3 | CONTEXT_UNLOADING | Script context is unloading or disposed |
| 4 | ALREADY_LOADED | Attempted duplicate load without reload flag |
| 5 | METHOD_NOT_FOUND | Target method string resolved to no delegate |
| 6 | RUNTIME_ERROR | Managed code threw an exception |
| 7 | INTERNAL_ERROR | Unexpected native failure (logged) |
| 8 | SHUTDOWN | Engine/script subsystem shutting down |
| 9 | BAD_FORMAT | Malformed target string or payload |

Reserved Future Range: 100-149 (extended subsystems), 200-219 (transport extensions).

## Rules
- Codes stable once released (only append new)
- Names UPPER_SNAKE_CASE
- Scripts must not rely on numeric gaps
