# Feature Specification: TextureManager Service for GameBuilder2d

**Feature Branch**: `008-texture-manager-service`

**Created**: 2025-10-01  
**Status**: Draft  
**Input**: "Introduce a TextureManager, similar to existing managers, that loads, caches, and returns Raylib textures by logical name or filename. The manager should own load/unload lifecycle, avoid duplicate loads, expose retrieval APIs, and integrate cleanly with the engine's service pattern."

---

## Execution Flow (authoring guardrail)
```
1. Capture the user intent and articulate desired outcomes
2. Identify actors, actions, data, constraints, and success criteria
3. Surface ambiguities as [NEEDS CLARIFICATION: question]
4. Describe user scenarios and acceptance tests focused on observable behavior
5. List functional requirements; add non-functional where relevant
6. Model key entities / data that must exist to support the feature
7. Document edge cases, risks, and assumptions
8. Run self-check: no implementation-level prescriptions, requirements are testable
```

---

## User Scenarios & Testing *(mandatory)*

### Primary User Story
As a GameBuilder2d developer, I can request a texture by logical key and receive a ready-to-use Raylib texture, allowing gameplay code and editor panels to share cached GPU resources without manual load/unload bookkeeping.

### Acceptance Scenarios
1. **Load on demand** — Given a texture file exists under configured search paths and is not yet cached, when a caller requests it by key, then the TextureManager loads the file, caches it, and returns a valid texture reference.
2. **Cache hit** — Given a texture has already been loaded, when any caller requests it again by the same canonical key, then the manager returns the cached texture without reloading from disk.
3. **Reference lifecycle** — Given multiple callers have requested the same texture, when each caller releases its reference, then the manager decrements the reference count and unloads the texture only after the final release completes.
4. **Forced unload** — Given a texture is cached, when tooling or debugging code issues a force-unload, then the manager removes it immediately and subsequent requests reload from disk.
5. **Missing asset fallback** — Given a requested file cannot be found or decoded, when the caller requests it, then the manager returns a clearly identifiable "missing texture" placeholder and logs the failure.
6. **Global reset** — Given several textures are cached, when the application triggers a reload-all (e.g., device lost or developer command), then the manager attempts to unload and reload each cached texture, preserving keys and notifying of any failures.

### Edge Cases & Decisions
- Missing textures will return a generated 64×64 magenta/black checkerboard placeholder and log the failure.
- Requests are normalized to lowercase absolute paths resolved against configured search roots; callers may supply explicit aliases when needed.
- Over-release clamps the reference count at zero, leaves the texture loaded, and emits a warning in the logs.
- Initial implementation uses synchronous on-demand loading; future async support remains a backlog item.
- Memory budgeting logs a warning when an optional configurable `maxBytes` threshold is exceeded but does not auto-evict.

---

## Requirements *(mandatory)*

### Functional Requirements
- **FR-001**: The system MUST expose a globally accessible TextureManager service consistent with existing manager registration patterns (Logger, WindowManager).
- **FR-002**: The manager MUST load textures on first request using configured search paths and cache them for reuse.
- **FR-003**: The manager MUST deduplicate loads by canonical key (filename or alias), returning the same cached texture for repeated requests.
- **FR-004**: The manager MUST track active references and only unload textures automatically when their reference count reaches zero.
- **FR-005**: The manager MUST provide an explicit method to force-unload a texture, bypassing reference counts for tooling workflows.
- **FR-006**: The manager MUST provide a read-only retrieval API that returns a pointer/handle without altering reference counts, to support draw-time lookups.
- **FR-007**: The manager MUST surface an API to reload all cached textures (e.g., `reloadAll`) and report any failures via the logging system.
- **FR-008**: The manager MUST log successful loads, unloads, and error conditions using the central logging facility.
- **FR-009**: On load failure, the manager MUST return a deterministic placeholder texture and keep the cache entry valid for subsequent requests.
- **FR-010**: The manager MUST allow configuration of default load parameters (e.g., filtering, mipmaps) via configuration settings.

### Non-Functional Requirements
- **NFR-001**: Cached texture retrieval SHOULD incur O(1) lookup time given reasonable hash map load factors.
- **NFR-002**: Initial implementation MAY operate on the main thread; the design SHOULD be extensible to background loading.
- **NFR-003**: The manager SHOULD expose metrics (count of loaded textures, total VRAM) for diagnostic overlays.

### Out of Scope (initial release)
- Automatic file watching or hot reload triggered by filesystem events.
- Streaming or partial loading of oversized textures.
- Cross-platform GPU residency or compression pipeline changes.

---

## Key Entities & Data
- **TextureKey**: Normalized identifier (canonical path or user-specified alias) used for cache lookups.
- **TextureRecord**: Internal structure containing the Raylib texture handle, originating path, reference count, load parameters, timestamps, and error state.
- **TextureManager Service**: Singleton-style façade offering init/shutdown, load/acquire, try-get, release, force-unload, reload-all, metrics, and configuration hooks.
- **Configuration Settings**: Optional structured data providing search directories, default sampler/filter options, placeholder asset path, and logging verbosity.

---

## Risks & Mitigations
- **Risk**: Excessive synchronous loads could stall the main thread. → Mitigation: encourage preloading or future async extension; document performance guidance.
- **Risk**: Reference leaks keep textures in memory indefinitely. → Mitigation: diagnostics API exposing ref counts; optional guard rails in debug builds.
- **Risk**: Misconfigured search paths lead to silent failures. → Mitigation: warning logs plus placeholder textures for visibility.

---

## Review & Acceptance Checklist
- [x] All [NEEDS CLARIFICATION] items resolved or tracked
- [x] Requirements are testable
- [x] Success criteria measurable via scenarios
- [x] Scope boundaries identified
- [x] No implementation-specific API signatures mandated

---

## Execution Status
- [x] Intent captured
- [x] Scenarios authored
- [x] Requirements drafted
- [x] Entities enumerated
- [ ] Clarifications outstanding
- [x] Ready for planning once open questions addressed
