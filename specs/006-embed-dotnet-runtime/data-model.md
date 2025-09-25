# Data Model: Embedded .NET Runtime & Interop

Date: 2025-09-25  
Branch: 006-embed-dotnet-runtime

## Entities

### ScriptAssembly
- id (string) – unique logical identifier
- path (string)
- version_token (string/hash)
- status (enum: loading|loaded|unloading|error)
- last_loaded_at (timestamp)
- dependency_paths (list<string>)

### ScriptContext
- assembly_id (fk ScriptAssembly.id)
- context_handle (opaque pointer/integer)
- state (enum: active|unloading|disposed)
- created_at (timestamp)
- reload_count (int)


### InteropStatusCode
- code (int)
- name (string)
- description (string)

### WindowHandle
- id (int)
- script_owner (optional string)
- created_at (timestamp)
- alive (bool)

### LogMessage
- severity (enum: info|warn|error)
- script_id (string optional)
- text (string)
- timestamp (timestamp)

## Relationships
- ScriptAssembly 1..1 → ScriptContext (current active) (future: multiple versions queued?)
- ScriptContext 1..N → WindowHandle (ownership optional)
- ScriptContext 1..N → LogMessage

## State Transitions
ScriptAssembly.status: loading → loaded → (unloading | error) → (reloaded -> loading) → loaded

ScriptContext.state: active → unloading → disposed

## Validation Rules
- target must match regex `^[A-Za-z0-9_.]+::[A-Za-z0-9_]+$`
- payload_format must match declared format support (json initially)
- duplicate assembly_id load without Reload flag → reject (status_code ALREADY_LOADED)
- unload only allowed when state=active

## Open Questions
- Multi-version staging? (Not MVP)
- (Resolved) Windows associated with a script are auto-closed during context unload.

---
End of data-model.
