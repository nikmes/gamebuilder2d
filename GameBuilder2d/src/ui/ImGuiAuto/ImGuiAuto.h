#pragma once

#include <imgui.h>
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>
#include <any>
#include <memory>

namespace ImGui {

// Define ImGuiAutoFlags
enum ImGuiAutoFlags_
{
    ImGuiAutoFlags_None                  = 0,
    ImGuiAutoFlags_ReadOnly              = 1 << 0,
    ImGuiAutoFlags_NoLabel               = 1 << 1,
    ImGuiAutoFlags_ExpandByDefault       = 1 << 2,
    ImGuiAutoFlags_Compact               = 1 << 3,
    ImGuiAutoFlags_NoTooltip             = 1 << 4,
    // ... add more flags as needed
};

typedef int ImGuiAutoFlags; // Forward compatibility: may change to ImU32

/**
 * @brief ImGui::Auto - A reflection-based automatic UI system for ImGui
 * 
 * This class provides a way to automatically generate ImGui widgets based on data types,
 * making it easier to create consistent UIs with less boilerplate code.
 */
class Auto {
public:
    /**
     * @brief Initialize the ImGui::Auto system
     * Must be called once before using any other functions
     */
    static void Init();

    /**
     * @brief Shutdown the ImGui::Auto system
     * Should be called when shutting down the application
     */
    static void Shutdown();

    /**
     * @brief Display a property with an automatically selected widget based on the data type
     * 
     * @param label The label for the property
     * @param value Reference to the value being edited
     * @param flags Optional flags to control the widget behavior
     * @return bool True if the value was modified
     */
    template<typename T>
    static bool Property(const char* label, T& value, ImGuiAutoFlags flags = 0);

    /**
     * @brief Begin a collapsible section for related properties
     * 
     * @param label The label for the section
     * @param flags Optional flags to control the section behavior
     * @return bool True if the section is open and should be populated
     */
    static bool BeginSection(const char* label, ImGuiAutoFlags flags = 0);

    /**
     * @brief End a section previously started with BeginSection
     */
    static void EndSection();

    /**
     * @brief Register a custom property editor for a specific type
     * 
     * @param typeName The name of the type to register
     * @param editor The editor function to call for this type
     */
    template<typename T>
    static void RegisterPropertyEditor(std::function<bool(const char* label, T& value, ImGuiAutoFlags flags)> editor);

    /**
     * @brief Register a custom property drawer for a specific type
     * 
     * @param typeName The name of the type to register
     * @param drawer The drawer function to call for this type
     */
    template<typename T>
    static void RegisterPropertyDrawer(std::function<bool(const char* label, const T& value, ImGuiAutoFlags flags)> drawer);

private:
    // Implementation details
    static bool initialized;
    
    // Add private implementation details here
};

} // namespace ImGui

// Include the implementation template methods
#include "ImGuiAuto.inl"