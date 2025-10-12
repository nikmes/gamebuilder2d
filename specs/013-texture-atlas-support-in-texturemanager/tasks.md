# Task Breakdown — TextureAtlas Support in TextureManager

## Milestone A · Foundations
- [x] **T001** · Finalize atlas API signature and documentation
- [x] **T002** · Select / integrate JSON parsing utility for TexturePacker schema
- [x] **T003** · Resolve handling for trimmed/rotated frames in TexturePacker exports
- [x] **T004** · Align naming conventions for atlas identifiers and frame keys across editor/gameplay code

## Milestone B · API & Data Model
- [x] **T005** · Define `TextureAtlasHandle` contract and frame lookup view
- [x] **T006** · Extend `TextureManager` headers with atlas acquire/release/lookups
- [x] **T007** · Update metrics structures to report atlas-specific counts and memory
- [x] **T008** · Document lifetime rules for atlas handles in developer guide draft updates

## Milestone C · Loader & Metadata Integration
- [x] **T009** · Implement JSON parsing pipeline for TexturePacker schema
- [x] **T010** · Extend manager state with co-located atlas records
- [x] **T011** · Ensure acquire/cache/release flows update texture and atlas metadata atomically
- [x] **T012** · Add logging for atlas load success, parse failures, and placeholder fallbacks

## Milestone D · Lifecycle Hooks & Diagnostics
- [x] **T013** · Update reload/force-unload/shutdown to refresh or purge atlas metadata
- [x] **T014** · Extend diagnostics/metrics endpoints for atlas counts, frames, placeholders
- [x] **T015** · Add optional debug logging flag to dump atlas contents for troubleshooting

## Milestone E · Testing & Validation
- [x] **T016** · Author unit tests covering atlas load, cache hits, failures, reload behavior
- [x] **T017** · Add integration test using `toolbaricons` atlas to verify frame coordinates
- [ ] **T018** · Measure atlas acquisition performance relative to baseline texture load

## Milestone F · Documentation & Rollout
- [ ] **T019** · Update `docs/texture-manager.md` with atlas usage examples and limitations
- [ ] **T020** · Publish tooling notes for TexturePacker export requirements
- [ ] **T021** · Capture backlog items (runtime packing, async loading, trimmed frame support)

## Milestone G · Configuration Modernization
- [x] **T022** · Remove legacy configuration key canonicalization layer so nested audio keys become the single source of truth
- [x] **T023** · Update default `config.json`, unit fixtures, and integration snapshots to use nested audio.volumes/engine/preload paths
- [x] **T024** · Refresh configuration documentation and environment override guidance to reflect the new key structure (no legacy aliases)
