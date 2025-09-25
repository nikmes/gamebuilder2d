# Phase 0 Research: Embedded .NET Runtime & Interop

Date: 2025-09-25  
Branch: 006-embed-dotnet-runtime  
Spec: `specs/006-embed-dotnet-runtime/spec.md`

## Summary
Goal: Confident design for hosting .NET runtime inside GameBuilder2d enabling managed→native interop (Window & Logging) and hot reload via unloadable contexts.

## Decisions & Rationale

### 1. .NET Runtime Version
Decision: Target .NET 9 (latest release; adopt early for longevity and features).  
Rationale: Access to newest runtime optimizations; aligns with forward-looking engine feature timeline; acceptable risk if minor breaking changes occur before first public release.  
Alternatives: .NET 8 (LTS, more conservative), .NET 6 (aging).  
Follow-up: Monitor .NET 9 servicing updates; confirm stability of hostfxr APIs (expected stable).

### 2. Distribution Strategy
Decision (tentative): Framework-dependent with bundled runtime assets in `runtimes/` sibling to executable.  
Rationale: Avoid self-contained bloat; flexible updates.  
Alternatives: Self-contained publish (larger footprint).  
Follow-up: [OPEN] Validate licensing & footprint constraints.

### 3. Payload Format Policy
Decision (proposed): Support JSON initially; C-struct path reserved for performance-critical calls later.  
Rationale: JSON simpler, leverages existing nlohmann_json; reduces early complexity.  
Alternatives: Dual-format now (higher complexity), Proto/FlatBuffers (overkill now).  
Follow-up: [OPEN] Confirm if struct path is needed MVP.

### 4. Hot Reload Concurrency Model
Decision (tentative): Synchronous reload triggered manually; block new invokes until old context fully released.  
Rationale: Predictability; simpler correctness.  
Alternatives: Async staged swap (adds concurrency risk).  
Follow-up: Potential async optimization later.

### 5. Debounce Interval
Decision: 500ms default between reload operations.  
Rationale: Avoid thrash during rapid compilation loops.  
Alternatives: Configurable via config file; dynamic detection.  
Follow-up: [OPEN] Make configurable? (Yes, likely.)

### 6. Concurrency & Thread Affinity
Decision: All WindowManager interop calls marshalled to main thread queue if invoked off-thread.  
Rationale: Typical graphics/windowing constraints; avoids race conditions.  
Alternatives: Lock-based multi-thread access (risk complexity).  
Follow-up: Implement a dispatch helper.

### 7. (Removed) Invocation Timeout
Not applicable—no native→managed invocation path in scoped MVP.

### 8. Logging Rate Limiting
Decision: Phase 1 skip enforcement; include severity tagging & script origin.  
Rationale: Premature optimization; collect data first.  
Alternatives: Token bucket per script.  
Follow-up: Add counters; implement limit if abuse observed.

### 9. Duplicate Script Load Policy
Decision: Reject with explicit "already loaded" unless caller specifies a `Reload` flag.  
Rationale: Avoid accidental shadowing; explicit intent for replacement.  
Alternatives: Replace automatically (surprise), Keep both versions (complex).  
Follow-up: Add `Reload` parameter spec.

### 10. Assembly Dependency Probing
Decision: Support base directory + `scripts/SharedLibs` search path list configurable in config.json.  
Rationale: Extensible while simple.  
Alternatives: Custom resolver callbacks (later).  
Follow-up: Document search order.

### 10. Status Code Enumeration
Decision: Integer enum stable: OK=0, NOT_INITIALIZED, INVALID_ID, CONTEXT_UNLOADING, ALREADY_LOADED, METHOD_NOT_FOUND, RUNTIME_ERROR, INTERNAL_ERROR, SHUTDOWN, BAD_FORMAT.  
Rationale: Coverage of spec errors; leaves space for expansion.  
Alternatives: String codes (slower comparisons).  
Follow-up: Reserve ranges for future expansions.

### 11. API Versioning
Decision: Single int `GB2D_INTEROP_API_VERSION = 1`; capabilities via optional flag bitmask later.  
Rationale: Simple & adequate now.  
Alternatives: Semantic version; negotiation struct (overkill now).  
Follow-up: Add capabilities when first optional feature appears.

### 12. Security / Trust
Decision: Treat scripts as trusted (no sandbox) in MVP.  
Rationale: Internal modding/development environment.  
Alternatives: AppDomain partial trust (deprecated) or WASM isolation.  
Follow-up: Document risk; revisit if distributing third-party mods.

### 13. Memory Ownership
Decision: All inbound `const char*` copied immediately to std::string; outbound strings either transient (call-scope) or caller-provided buffer later.  
Rationale: Eliminates lifetime hazards.  
Alternatives: Borrowed pointers (unsafe).  
Follow-up: Provide retrieval APIs returning length + copy pattern.

### 14. Max Concurrent Contexts
Decision: Hard advisory cap 64 contexts; configurable; enforce with load refusal.  
Rationale: Avoid excessive memory fragmentation & handle exhaustion.  
Alternatives: Unlimited (risk).  
Follow-up: Introduce metrics.

### 15. Managed Test Framework
Decision: xUnit (broad ecosystem).  
Rationale: Common & parallel test support.  
Alternatives: MSTest (more boilerplate), NUnit (similar).  
Follow-up: Add test harness invoking host with test assemblies.

### 16. Window Metadata Readback
Decision: Defer until a use case arises; spec requirement remains SHOULD.  
Rationale: YAGNI; no current acceptance scenario needs it.  
Alternatives: Implement now (adds scope).  
Follow-up: Add FR if scenario emerges.

## Open Items (Unresolved)
- Confirm C++ standard (17 vs 20).
- Confirm distribution packaging process for runtime assets.
- Decide config key names for search paths & debounce interval.
- Determine instrumentation format (JSON log vs plain) for interop events.

## Risk Assessment
| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|-----------|
| Hot reload deadlocks | High | Medium | Synchronous blocking model + locks ordering doc |
| Memory leaks on context unload | Medium | Medium | RAII wrappers + stress tests |
| Performance regression with JSON payload | Medium | Medium | Benchmark; introduce struct fast path later |
| Logging spam | Low | Medium | Add counters; later rate limit |
| Cross-platform path differences | Medium | Medium | Normalize path resolution layer |

## Benchmarks Plan (Future)
- Measure average Invoke latency (native→managed round trip) with empty and moderately complex method.
- Measure hot reload time vs assembly size.

## Next Steps for Phase 1
1. Draft status code header & mapping doc.
2. Define interop C API header skeleton.
3. Draft managed P/Invoke wrapper stub.
4. Produce data-model & contracts from decisions above.

---
All identified critical decisions have provisional answers; open items tracked for refinement before implementation tasks generation.
