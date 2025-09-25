# Quickstart: Embedded .NET Runtime & Script Interop

## 1. Prerequisites
- .NET 9 SDK installed
- GameBuilder2d built (Debug or Release)
- Runtime assets (hostfxr, coreclr) placed in `runtimes/` alongside executable

## 2. Build a Managed Script Assembly
Example `GameScripts.csproj` (class library targeting net8.0):
```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net8.0</TargetFramework>
    <ImplicitUsings>enable</ImplicitUsings>
    <Nullable>enable</Nullable>
  </PropertyGroup>
</Project>
```

Script file:
```csharp
public static class SampleWindowScript {
    public static void Run() {
        int id = Gb2dWindowNative.CreateWindow(800, 600, "Scripted Window");
        Gb2dWindowNative.UpdateTitle(id, "Updated Title");
        Gb2dLogger.Info("Window created with id=" + id);
    }
}
```

Interop wrappers (simplified):
```csharp
internal static class Gb2dLogger {
    [DllImport("GameBuilder2d", EntryPoint="gb2d_log_info", CharSet=CharSet.Ansi)]
    internal static extern void Info(string msg);
}
```

## 3. Place Assembly
Copy `GameScripts.dll` into `scripts/` directory recognized by engine (configure search paths in config.json once defined).

## 4. Native Initialization Flow
1. Start GameBuilder2d → runtime bootstrap locates hostfxr & initializes once.
2. Engine enumerates `scripts/` and loads assemblies (or loads on-demand).
3. Invoke from native: `engine.Invoke("SampleWindowScript::Run");`

## 5. Hot Reload
1. Rebuild `GameScripts.dll`.
2. Replace file on disk.
3. Trigger reload command (keybind / console) → engine unloads context, loads new assembly version, logs lifecycle events.

## 6. Logging From Scripts
`Gb2dLogger.Info("Message");` routes to native LoggerManager with script id tagging.

## 7. Error Handling
- Missing method → METHOD_NOT_FOUND
- Script not loaded → CONTEXT_UNLOADING or NOT_INITIALIZED depending on state
- Managed exception → RUNTIME_ERROR with message captured

## 8. Unloading
`engine.Unload("GameScripts")` → marks context unloading, waits for in-flight invoke, releases handles, logs event.

## 9. Future Enhancements (Not in MVP)
- Struct payload fast path
- Timeout support
- Logging rate limiting
- Window metadata readback

---
This quickstart reflects MVP assumptions; update once configuration keys and final status codes are locked.
