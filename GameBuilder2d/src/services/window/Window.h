#pragma once
#include <string>
#include <optional>

namespace gb2d {

enum class WindowState { Floating, Docked };

struct Size {
    int width{0};
    int height{0};
};

struct Window {
    std::string id;
    std::string title;
    WindowState state{WindowState::Floating};
    std::optional<Size> minSize{};
};

} // namespace gb2d
