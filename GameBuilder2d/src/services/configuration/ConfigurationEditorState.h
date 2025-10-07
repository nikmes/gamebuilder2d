#pragma once

#include "ConfigurationSchema.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace gb2d {

struct ConfigFieldState {
    const ConfigFieldDesc* descriptor{nullptr};
    ConfigValue originalValue{};
    ConfigValue currentValue{};
    ConfigValue defaultValue{};
    FieldValidationState validation{};

    [[nodiscard]] bool isDirty() const;
    [[nodiscard]] bool isValid() const { return validation.valid; }

    bool setValue(ConfigValue value);
    bool revertToOriginal();
    bool revertToDefault();

    void setValidation(FieldValidationState state);
    void clearValidation();
};

struct ConfigSectionState {
    const ConfigSectionDesc* descriptor{nullptr};
    std::vector<ConfigFieldState> fields;
    std::vector<ConfigSectionState> children;

    [[nodiscard]] bool isDirty() const;
    [[nodiscard]] bool hasInvalidFields() const;
    [[nodiscard]] std::size_t dirtyFieldCount() const;
    [[nodiscard]] std::size_t invalidFieldCount() const;

    bool revertToOriginal();
    bool revertToDefaults();
};

class ConfigurationEditorState {
public:
    static ConfigurationEditorState fromCurrent();
    static ConfigurationEditorState fromJson(const nlohmann::json& document,
                                             const ConfigurationSchema& schema);

    [[nodiscard]] bool isDirty() const;
    [[nodiscard]] bool hasInvalidFields() const;

    ConfigFieldState* field(std::string_view id) noexcept;
    const ConfigFieldState* field(std::string_view id) const noexcept;

    ConfigSectionState* section(std::string_view id) noexcept;
    const ConfigSectionState* section(std::string_view id) const noexcept;

    bool setFieldValue(std::string_view id, ConfigValue value);
    bool revertField(std::string_view id);
    bool revertFieldToDefault(std::string_view id);

    bool revertSection(std::string_view id);
    bool revertSectionToDefaults(std::string_view id);
    void revertAll();
    void revertAllToDefaults();

    bool validateField(std::string_view id, ValidationPhase phase);
    bool validateAll(ValidationPhase phase);

    [[nodiscard]] const std::vector<ConfigSectionState>& sections() const noexcept { return sections_; }

private:
    std::vector<ConfigSectionState> sections_{};
    std::unordered_map<std::string, ConfigFieldState*> fieldIndex_{};
    std::unordered_map<std::string, ConfigSectionState*> sectionIndex_{};

    void rebuildIndices();
    void indexSection(ConfigSectionState& section);
};

} // namespace gb2d
