#include "ConfigurationSchema.h"

namespace gb2d {

namespace {
[[nodiscard]] constexpr ConfigFieldFlags toggleFlag(ConfigFieldFlags flags, ConfigFieldFlags flag, bool enabled) noexcept {
    return enabled ? (flags | flag) : static_cast<ConfigFieldFlags>(static_cast<std::uint8_t>(flags) & ~static_cast<std::uint8_t>(flag));
}

[[nodiscard]] constexpr ConfigSectionFlags toggleFlag(ConfigSectionFlags flags, ConfigSectionFlags flag, bool enabled) noexcept {
    return enabled ? (flags | flag) : static_cast<ConfigSectionFlags>(static_cast<std::uint8_t>(flags) & ~static_cast<std::uint8_t>(flag));
}
} // namespace

ConfigFieldBuilder::ConfigFieldBuilder(std::string id, ConfigFieldType type) noexcept {
    desc_.id = std::move(id);
    desc_.type = type;
}

ConfigFieldBuilder& ConfigFieldBuilder::label(std::string value) noexcept {
    desc_.label = std::move(value);
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::description(std::string value) noexcept {
    desc_.description = std::move(value);
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::defaultValue(const ConfigValue& value) noexcept {
    desc_.defaultValue = value;
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::defaultValue(ConfigValue&& value) noexcept {
    desc_.defaultValue = std::move(value);
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::defaultBool(bool value) noexcept {
    desc_.defaultValue = value;
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::defaultInt(std::int64_t value) noexcept {
    desc_.defaultValue = value;
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::defaultFloat(double value) noexcept {
    desc_.defaultValue = value;
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::defaultString(std::string value) noexcept {
    desc_.defaultValue = std::move(value);
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::defaultStringList(std::vector<std::string> value) noexcept {
    desc_.defaultValue = std::move(value);
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::defaultJson(const nlohmann::json& value) noexcept {
    desc_.defaultValue = value;
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::defaultJson(nlohmann::json&& value) noexcept {
    desc_.defaultValue = std::move(value);
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::min(double value) noexcept {
    desc_.validation.min = value;
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::max(double value) noexcept {
    desc_.validation.max = value;
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::regex(std::string pattern) noexcept {
    desc_.validation.regex = std::move(pattern);
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::enumValues(std::vector<std::string> values) noexcept {
    desc_.validation.enumValues = std::move(values);
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::addEnumValue(std::string value) {
    desc_.validation.enumValues.push_back(std::move(value));
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::pathMode(std::string mode) noexcept {
    desc_.validation.pathMode = std::move(mode);
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::step(double value) noexcept {
    desc_.validation.step = value;
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::precision(int value) noexcept {
    desc_.validation.precision = value;
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::advanced(bool enabled) noexcept {
    desc_.flags = toggleFlag(desc_.flags, ConfigFieldFlags::Advanced, enabled);
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::experimental(bool enabled) noexcept {
    desc_.flags = toggleFlag(desc_.flags, ConfigFieldFlags::Experimental, enabled);
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::hidden(bool enabled) noexcept {
    desc_.flags = toggleFlag(desc_.flags, ConfigFieldFlags::Hidden, enabled);
    return *this;
}

ConfigFieldBuilder& ConfigFieldBuilder::uiHint(std::string key, nlohmann::json value) {
    desc_.uiHints.emplace(std::move(key), std::move(value));
    return *this;
}

ConfigFieldDesc ConfigFieldBuilder::build() && noexcept {
    return std::move(desc_);
}

ConfigSectionBuilder::ConfigSectionBuilder(std::string id) noexcept {
    desc_.id = std::move(id);
}

ConfigSectionBuilder& ConfigSectionBuilder::label(std::string value) noexcept {
    desc_.label = std::move(value);
    return *this;
}

ConfigSectionBuilder& ConfigSectionBuilder::description(std::string value) noexcept {
    desc_.description = std::move(value);
    return *this;
}

ConfigSectionBuilder& ConfigSectionBuilder::advanced(bool enabled) noexcept {
    desc_.flags = toggleFlag(desc_.flags, ConfigSectionFlags::Advanced, enabled);
    return *this;
}

ConfigSectionBuilder& ConfigSectionBuilder::experimental(bool enabled) noexcept {
    desc_.flags = toggleFlag(desc_.flags, ConfigSectionFlags::Experimental, enabled);
    return *this;
}

ConfigSectionBuilder& ConfigSectionBuilder::hidden(bool enabled) noexcept {
    desc_.flags = toggleFlag(desc_.flags, ConfigSectionFlags::Hidden, enabled);
    return *this;
}

ConfigSectionBuilder& ConfigSectionBuilder::field(std::string id, ConfigFieldType type, const FieldInit& init) {
    ConfigFieldBuilder builder(std::move(id), type);
    if (init) {
        init(builder);
    }
    desc_.fields.emplace_back(std::move(builder).build());
    return *this;
}

ConfigSectionBuilder& ConfigSectionBuilder::section(std::string id, const SectionInit& init) {
    ConfigSectionBuilder builder(std::move(id));
    if (init) {
        init(builder);
    }
    desc_.children.emplace_back(std::move(builder).build());
    return *this;
}

ConfigSectionDesc ConfigSectionBuilder::build() && noexcept {
    return std::move(desc_);
}

ConfigurationSchemaBuilder& ConfigurationSchemaBuilder::section(std::string id, const SectionInit& init) {
    ConfigSectionBuilder builder(std::move(id));
    if (init) {
        init(builder);
    }
    sections_.emplace_back(std::move(builder).build());
    return *this;
}

ConfigurationSchema ConfigurationSchemaBuilder::build() && noexcept {
    ConfigurationSchema schema;
    schema.sections = std::move(sections_);
    return schema;
}

const ConfigSectionDesc* ConfigurationSchema::findSection(std::string_view id) const noexcept {
    for (const auto& section : sections) {
        if (section.id == id) {
            return &section;
        }
        if (const auto* found = findSectionRecursive(section, id)) {
            return found;
        }
    }
    return nullptr;
}

const ConfigSectionDesc* ConfigurationSchema::findSectionRecursive(const ConfigSectionDesc& section, std::string_view id) const noexcept {
    for (const auto& child : section.children) {
        if (child.id == id) {
            return &child;
        }
        if (const auto* found = findSectionRecursive(child, id)) {
            return found;
        }
    }
    return nullptr;
}

const ConfigFieldDesc* ConfigurationSchema::findField(std::string_view id) const noexcept {
    for (const auto& section : sections) {
        if (const auto* field = findFieldRecursive(section, id)) {
            return field;
        }
    }
    return nullptr;
}

const ConfigFieldDesc* ConfigurationSchema::findFieldRecursive(const ConfigSectionDesc& section, std::string_view id) const noexcept {
    for (const auto& field : section.fields) {
        if (field.id == id) {
            return &field;
        }
    }
    for (const auto& child : section.children) {
        if (const auto* field = findFieldRecursive(child, id)) {
            return field;
        }
    }
    return nullptr;
}

} // namespace gb2d
