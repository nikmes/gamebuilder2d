#pragma once

#include <imgui.h>

// NOTE: This header is force-included for the ImGuiColorTextEdit target via CMake (/FI or -include)
// If you see build errors about missing GetKeyIndex or PushAllowKeyboardFocus, verify the compile options
// in GameBuilder2d/GameBuilder2d/CMakeLists.txt still list this file for the imguicolortextedit target.

namespace ImGui {

// ImGui 1.87-era API provided GetKeyIndex which was removed later.
// Map to the new ImGuiKey values directly so legacy code compiles.
inline ImGuiKey GetKeyIndex(ImGuiKey key) {
    return key;
}

// Push/PopAllowKeyboardFocus were removed; emulate using item flags.
inline void PushAllowKeyboardFocus(bool allow) {
    ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, !allow);
}

inline void PopAllowKeyboardFocus() {
    ImGui::PopItemFlag();
}

} // namespace ImGui
