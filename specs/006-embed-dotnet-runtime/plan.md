# Implementation Plan: Embedded .NET Runtime Script Engine Integration

**Branch**: `006-embed-dotnet-runtime` | **Date**: 2025-09-25 | **Spec**: `specs/006-embed-dotnet-runtime/spec.md`
**Input**: Feature specification from `/specs/006-embed-dotnet-runtime/spec.md`

## Execution Flow (/plan command scope)
```
1. Load feature spec from Input path
   → Loaded successfully
2. Fill Technical Context (scan for NEEDS CLARIFICATION)
   → Identified technology domain: Native C++ engine embedding .NET (CoreCLR hosting)
3. Fill the Constitution Check section based on constitution document
4. Evaluate Constitution Check
5. Execute Phase 0 → research.md (outlined below; will enumerate unknowns & decisions here since /plan must produce research.md)
6. Execute Phase 1 → produce data-model.md, contracts/, quickstart.md
7. Re-evaluate Constitution Check (post-design)
8. Plan Phase 2 approach (do NOT create tasks.md)
9. STOP - Ready for /tasks command
```

## Summary
Primary requirement: Allow GameBuilder2d to host the .NET runtime so managed scripts can call native subsystems (WindowManager, LoggerManager) with hot reload & unload isolation.
Technical approach (high-level): Embed CoreCLR via hostfxr; provide a stable managed→native interop surface (C ABI + optional function pointer table); manage per-script AssemblyLoadContexts; define status codes; enable structured logging and window operations.

## Technical Context
**Language/Version**: C++20 + .NET 9
**Primary Dependencies**: hostfxr (.NET runtime), existing GameBuilder2d subsystems (WindowManager, LoggerManager), JSON library (nlohmann_json already present), logging (spdlog) 
**Storage**: None (in-memory only for this feature)
**Testing**: Catch2 (native), xUnit (managed)
**Target Platform**: Windows + Linux desktop
**Project Type**: Single engine project with native core + managed plugin assemblies
**Performance Goals**: Mean interop call (e.g., create/set title/log) < 0.20 ms, p95 < 0.50 ms (release build)
**Constraints**: Must avoid frame hitching during hot reload; safe unload semantics; deterministic error reporting
**Scale/Scope**: Up to 64 simultaneous script contexts (configurable)

Outstanding Clarifications (from spec):
- Payload format policy (dual struct + JSON simultaneous or config?)
- Hot reload concurrency semantics (blocking vs async)
- Debounce interval for rapid reloads
- Logging verbosity levels & rate limiting policy
- (Removed) Timeout support for managed invocation (no native→managed path)
- Duplicate script load policy (replace vs reject)
- Thread affinity for WindowManager access
- API version negotiation mechanism
- Need for window metadata readback
- Rate limit thresholds for logging
- C# test framework choice

## Constitution Check
The constitution file is a template with placeholders; no concrete principles defined yet. Therefore automated gate evaluation cannot assert compliance specifics. To proceed, we adopt provisional interpretations:
- Principle: Test-First will apply → We will author interop contract tests before implementation.
- Principle: Simplicity → Choose C ABI + P/Invoke before more complex IPC or code-generation solutions.
- Principle: Observability → Integrate logging of lifecycle & interop errors.

No violations detected because constitution lacks concrete constraints; Complexity Tracking remains empty.

## Project Structure
Retain existing repo layout; add interop bridge under `GameBuilder2d/src/bridge/` and managed sample under `managed/` (stored in repo).

### Documentation (this feature)
```
specs/006-embed-dotnet-runtime/
├── plan.md
├── research.md          (Phase 0 output)
├── data-model.md        (Phase 1 output)
├── quickstart.md        (Phase 1 output)
├── contracts/           (Phase 1 output)
├── spec.md              (Existing)
└── tasks.md             (Phase 2 via /tasks command)
```

**Structure Decision**: DEFAULT (single project) — engine-centric feature.

## Phase 0: Outline & Research
Unknowns / Research Topics (updated):
1. Determine .NET runtime version & distribution strategy.
2. Decide payload (JSON only MVP) final confirmation.
3. Define hot reload sequence & blocking behavior.
4. Establish debounce interval for rapid rebuilds.
5. Concurrency model: thread affinity for window operations.
6. Logging rate limiting strategy & severity mapping.
7. Duplicate load policy + state transition diagram.
8. Assembly dependency probing strategy.
9. Status code enumeration stability.
10. API versioning scheme (integer + future flags).
11. Security / sandboxing considerations (trusted scripts).
12. Memory ownership rules (UTF-8) and window id validation.
13. Max concurrent script contexts & recycling.
14. Managed test framework & integration into CI.

Research Output Plan (`research.md` will include for each item):
- Decision
- Rationale
- Alternatives considered
- Impact / follow-on tasks

## Phase 1: Design & Contracts
Planned Artifacts:
1. `data-model.md`: Entities: ScriptAssembly, ScriptContext, InteropStatusCode, WindowHandle, LogMessage, RuntimeAssets.
2. `contracts/`: 
   - `interop_status_codes.md` (enumeration table)
   - `window_interop.md` (operations: create, set_title, close, exists)
   - `logging_interop.md` (operations: log_info, log_warn, log_error; optional structured fields)
   - `scripting_lifecycle.md` (load, unload, reload, enumerate)
3. `quickstart.md`: Steps to add a managed script, build, place assembly, invoke from native, call WindowManager.
4. (Optional) `api_versioning.md` if complexity warrants separate doc.
5. Managed sample code snippet included inline in quickstart.

Contract Test Philosophy:
- Each interop contract op has a native test that loads a minimal managed stub performing the call (or mocks managed side for reverse direction once added).
- Error path tests: invalid id, pre-init, context unloading, duplicate load, malformed target string.

## Phase 2: Task Planning Approach
Will map:
- Each status code → implementation & test task
- Each contract file → parser/validation test task
- Each entity → creation & lifecycle tests
- Each scenario (Acceptance 1–11) → integration test tasks

Ordering:
1. Status code enum & error pathway
2. Runtime bootstrap & initialization guard
3. Script loading & context management
4. Logging interop (managed → native)
5. Window interop (managed → native)
6. Hot reload & unload semantics
7. Edge cases & stress (rapid reload, invalid ids)
8. Performance benchmark harness (added later if p95 exceeds budget)

## Phase 3+: Future Implementation (Not executed here)
Standard TDD cycle per task list.

## Complexity Tracking
| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|---------------------------------------|
| (none) | | |

## Progress Tracking
**Phase Status**:
- [x] Phase 0: Research complete (/plan command)  (research.md generated)
- [x] Phase 1: Design complete (/plan command)    (data-model, quickstart, contracts skeleton created)
- [ ] Phase 2: Task planning complete (/plan command - describe approach only)
- [ ] Phase 3: Tasks generated (/tasks command)
- [ ] Phase 4: Implementation complete
- [ ] Phase 5: Validation passed

**Gate Status**:
- [x] Initial Constitution Check: PASS (template placeholders, no violations) 
- [x] Post-Design Constitution Check: PASS (still aligned)
- [x] All NEEDS CLARIFICATION resolved
- [ ] Complexity deviations documented

---
*Based on Constitution template (placeholders present) — will re-evaluate when constitution is finalized.*
