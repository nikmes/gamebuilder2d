# Task Breakdown — TextureManager Service

## Clarifications & Preparations
- [ ] Decide on placeholder texture asset (path, appearance)
- [ ] Confirm canonical path / alias strategy
- [ ] Define configuration keys in `config.json`

## Engineering
- [ ] Create `TextureManager` service interface and implementation stubs
- [ ] Hook service into existing service locator / bootstrap
- [ ] Implement cache map with reference counting
- [ ] Implement load-on-demand with configuration-driven parameters
- [ ] Implement placeholder return on failure and log errors
- [ ] Implement `release`, `forceUnload`, and `reloadAll`
- [ ] Expose read-only metrics API (count, memory estimate)

## Integration
- [ ] Replace direct Raylib load calls in editor UI (pilot: GameWindow)
- [ ] Replace direct load calls in fullscreen session / gameplay code paths
- [ ] Update configuration defaults and migration notes

## Quality Assurance
- [ ] Unit tests (load, cache hit, release to zero, force unload, reload failure)
- [ ] Integration smoke test (request → render → reload)
- [ ] Performance sanity (measure synchronous load cost for representative textures)

## Documentation & Handoff
- [ ] Add developer guide / README section for TextureManager usage
- [ ] Update changelog / release notes
- [ ] Capture future enhancements (async loading, hot reload, LRU, compression)
