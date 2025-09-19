#pragma once
#include <string>
#include <vector>
#include <variant>

namespace gb2d {

enum class DockPosition { Left, Right, Top, Bottom, Center };

struct RegionSize {
    int width{0};
    int height{0};
};

struct DockRegionRef { std::string id; };
struct WindowRef { std::string id; };

using RegionChild = std::variant<DockRegionRef, WindowRef>;

struct DockRegion {
    std::string id;
    DockPosition position{DockPosition::Left};
    RegionSize size{0, 0};
    std::vector<RegionChild> children{};
    std::string activeTabId{}; // when tabbed with windows
};

} // namespace gb2d
