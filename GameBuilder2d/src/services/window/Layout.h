#pragma once
#include <string>
#include <vector>
#include <chrono>
#include "DockRegion.h"
#include "Window.h"

namespace gb2d {

struct Layout {
    std::string id;
    std::vector<DockRegion> regions{};
    std::vector<Window> windows{};
    std::chrono::system_clock::time_point lastSaved{};
    std::string name{}; // optional
};

} // namespace gb2d
