# Feature Specification: TextureAtlas Support in TextureManager

**Feature Branch**: `013-texture-atlas-support`  
**Created**: 2025-10-11  
**Status**: Draft  
**Input**: "Extend the current texture service to understand JSON+PNG atlases (e.g., toolbaricons.json / toolbaricons.png) while keeping atlas data aligned with texture lifecycle, caching, and metrics."

---

## Execution Flow *(authoring guardrail)*
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
As a GameBuilder2d developer, I can request a texture atlas by logical key and receive both the cached texture and its frame metadata so UI panels and tools can draw individual sprites without duplicating loads or bookkeeping.

### Acceptance Scenarios
1. **Atlas load-on-demand** — Given `toolbaricons.json/png` reside in configured search paths and aren’t cached, when a caller acquires the atlas, the service loads the PNG through `TextureManager`, parses the JSON into frame rectangles, caches both, and returns a handle referencing the shared texture.
2. **Atlas cache hit** — Given an atlas is already cached, when another caller acquires it by the same key or alias, the service returns the existing texture pointer and frame table without re-reading disk.
3. **Frame lookup** — Given an acquired atlas, when the caller asks for `frame("zoom-in.png")`, the service returns the correct source rectangle in pixel coordinates; missing frame names yield a clear error result.
4. **Placeholder fallback** — Given the PNG or JSON is missing/corrupt, when a caller requests the atlas, the service returns the standard placeholder texture with an empty (or flagged) frame table and logs the failure.
5. **Reload resilience** — Given atlases are cached, when `reloadAll` is triggered, the service reloads textures and re-parses metadata, updating callers on success/failure counts.
6. **Force unload** — Given an atlas is cached, when tooling issues a force-unload on its key, both texture and metadata are evicted so the next acquire repeats the full load path.

### Edge Cases & Decisions
- Atlas JSON is assumed to live alongside the PNG (same directory); alternate layouts become a configuration extension.
- Frame rectangles are stored in pixel-space `Rectangle` structs derived from the JSON `frame` fields.
- Duplicate frame names resolve deterministically to the first encountered entry and emit a warning.
- Atlases share the existing reference-count and placeholder policies; releasing the atlas decrements the underlying texture record.
- [NEEDS CLARIFICATION] How to handle trimmed/rotated frames if future atlases include those flags (current sample has none).

---

## Requirements *(mandatory)*

### Functional Requirements
- **FR-101**: The system MUST expose an API (e.g., `TextureManager::acquireAtlas`) that returns an atlas handle containing the cached texture pointer, atlas key, and frame metadata.
- **FR-102**: Atlases MUST reuse `TextureManager`’s texture caching, alias canonicalization, reference tracking, and placeholder behavior.
- **FR-103**: The service MUST parse TexturePacker JSON (frames array + meta) into an in-memory structure accessible by frame name.
- **FR-104**: The service MUST surface a read-only lookup (e.g., `getAtlasFrame(atlasKey, frameName)`) that does not mutate reference counts.
- **FR-105**: On reload or force-unload, the service MUST invalidate and reconstruct both texture and metadata consistently.
- **FR-106**: The service MUST log load success, parse errors, missing frames, and placeholder fallbacks using `LogManager`.
- **FR-107**: Atlas metadata MUST be reflected in diagnostics (e.g., counts of loaded atlases/frames) alongside existing texture metrics.
- **FR-108**: The service MUST fail gracefully when either JSON or PNG is missing, returning placeholder visuals and empty metadata.

### Non-Functional Requirements
- **NFR-101**: Atlas lookup SHOULD be O(1) by frame name using hash maps.
- **NFR-102**: JSON parsing SHOULD leverage an existing lightweight dependency already bundled (if available) or a small bespoke parser that keeps load time minimal.
- **NFR-103**: Memory overhead for metadata SHOULD remain proportional to frame count and avoid duplicating large strings (e.g., store views or interned names).
- **NFR-104**: Design SHOULD permit future support for rotated/trimmed frames and atlas variants without breaking the API.

### Out of Scope (initial release)
- Runtime atlas packing or procedural generation.
- Asynchronous/streaming atlas loads.
- Automatic asset watching to hot-reload atlases on file changes.

---

## Key Entities & Data
- **AtlasKey**: Canonical string key (path or alias) mapping to both texture and metadata.
- **TextureAtlasRecord**: Manager-side record bundling the shared texture key, reference counter, frame map, optional metadata (size, hash), and error state.
- **AtlasFrame**: Structure containing frame rectangle in pixels, original source size, pivot, rotation flags, and trimming info.
- **AtlasHandle**: Returned to callers; wraps atlas key, texture pointer, `std::span`/view of frames, and flags (`newlyLoaded`, `placeholder`).
- **AtlasMetrics**: Extension to existing metrics reporting counts and memory for atlases vs. standalone textures.

---

## Risks & Mitigations
- **Risk**: JSON parsing failures leave callers without sprites. → **Mitigation**: Provide placeholder metadata + distinct log/error channel; add test coverage with malformed JSON.
- **Risk**: Atlas metadata diverges from texture cache state (e.g., texture reloaded but metadata stale). → **Mitigation**: Treat atlas as an extension of the texture record and update both inside the same critical sections.
- **Risk**: Large atlases with thousands of frames may increase memory usage. → **Mitigation**: Store frame names via string interning or references to JSON storage; document guidelines in tooling docs.

---

## Review & Acceptance Checklist
- [x] Intent captured
- [x] Scenarios articulate observable behavior
- [x] Functional/non-functional requirements drafted and testable
- [x] Entities and data modeled
- [x] Risks documented with mitigations
- [ ] [NEEDS CLARIFICATION] items resolved

---

## Execution Status
- [x] Ready for solution design & planning once open question addressed
- [ ] Implementation pending
- [ ] Validation pending
