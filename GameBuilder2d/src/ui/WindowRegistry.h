#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "ui/Window.h"

namespace gb2d {

struct WindowTypeDesc {
    std::string typeId;
    std::string displayName;
    std::function<std::unique_ptr<IWindow>(WindowContext&)> factory;
};

class WindowRegistry {
public:
    void registerType(WindowTypeDesc desc);
    std::unique_ptr<IWindow> create(const std::string& typeId, WindowContext& ctx) const;
    const std::vector<WindowTypeDesc>& types() const { return types_; }
private:
    std::vector<WindowTypeDesc> types_{};
};

} // namespace gb2d
