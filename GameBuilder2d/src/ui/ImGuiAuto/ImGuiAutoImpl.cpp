#include "ImGuiAuto.h"
#include <imgui.h>

namespace ImGui {

namespace AutoImpl {

// Specialized handlers for different types
bool HandleInt(const char* label, int& value, ImGuiAutoFlags flags) {
    if (flags & ImGuiAutoFlags_ReadOnly) {
        ImGui::LabelText(label, "%d", value);
        return false;
    }
    
    if (flags & ImGuiAutoFlags_Compact) {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
    }
    
    return ImGui::DragInt(label, &value);
}

bool HandleFloat(const char* label, float& value, ImGuiAutoFlags flags) {
    if (flags & ImGuiAutoFlags_ReadOnly) {
        ImGui::LabelText(label, "%.3f", value);
        return false;
    }
    
    if (flags & ImGuiAutoFlags_Compact) {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
    }
    
    return ImGui::DragFloat(label, &value, 0.01f);
}

bool HandleBool(const char* label, bool& value, ImGuiAutoFlags flags) {
    if (flags & ImGuiAutoFlags_ReadOnly) {
        ImGui::LabelText(label, value ? "True" : "False");
        return false;
    }
    
    return ImGui::Checkbox(label, &value);
}

bool HandleString(const char* label, std::string& value, ImGuiAutoFlags flags) {
    if (flags & ImGuiAutoFlags_ReadOnly) {
        ImGui::LabelText(label, "%s", value.c_str());
        return false;
    }
    
    if (flags & ImGuiAutoFlags_Compact) {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
    }
    
    // Use a buffer for InputText
    char buffer[256];
    strncpy(buffer, value.c_str(), sizeof(buffer));
    buffer[sizeof(buffer) - 1] = 0;
    
    if (ImGui::InputText(label, buffer, sizeof(buffer))) {
        value = buffer;
        return true;
    }
    
    return false;
}

} // namespace AutoImpl

} // namespace ImGui