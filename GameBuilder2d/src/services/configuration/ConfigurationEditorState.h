#pragma once

#include "ConfigurationSchema.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <optional>

#include <nlohmann/json_fwd.hpp>

namespace gb2d {

struct ConfigFieldState {
    const ConfigFieldDesc* descriptor{nullptr};
    ConfigValue originalValue{};
    ConfigValue currentValue{};
    ConfigValue defaultValue{};
    FieldValidationState validation{};
    std::optional<ConfigValue> undoValue{};
    std::optional<ConfigValue> redoValue{};

    [[nodiscard]] bool isDirty() const;
    [[nodiscard]] bool isValid() const { return validation.valid; }
    [[nodiscard]] bool canUndo() const noexcept;
    [[nodiscard]] bool canRedo() const noexcept;

    bool setValue(ConfigValue value);
    bool revertToOriginal();
    bool revertToDefault();
    bool undo();
    bool redo();

    void setValidation(FieldValidationState state);
    void clearValidation();
    void clearHistory();
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

struct ConfigUnknownState {
    nlohmann::json original{nlohmann::json::object()};
    nlohmann::json current{nlohmann::json::object()};
    FieldValidationState validation{};

    [[nodiscard]] bool isDirty() const { return original != current; }
    [[nodiscard]] bool isValid() const { return validation.valid; }
    void resetValidation() {
        validation.valid = true;
        validation.message.clear();
    }
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

    bool undoField(std::string_view id);
    bool redoField(std::string_view id);
    bool undoSection(std::string_view id);
    bool redoSection(std::string_view id);
    void undoAll();
    void redoAll();

    [[nodiscard]] const std::vector<ConfigSectionState>& sections() const noexcept { return sections_; }
    [[nodiscard]] const nlohmann::json& unknownEntries() const noexcept { return unknown_.current; }
    [[nodiscard]] bool hasUnknownEntries() const noexcept;
    [[nodiscard]] bool isUnknownDirty() const noexcept;
    [[nodiscard]] const FieldValidationState& unknownValidation() const noexcept { return unknown_.validation; }
    void setUnknownEntries(nlohmann::json value);
    void setUnknownValidation(FieldValidationState state);
    void clearUnknownValidation();
    void revertUnknownEntries();

    [[nodiscard]] nlohmann::json toJson() const;
    void commitToCurrent();

private:
    std::vector<ConfigSectionState> sections_{};
    std::unordered_map<std::string, ConfigFieldState*> fieldIndex_{};
    std::unordered_map<std::string, ConfigSectionState*> sectionIndex_{};
    ConfigUnknownState unknown_{};

    void rebuildIndices();
    void indexSection(ConfigSectionState& section);
};

} // namespace gb2d
