# Contracts: IWindow, WindowContext, WindowRegistry

> Pseudocode/C++-like signatures. Actual headers will live in `src/ui`.

## IWindow

```cpp
class IWindow {
public:
    virtual ~IWindow() = default;

    // Identity
    virtual const char* typeId() const = 0;       // e.g. "console-log"
    virtual const char* displayName() const = 0;  // e.g. "Console Log"

    // Title shown in ImGui title/tab
    virtual std::string title() const = 0;
    virtual void setTitle(std::string) = 0;

    // Optional minimum size for docking splits
    virtual std::optional<Size> minSize() const { return std::nullopt; }

    // Draw contents; use context for services and manager interactions
    virtual void render(WindowContext& ctx) = 0;

    // Lifecycle hooks
    virtual void onFocus(WindowContext&) {}
    virtual void onClose(WindowContext&) {}

    // Persistence (nlohmann::json)
    virtual void serialize(nlohmann::json& out) const {}
    virtual void deserialize(const nlohmann::json& in) {}
};
```

## WindowContext

```cpp
struct WindowContext {
    // Services
    logging::LogManager* log = nullptr; // or a thin adapter
    FileDialogService* files = nullptr; // wrapper over ImGuiFileDialog
    RecentFilesService* recent = nullptr;
    Notifications* notify = nullptr;    // addToast

    // Manager interactions (bound to the current window id internally)
    std::function<void()> requestFocus;
    std::function<void()> requestUndock;
    std::function<void()> requestClose;

    // Config access if needed
    const Config* config = nullptr;
};
```

## WindowRegistry

```cpp
struct WindowTypeDesc {
    std::string typeId;
    std::string displayName;
    std::function<std::unique_ptr<IWindow>(WindowContext&)> factory;
};

class WindowRegistry {
public:
    void registerType(WindowTypeDesc desc);
    std::unique_ptr<IWindow> create(const std::string& typeId, WindowContext& ctx) const;
    const std::vector<WindowTypeDesc>& types() const; // for menus
};
```

## Invariants
- `typeId()` must be globally unique and stable across versions (for layout restore).
- Windows may not store raw pointers to `WindowManager`; only use `WindowContext`.
- Window `serialize` must not include transient resources (e.g., GPU texture ids).
