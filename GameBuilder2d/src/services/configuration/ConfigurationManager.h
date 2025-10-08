#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <functional>
#include <nlohmann/json_fwd.hpp>

#include "ConfigurationSchema.h"

namespace gb2d {
class ConfigurationManager {
public:
    static void loadOrDefault();
    static bool load();
    static bool save(bool createBackup = false, bool* outBackupCreated = nullptr);
    static bool applyRuntime(const nlohmann::json& document);

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

    [[nodiscard]] static const ConfigurationSchema& schema() noexcept;
    [[nodiscard]] static const ConfigSectionDesc* findSection(std::string_view id) noexcept;
    [[nodiscard]] static const ConfigFieldDesc* findField(std::string_view id) noexcept;
    [[nodiscard]] static ConfigValue valueFor(std::string_view id, ConfigValue fallback = {}) noexcept;

    static FieldValidationState validateFieldValue(const ConfigFieldDesc& desc,
                                                   const ConfigValue& value,
                                                   ValidationPhase phase = ValidationPhase::OnEdit);
    static FieldValidationState validateFieldValue(std::string_view id,
                                                   const ConfigValue& value,
                                                   ValidationPhase phase = ValidationPhase::OnEdit);
    
    struct OnConfigReloadedHook {
        std::string name;
        std::function<void()> callback;
    };
};
}
