# Implementation Plan — TextureManager Service

## Phase 0 · Alignment & Clarifications
- [ ] Review outstanding questions from the spec (placeholder asset, path normalization rules, memory limits)
- [ ] Confirm configuration keys and defaults with stakeholders

## Phase 1 · Scaffolding
- [ ] Define public service API in header (init, shutdown, acquire, tryGet, release, forceUnload, reloadAll, metrics)
- [ ] Establish service registration in the same module that registers existing managers
- [ ] Introduce configuration struct + default values (search paths, filters, placeholder texture)

## Phase 2 · Core Cache & Lifecycle
- [ ] Implement internal cache map (TextureKey → TextureRecord with ref counting)
- [ ] Implement load-on-demand with canonical path resolution and logging
- [ ] Implement release + automatic unload when ref count reaches zero
- [ ] Implement force-unload bypassing reference tracking (debug/dev workflows)

## Phase 3 · Resilience & Diagnostics
- [ ] Add placeholder texture handling for failed loads
- [ ] Implement reloadAll traversal with aggregate success reporting
- [ ] Wire logging + metrics (loaded count, VRAM estimate)
- [ ] Provide optional diagnostics hook for UI overlays (list cached textures)

## Phase 4 · Integration & Adoption
- [ ] Replace direct `LoadTexture` calls in existing code paths with manager usage (scoped pilot areas)
- [ ] Update configuration docs and examples
- [ ] Add developer-facing guide for requesting/releasing textures

## Phase 5 · Testing & Validation
- [ ] Unit tests for cache hits, ref counting, unload, reload, failure paths
- [ ] Integration test covering reloadAll and placeholder flow
- [ ] Manual QA checklist for editor and fullscreen scenarios

## Phase 6 · Wrap-up
- [ ] Document known limitations (sync loads, no hot reload)
- [ ] Capture backlog items (async loading, LRU eviction, filesystem watch)
- [ ] Final review & merge readiness checklist
