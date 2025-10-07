#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <nlohmann/json_fwd.hpp>

namespace gb2d {
class ConfigurationManager {
public:
    static void loadOrDefault();
    static bool load();
    static bool save();

    struct OnConfigReloadedHook;

    static bool getBool(const std::string& key, bool defaultValue);
    static int64_t getInt(const std::string& key, int64_t defaultValue);
    static double getDouble(const std::string& key, double defaultValue);
    static std::string getString(const std::string& key, const std::string& defaultValue);
    static std::vector<std::string> getStringList(const std::string& key, const std::vector<std::string>& defaultValue);

    static void set(const std::string& key, bool value);
    static void set(const std::string& key, int64_t value);
    static void set(const std::string& key, double value);
    static void set(const std::string& key, const std::string& value);
    static void set(const std::string& key, const std::vector<std::string>& value);
    static void setJson(const std::string& key, const nlohmann::json& value);

    // Change notifications: called after successful save(). Returns subscription id.
    static int subscribeOnChange(const std::function<void()>& cb);
    static void unsubscribe(int id);

    // Export current configuration as compact JSON for diagnostics.
    static std::string exportCompact();

    // Expose the raw JSON document for read-only consumers (e.g., hotkey loader).
    [[nodiscard]] static const nlohmann::json& raw();
    static void pushReloadHook(const OnConfigReloadedHook& hook);
    
        struct OnConfigReloadedHook {
            std::string name;
            std::function<void()> callback;
        };
};
}
