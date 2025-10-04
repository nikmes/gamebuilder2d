#pragma once

#include <type_traits>

namespace ImGui {

template<typename T>
bool Auto::Property(const char* label, T& value, ImGuiAutoFlags flags) {
    // TODO: Implement type-specific handling through type traits
    
    // Default implementation for unknown types
    if constexpr (std::is_same_v<T, int>) {
        return ImGui::DragInt(label, &value);
    }
    else if constexpr (std::is_same_v<T, float>) {
        return ImGui::DragFloat(label, &value);
    }
    else if constexpr (std::is_same_v<T, bool>) {
        return ImGui::Checkbox(label, &value);
    }
    else if constexpr (std::is_same_v<T, std::string>) {
        char buffer[256];
        strncpy(buffer, value.c_str(), sizeof(buffer));
        buffer[sizeof(buffer) - 1] = 0;
        if (ImGui::InputText(label, buffer, sizeof(buffer))) {
            value = buffer;
            return true;
        }
        return false;
    }
    else {
        // For types we don't know how to handle, just display the label
        ImGui::Text("%s: [Unsupported Type]", label);
        return false;
    }
}

template<typename T>
void Auto::RegisterPropertyEditor(std::function<bool(const char* label, T& value, ImGuiAutoFlags flags)> editor) {
    // TODO: Implement editor registration system
}

template<typename T>
void Auto::RegisterPropertyDrawer(std::function<bool(const char* label, const T& value, ImGuiAutoFlags flags)> drawer) {
    // TODO: Implement drawer registration system
}

} // namespace ImGui