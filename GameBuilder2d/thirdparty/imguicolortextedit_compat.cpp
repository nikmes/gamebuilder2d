#include "imgui.h"

// Compatibility shims for Dear ImGui newer API
namespace ImGui {
    // ImGui::GetKeyIndex was removed; new API uses ImGuiKey directly
    inline ImGuiKey GetKeyIndex(ImGuiKey key) { return key; }

    // ImGui::PushAllowKeyboardFocus/PopAllowKeyboardFocus were replaced by item flags
    inline void PushAllowKeyboardFocus(bool allow) { PushItemFlag(ImGuiItemFlags_NoTabStop, !allow); }
    inline void PopAllowKeyboardFocus() { PopItemFlag(); }
}

// Include upstream implementation after defining shims
#include "TextEditor.cpp"
