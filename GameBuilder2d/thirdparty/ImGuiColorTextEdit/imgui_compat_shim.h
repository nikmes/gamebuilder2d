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

inline ImGuiKey LegacyKeyFromIndex(int index) {
    if (index >= static_cast<int>(ImGuiKey_NamedKey_BEGIN)) {
        return static_cast<ImGuiKey>(index);
    }
    const int mapped = static_cast<int>(ImGuiKey_NamedKey_BEGIN) + index;
    if (mapped < static_cast<int>(ImGuiKey_NamedKey_BEGIN) || mapped >= static_cast<int>(ImGuiKey_COUNT)) {
        return ImGuiKey_None;
    }
    return static_cast<ImGuiKey>(mapped);
}

inline bool IsKeyPressed(int index, bool repeat = true) {
    const ImGuiKey key = LegacyKeyFromIndex(index);
    if (key == ImGuiKey_None) {
        return false;
    }
    return ImGui::IsKeyPressed(key, repeat);
}

inline bool IsKeyDown(int index) {
    const ImGuiKey key = LegacyKeyFromIndex(index);
    if (key == ImGuiKey_None) {
        return false;
    }
    return ImGui::IsKeyDown(key);
}

// Push/PopAllowKeyboardFocus were removed; emulate using item flags.
inline void PushAllowKeyboardFocus(bool allow) {
    ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, !allow);
}

inline void PopAllowKeyboardFocus() {
    ImGui::PopItemFlag();
}

} // namespace ImGui
