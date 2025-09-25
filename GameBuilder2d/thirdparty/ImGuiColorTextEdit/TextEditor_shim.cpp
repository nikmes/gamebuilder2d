#include <algorithm>
#include <chrono>
#include <string>
#include <regex>
#include <cmath>

#include <TextEditor.h>

#include "imgui.h"

// Shim for removed APIs in newer Dear ImGui
namespace ImGui {
    inline ImGuiKey GetKeyIndex(ImGuiKey key) { return key; }
    inline void PushAllowKeyboardFocus(bool allow) { PushItemFlag(ImGuiItemFlags_NoTabStop, !allow); }
    inline void PopAllowKeyboardFocus() { PopItemFlag(); }
}

// Upstream implementation is compiled separately in the target sources.
