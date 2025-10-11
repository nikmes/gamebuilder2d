# Implementation Plan — TextureAtlas Support in TextureManager

## Phase 0 · Alignment & Clarifications *(In Progress)*
- [ ] Resolve handling for trimmed/rotated frames in TexturePacker exports (support now vs. backlog)
- [ ] Confirm JSON parser dependency (reuse existing vs. introduce new lightweight library)
- [ ] Align naming conventions for atlas identifiers and frame keys across editor/gameplay code

## Phase 1 · API & Data Model Design
- Outline:
	- Define `TextureAtlasHandle` data contract (texture pointer, atlas key, frame lookup view)
	- Extend `TextureManager` headers with `acquireAtlas`, `releaseAtlas`, and `getAtlasFrame` entry points
	- Update metrics structures to report atlas-specific counts and memory usage
- [ ] Draft header changes and circulate for review
- [ ] Document expected lifetime rules in developer guide additions

## Phase 2 · Loader & Metadata Integration
- [ ] Implement JSON parsing pipeline transforming TexturePacker schema into internal frame structures
- [ ] Extend manager state with atlas records co-located with existing texture records
- [ ] Ensure acquire/cache/release flows update both texture and atlas metadata atomically
- [ ] Add logging for atlas load success, parse failures, and placeholder fallbacks

## Phase 3 · Lifecycle Hooks & Diagnostics
- [ ] Update `reloadAll`, `forceUnload`, and `shutdown` to refresh or purge atlas metadata alongside textures
- [ ] Extend diagnostics/metrics endpoints to expose atlas counts, frame totals, and placeholder usage
- [ ] Add optional debug logging flag to dump atlas contents during troubleshooting

## Phase 4 · Testing & Validation
- [ ] Unit tests: atlas load success, cache hits, missing JSON/PNG, malformed frame entries, reload behavior
- [ ] Integration test: load `toolbaricons` atlas end-to-end and verify representative frame coordinates
- [ ] Performance sanity: measure atlas acquisition overhead vs. baseline texture load

## Phase 5 · Documentation & Rollout
- [ ] Update `docs/texture-manager.md` with atlas usage examples and limitations
- [ ] Publish tooling notes for artists specifying required TexturePacker export options
- [ ] Capture backlog items (runtime atlas packing, async loading, trimmed frame support)
