# Feature Specification: Embedded .NET Runtime Script Engine Integration

**Feature Branch**: `006-embed-dotnet-runtime`  
**Created**: 2025-09-25  
**Status**: Draft  
**Input**: User description: "Embed the .NET runtime via the official Hosting API (CoreCLR CoreCLRHost). Compile a native bridge library that loads CoreClrCreateDelegate, exposes script entry points, and marshals data via C-style structs or JSON. On Windows and Linux, ship the appropriate runtime assets alongside your app, resolve them with hostfxr helpers, and keep the CLR in its own module to avoid platform-specific conditionals inside game code. Wrap this bridge behind a ScriptEngine interface so the rest of your C++ code simply calls engine.Invoke(\"Namespace.Class::Method\"), letting you hot-reload assemblies or unload AppDomains (AssemblyLoadContexts) per script." 

## Execution Flow (main)
```
1. Parse user description from Input
	→ If empty: ERROR "No feature description provided"
2. Extract key concepts from description
	→ Identify: actors (game runtime, script author), actions (call native services from managed code, hot reload, unload script), data (script assemblies, windows, logs), constraints (cross-platform Win/Linux, isolation, minimal coupling)
3. For each unclear aspect:
	→ Mark with [NEEDS CLARIFICATION: specific question]
4. Fill User Scenarios & Testing section
	→ If no clear user flow: ERROR "Cannot determine user scenarios"
5. Generate Functional Requirements
	→ Each requirement must be testable
	→ Mark ambiguous requirements
6. Identify Key Entities (scripts, assemblies, contexts, windows, logs, status codes)
7. Run Review Checklist
	→ If any [NEEDS CLARIFICATION]: WARN "Spec has uncertainties"
	→ If implementation details found: ERROR "Remove tech details" (Note: this feature inherently references .NET hosting concept; confined to WHAT behaviors the system must expose)
8. Return: SUCCESS (spec ready for planning)
```

---

## ⚡ Quick Guidelines
- ✅ Focus on WHAT capability the engine must provide to end-users (game systems / tool developers) and WHY (extend game without recompiling C++)
- ❌ Avoid code-level implementation specifics (no function signatures, build flags) beyond naming required externally observable capabilities
- 👥 Written for stakeholders who care about extensibility & modding workflow

### Section Requirements
- **Mandatory sections** completed
- Optional sections omitted if not relevant

### Ambiguity Handling Notes
All prior clarification markers have been resolved; decisions recorded in research & plan documents.

---

## User Scenarios & Testing *(mandatory)*

### Primary User Story
As a game/tool developer, I want managed (.NET) scripts to call core native engine services (WindowManager, LoggerManager) so I can extend and prototype functionality without modifying or recompiling native C++ code.

### Acceptance Scenarios
1. **Given** the game is running and the runtime is initialized, **When** a managed script calls the create window interop, **Then** a native window is created and a valid window id is returned.
2. **Given** a window id obtained from a previous call, **When** the script calls set title, **Then** the native window title changes and the operation returns OK.
3. **Given** the logging interop is available, **When** a script logs info/warn/error messages, **Then** they appear in the native logging system with correct severity and script attribution.
4. **Given** the runtime is not yet initialized, **When** a script attempts any interop call, **Then** it receives NOT_INITIALIZED and no native action occurs.
5. **Given** a script unload is initiated, **When** the script attempts further interop calls, **Then** CONTEXT_UNLOADING (or SHUTDOWN after full teardown) is returned.
6. **Given** a script creates one or more windows, **When** that script unloads, **Then** its windows are automatically closed and further operations on their ids return INVALID_ID.
7. **Given** a duplicate load request for an already loaded script without a reload flag, **When** the load is attempted, **Then** ALREADY_LOADED is returned and no new context is created.
8. **Given** rapid successive reload requests (<500 ms apart), **When** they are triggered, **Then** only the first proceeds and others are coalesced with a debounce log entry.

### Edge Cases
- Loading a script while a previous version is unloading → New load waits synchronously until prior unload completes, then proceeds (or returns ALREADY_LOADED if unchanged).
- Invalid window id usage → Returns INVALID_ID (no undefined behavior).
- Rapid consecutive reloads (<500 ms) → Debounced (single execution, others coalesced and logged).
- Unload during active interop call → Call completes; subsequent calls return CONTEXT_UNLOADING.
- Shutdown during script activity → Remaining interop calls return SHUTDOWN (no native access after teardown).
- Logging spam → All accepted (MVP); per-script counters incremented for observability.
- Duplicate load without reload flag → ALREADY_LOADED.
- Script attempts window API before runtime init → NOT_INITIALIZED.

## Requirements *(mandatory)*

### Functional Requirements
- **FR-001**: System MUST initialize the embedded .NET runtime exactly once per application lifecycle.
- **FR-002**: System MUST load managed script assemblies at runtime given a file path or identifier.
- **FR-003**: System MUST expose managed-callable WindowManager operations (create, set title, close, exists) through a stable interop surface.
- **FR-004**: System MUST expose managed-callable logging operations (info, warning, error) routing messages to native LoggerManager with script attribution.
- **FR-005**: System MUST define and document a finite set of interop status codes (OK, NOT_INITIALIZED, INVALID_ID, CONTEXT_UNLOADING, ALREADY_LOADED, SHUTDOWN, INTERNAL_ERROR, RUNTIME_ERROR).
- **FR-006**: System MUST marshal window-related interop originating off the main thread onto the main thread safely.
- **FR-007**: System MUST reject duplicate script loads unless an explicit Reload flag is provided (synchronous unload+replace).
- **FR-008**: System SHOULD support hot reload via unload+replace maintaining isolation of contexts.
- **FR-009**: System MUST validate required runtime assets at startup and report clear errors if missing.
- **FR-010**: System MUST operate identically on Windows and Linux with the same managed API surface (no conditional user code required).
- **FR-011**: System MUST ensure interop calls prior to runtime initialization return NOT_INITIALIZED.
- **FR-012**: System MUST log lifecycle events (runtime init, script load/unload/reload) at info; errors/exceptions at error severity.
- **FR-013**: System MUST auto-close windows owned by a script on unload.
- **FR-014**: System MUST provide enumeration of loaded scripts with status.
- **FR-015**: System MUST support graceful shutdown where further interop returns SHUTDOWN without unsafe access.
- **FR-016**: System SHOULD allow configuration of runtime asset/search paths; default `./runtimes/` plus configured `scripting.runtimeSearchPaths`.
- **FR-017**: System SHOULD probe dependencies in base directory + `scripts/SharedLibs/`.
- **FR-018**: System MUST record per-script log/message counts for observability.
- **FR-019**: System MUST validate UTF-8 inputs and treat invalid sequences as BAD_FORMAT (error path).
- **FR-020**: System MUST version the interop contract with `GB2D_INTEROP_API_VERSION = 1`.
- **FR-021**: System MUST expose a safe mechanism to dispose/unload a script context releasing resources.
- **FR-022**: System SHOULD defer window metadata query (width/height/title) to a future extension (documented but not implemented).

### Key Entities *(include if feature involves data)*
- **Script Assembly**: Managed code unit; id, path, version token, status.
- **Script Context**: Isolation boundary for a script; state, created_at, reload_count.
- **Runtime Assets**: Files required to bootstrap .NET hosting.
- **Interop Status Code**: Code + name + description for managed-visible operation results.
- **Window Handle Identifier**: Opaque integer referencing a native window created via interop.
- **Log Message**: Severity, timestamp, script id, message text.

---

## Review & Acceptance Checklist
*GATE: Automated checks run during main() execution*

### Content Quality
- [ ] No implementation details (languages, frameworks, APIs)
- [ ] Focused on user value and business needs
- [ ] Written for non-technical stakeholders
- [ ] All mandatory sections completed

### Requirement Completeness
- [x] No [NEEDS CLARIFICATION] markers remain
- [ ] Requirements are testable and unambiguous  
- [ ] Success criteria are measurable
- [ ] Scope is clearly bounded
- [ ] Dependencies and assumptions identified

---

## Execution Status
*Updated by main() during processing*

- [x] User description parsed
- [x] Key concepts extracted
- [x] Ambiguities marked
- [x] User scenarios defined
- [x] Requirements generated
- [x] Entities identified
- [ ] Review checklist passed

---

