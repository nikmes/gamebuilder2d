#pragma once
#include <memory>
#include <string>
#include <optional>
#include <functional>
#include <nlohmann/json_fwd.hpp>
#include "services/window/Window.h" // for Size

namespace gb2d {

struct WindowContext;

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

    // Returning false vetoes the close request; window remains open
    virtual bool handleCloseRequest(WindowContext&) { return true; }

    // Lifecycle hooks
    virtual void onFocus(WindowContext&) {}
    virtual void onClose(WindowContext&) {}

    // Persistence (nlohmann::json)
    virtual void serialize(nlohmann::json& out) const {}
    virtual void deserialize(const nlohmann::json& in) {}
};

} // namespace gb2d
