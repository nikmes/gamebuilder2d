# Feature Specification: ConfigurationManager

**Feature Branch**: `004-configurationmanager-to-handle`  
**Created**: 2025-09-21  
**Status**: Draft  
**Input**: User description: "ConfigurationManager to handle globaly our configuration and persist it."

## User Scenarios & Testing

### Primary User Story
As a developer, I want a `ConfigurationManager` that provides a single place to read and write application configuration so that settings are consistent across runs and easy to manage.

### Acceptance Scenarios
1. Given no prior config exists, When the app calls `ConfigurationManager::loadOrDefault()`, Then defaults are loaded into memory and saved to the per-user config location on first successful save.
2. Given a valid config file exists, When `ConfigurationManager::load()` is called, Then current settings reflect the file’s values.
3. Given changes are applied via `ConfigurationManager::set(key, value)`, When `ConfigurationManager::save()` is called, Then values persist and are reloaded on next start.
4. Given invalid or corrupted config on disk, When `load()` is called, Then the system falls back to defaults, preserves the original file as a “.bak” backup, and reports a recoverable warning.
5. Given concurrent access, When multiple threads read and occasional writes occur, Then reads are thread-safe and consistent, writes are serialized, and subscribers are notified after a successful save.

### Edge Cases
- Missing write permissions at target path: configuration remains in-memory; save returns an error result and a user-visible warning is emitted; the app continues to operate with in-memory settings.
- Partial writes (e.g., crash during save): saves are atomic (write to a temp file and replace) to avoid corruption. On failure, the original file remains intact and an error is returned.
- Versioning/migrations: the file contains a `version` field. Older versions are auto-migrated forward with a `.bak` backup created. Unknown newer major versions trigger fallback to defaults with a warning while preserving the file.
- Large or unsupported data: configuration files larger than 1 MB are rejected with a warning; binary blobs or unsupported types are not allowed.

## Requirements

### Functional Requirements
- FR-001: System MUST provide a central `ConfigurationManager` to read/write application settings.
- FR-002: System MUST support default configuration when no persisted state exists.
- FR-003: System MUST load configuration from a persistent store at startup and allow save during runtime.
- FR-004: System MUST expose typed accessors for boolean, integer (64-bit), floating-point (double), string, and list-of-strings; other types are rejected with a validation error.
- FR-005: System MUST validate configuration and safely handle invalid/corrupt files (fallback to defaults + warning, original preserved as `.bak`).
- FR-006: System MUST persist configuration as JSON in the per-user config directory:
	- Windows: `%APPDATA%/GameBuilder2d/config.json`
	- Linux: `$XDG_CONFIG_HOME/GameBuilder2d/config.json` (fallback `~/.config/GameBuilder2d/config.json`)
	- macOS: `~/Library/Application Support/GameBuilder2d/config.json`
- FR-007: System MUST perform atomic saves (write temp file and replace) to avoid corruption.
- FR-008: System MUST provide change notification callbacks that fire after successful save; callbacks run on the caller’s thread and must return quickly.
- FR-009: System MUST support environment variable overrides at load time using the `GB2D_` prefix (e.g., `GB2D_WINDOW__WIDTH=1280` for key `window.width`); command-line overrides are out-of-scope for this release.
- FR-010: System MUST include a `version` field and support forward migrations for older files; on migration, create a `.bak` of the original and write the updated file.

### Key Entities
- ConfigurationManager: facade owning in-memory settings, load/save, and default provisioning.
- Configuration Store: persistent representation (file on disk, path rules, format, and versioning policy).

## Review & Acceptance Checklist

### Content Quality
- [x] No implementation details (languages, frameworks, APIs)
- [x] Focused on user value and business needs
- [x] Written for non-technical stakeholders
- [x] All mandatory sections completed

### Requirement Completeness
- [x] No [NEEDS CLARIFICATION] markers remain
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Scope is clearly bounded
- [x] Dependencies and assumptions identified

## Execution Status

- [x] User description parsed
- [x] Key concepts extracted
- [x] Ambiguities marked
- [x] User scenarios defined
- [x] Requirements generated
- [x] Entities identified
- [x] Review checklist passed
