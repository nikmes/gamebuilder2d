#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <nlohmann/json.hpp>

namespace gb2d {

enum class ConfigFieldType : std::uint8_t {
    Boolean,
    Integer,
    Float,
    Enum,
    String,
    Path,
    List,
    JsonBlob,
    Hotkeys,
};

enum class ConfigFieldFlags : std::uint8_t {
    None = 0,
    Advanced = 1 << 0,
    Experimental = 1 << 1,
    Hidden = 1 << 2,
};

[[nodiscard]] constexpr ConfigFieldFlags operator|(ConfigFieldFlags lhs, ConfigFieldFlags rhs) noexcept {
    return static_cast<ConfigFieldFlags>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

[[nodiscard]] constexpr ConfigFieldFlags operator&(ConfigFieldFlags lhs, ConfigFieldFlags rhs) noexcept {
    return static_cast<ConfigFieldFlags>(static_cast<std::uint8_t>(lhs) & static_cast<std::uint8_t>(rhs));
}

[[nodiscard]] constexpr ConfigFieldFlags& operator|=(ConfigFieldFlags& lhs, ConfigFieldFlags rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

[[nodiscard]] constexpr ConfigFieldFlags& operator&=(ConfigFieldFlags& lhs, ConfigFieldFlags rhs) noexcept {
    lhs = lhs & rhs;
    return lhs;
}

enum class ConfigSectionFlags : std::uint8_t {
    None = 0,
    Advanced = 1 << 0,
    Experimental = 1 << 1,
    Hidden = 1 << 2,
};

[[nodiscard]] constexpr ConfigSectionFlags operator|(ConfigSectionFlags lhs, ConfigSectionFlags rhs) noexcept {
    return static_cast<ConfigSectionFlags>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

[[nodiscard]] constexpr ConfigSectionFlags operator&(ConfigSectionFlags lhs, ConfigSectionFlags rhs) noexcept {
    return static_cast<ConfigSectionFlags>(static_cast<std::uint8_t>(lhs) & static_cast<std::uint8_t>(rhs));
}

[[nodiscard]] constexpr ConfigSectionFlags& operator|=(ConfigSectionFlags& lhs, ConfigSectionFlags rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

[[nodiscard]] constexpr ConfigSectionFlags& operator&=(ConfigSectionFlags& lhs, ConfigSectionFlags rhs) noexcept {
    lhs = lhs & rhs;
    return lhs;
}

struct ConfigValidation {
    std::optional<double> min;
    std::optional<double> max;
    std::optional<std::string> regex;
    std::vector<std::string> enumValues;
    std::optional<std::string> pathMode;
    std::optional<double> step;
    std::optional<int> precision;
};

enum class ValidationPhase {
    OnEdit,
    OnApply,
};

struct FieldValidationState {
    bool valid{true};
    std::string message;
};

using ConfigValue = std::variant<std::monostate, bool, std::int64_t, double, std::string, std::vector<std::string>, nlohmann::json>;

struct ConfigFieldDesc {
    std::string id;
    ConfigFieldType type{ConfigFieldType::String};
    std::string label;
    std::string description;
    ConfigValue defaultValue;
    ConfigValidation validation;
    ConfigFieldFlags flags{ConfigFieldFlags::None};
    std::unordered_map<std::string, nlohmann::json> uiHints;
};

struct ConfigSectionDesc {
    std::string id;
    std::string label;
    std::string description;
    std::vector<ConfigFieldDesc> fields;
    std::vector<ConfigSectionDesc> children;
    ConfigSectionFlags flags{ConfigSectionFlags::None};
};

struct ConfigurationSchema {
    std::vector<ConfigSectionDesc> sections;

    [[nodiscard]] const ConfigSectionDesc* findSection(std::string_view id) const noexcept;
    [[nodiscard]] const ConfigFieldDesc* findField(std::string_view id) const noexcept;

    template <typename Callback>
    void forEachField(const Callback& cb) const {
        for (const auto& section : sections) {
            forEachFieldRecursive(section, cb);
        }
    }

private:
    [[nodiscard]] const ConfigSectionDesc* findSectionRecursive(const ConfigSectionDesc& section, std::string_view id) const noexcept;
    [[nodiscard]] const ConfigFieldDesc* findFieldRecursive(const ConfigSectionDesc& section, std::string_view id) const noexcept;

    template <typename Callback>
    void forEachFieldRecursive(const ConfigSectionDesc& section, const Callback& cb) const {
        for (const auto& field : section.fields) {
            cb(field, section);
        }
        for (const auto& child : section.children) {
            forEachFieldRecursive(child, cb);
        }
    }
};

class ConfigFieldBuilder {
public:
    ConfigFieldBuilder(std::string id, ConfigFieldType type) noexcept;

    ConfigFieldBuilder& label(std::string value) noexcept;
    ConfigFieldBuilder& description(std::string value) noexcept;

    ConfigFieldBuilder& defaultValue(const ConfigValue& value) noexcept;
    ConfigFieldBuilder& defaultValue(ConfigValue&& value) noexcept;
    ConfigFieldBuilder& defaultBool(bool value) noexcept;
    ConfigFieldBuilder& defaultInt(std::int64_t value) noexcept;
    ConfigFieldBuilder& defaultFloat(double value) noexcept;
    ConfigFieldBuilder& defaultString(std::string value) noexcept;
    ConfigFieldBuilder& defaultStringList(std::vector<std::string> value) noexcept;
    ConfigFieldBuilder& defaultJson(const nlohmann::json& value) noexcept;
    ConfigFieldBuilder& defaultJson(nlohmann::json&& value) noexcept;

    ConfigFieldBuilder& min(double value) noexcept;
    ConfigFieldBuilder& max(double value) noexcept;
    ConfigFieldBuilder& regex(std::string pattern) noexcept;
    ConfigFieldBuilder& enumValues(std::vector<std::string> values) noexcept;
    ConfigFieldBuilder& addEnumValue(std::string value);
    ConfigFieldBuilder& pathMode(std::string mode) noexcept;
    ConfigFieldBuilder& step(double value) noexcept;
    ConfigFieldBuilder& precision(int value) noexcept;

    ConfigFieldBuilder& advanced(bool enabled = true) noexcept;
    ConfigFieldBuilder& experimental(bool enabled = true) noexcept;
    ConfigFieldBuilder& hidden(bool enabled = true) noexcept;

    ConfigFieldBuilder& uiHint(std::string key, nlohmann::json value);

    [[nodiscard]] ConfigFieldDesc build() && noexcept;

private:
    ConfigFieldDesc desc_;
};

class ConfigSectionBuilder {
public:
    explicit ConfigSectionBuilder(std::string id) noexcept;

    ConfigSectionBuilder& label(std::string value) noexcept;
    ConfigSectionBuilder& description(std::string value) noexcept;

    ConfigSectionBuilder& advanced(bool enabled = true) noexcept;
    ConfigSectionBuilder& experimental(bool enabled = true) noexcept;
    ConfigSectionBuilder& hidden(bool enabled = true) noexcept;

    using FieldInit = std::function<void(ConfigFieldBuilder&)>;
    using SectionInit = std::function<void(ConfigSectionBuilder&)>;

    ConfigSectionBuilder& field(std::string id, ConfigFieldType type, const FieldInit& init = {});
    ConfigSectionBuilder& section(std::string id, const SectionInit& init = {});

    [[nodiscard]] ConfigSectionDesc build() && noexcept;

private:
    ConfigSectionDesc desc_;
};

class ConfigurationSchemaBuilder {
public:
    using SectionInit = ConfigSectionBuilder::SectionInit;

    ConfigurationSchemaBuilder& section(std::string id, const SectionInit& init = {});

    [[nodiscard]] ConfigurationSchema build() && noexcept;

private:
    std::vector<ConfigSectionDesc> sections_;
};

} // namespace gb2d
