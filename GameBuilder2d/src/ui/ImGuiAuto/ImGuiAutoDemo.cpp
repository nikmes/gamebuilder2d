#include "ImGuiAutoDemo.h"

namespace ImGui {
namespace AutoDemo {

void ShowDemo() {
    static bool showDemo = true;
    
    // Basic usage - simple types
    static int intValue = 42;
    static float floatValue = 3.14159f;
    static bool boolValue = true;
    static std::string stringValue = "Hello, ImGui::Auto!";
    
    // Start a demo window
    if (ImGui::Begin("ImGui::Auto Demo", &showDemo)) {
        // Initialize ImGui::Auto if needed
        ImGui::Auto::Init();
        
        // Simple usage for basic types
        ImGui::Text("Basic Usage:");
        ImGui::Separator();
        
        if (ImGui::Auto::BeginSection("Simple Types")) {
            ImGui::Auto::Property("Integer", intValue);
            ImGui::Auto::Property("Float", floatValue);
            ImGui::Auto::Property("Boolean", boolValue);
            ImGui::Auto::Property("String", stringValue);
            ImGui::Auto::EndSection();
        }
        
        // Structure example with nested sections
        if (ImGui::Auto::BeginSection("Nested Structures")) {
            static struct {
                std::string name = "Player";
                struct {
                    float x = 100.0f;
                    float y = 200.0f;
                    float rotation = 45.0f;
                } position;
                struct {
                    float health = 100.0f;
                    int mana = 50;
                    int level = 5;
                } stats;
            } player;
            
            ImGui::Auto::Property("Name", player.name);
            
            if (ImGui::Auto::BeginSection("Position")) {
                ImGui::Auto::Property("X", player.position.x, 0.1f);
                ImGui::Auto::Property("Y", player.position.y, 0.1f);
                ImGui::Auto::Property("Rotation", player.position.rotation, 1.0f);
                ImGui::Auto::EndSection();
            }
            
            if (ImGui::Auto::BeginSection("Stats")) {
                ImGui::Auto::Property("Health", player.stats.health);
                ImGui::Auto::Property("Mana", player.stats.mana);
                ImGui::Auto::Property("Level", player.stats.level);
                ImGui::Auto::EndSection();
            }
            
            ImGui::Auto::EndSection();
        }
        
        ImGui::End();
    }
}

} // namespace AutoDemo
} // namespace ImGui