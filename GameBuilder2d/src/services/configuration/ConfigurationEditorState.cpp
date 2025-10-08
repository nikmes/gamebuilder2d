#include "ConfigurationEditorState.h"

#include "ConfigurationManager.h"

#include <algorithm>
#include <cassert>
#include <utility>

#include <nlohmann/json.hpp>

namespace gb2d {

namespace {

using nlohmann::json;

bool eraseJsonPath(json& target, std::string_view path) {
    if (!target.is_object() || path.empty()) {
        return false;
    }

    json* current = &target;
    std::vector<std::pair<json*, std::string>> stack;
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t dot = path.find('.', start);
        const std::size_t length = dot == std::string_view::npos ? path.size() - start : dot - start;
        std::string key(path.substr(start, length));
        if (!current->is_object()) {
            return false;
        }
        auto it = current->find(key);
        if (it == current->end()) {
            return false;
        }
        stack.emplace_back(current, key);
        current = &(*it);
        if (dot == std::string_view::npos) {
            break;
        }
        start = dot + 1;
    }

    if (stack.empty()) {
        return false;
    }

    auto& [leafParent, leafKey] = stack.back();
    if (!leafParent->is_object()) {
        return false;
    }
    auto leafIt = leafParent->find(leafKey);
    if (leafIt == leafParent->end()) {
        return false;
    }
    leafParent->erase(leafIt);

    for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
        json* parent = it->first;
        const std::string& key = it->second;
        if (!parent->is_object()) {
            continue;
        }
        auto childIt = parent->find(key);
        if (childIt == parent->end()) {
            continue;
        }
        if ((childIt->is_object() || childIt->is_array()) && childIt->empty()) {
            parent->erase(childIt);
        }
    }
    return true;
}

void pruneEmptyContainers(json& value) {
    if (value.is_object()) {
        for (auto it = value.begin(); it != value.end();) {
            pruneEmptyContainers(*it);
            if ((it->is_object() || it->is_array()) && it->empty()) {
                it = value.erase(it);
            } else {
                ++it;
            }
        }
    } else if (value.is_array()) {
        for (auto it = value.begin(); it != value.end();) {
            pruneEmptyContainers(*it);
            if ((it->is_object() || it->is_array()) && it->empty()) {
                it = value.erase(it);
            } else {
                ++it;
            }
        }
    }
}

json buildUnknownEntries(const json& document, const ConfigurationSchema& schema) {
    json unknown = document.is_object() ? document : json::object();
    schema.forEachField([&](const ConfigFieldDesc& field, const ConfigSectionDesc&) {
        (void)eraseJsonPath(unknown, field.id);
    });
    pruneEmptyContainers(unknown);
    if (!unknown.is_object() || unknown.empty()) {
        return json::object();
    }
    return unknown;
}

json& ensureJsonPath(json& target, std::string_view path) {
    if (!target.is_object()) {
        target = json::object();
    }

    json* current = &target;
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t dot = path.find('.', start);
        const std::size_t length = dot == std::string_view::npos ? path.size() - start : dot - start;
        std::string key(path.substr(start, length));
        if (!current->is_object()) {
            *current = json::object();
        }
        current = &((*current)[key]);
        if (dot == std::string_view::npos) {
            break;
        }
        start = dot + 1;
    }
    return *current;
}

struct ConfigValueToJson {
    json operator()(std::monostate) const { return json(nullptr); }
    json operator()(bool value) const { return json(value); }
    json operator()(std::int64_t value) const { return json(value); }
    json operator()(double value) const { return json(value); }
    json operator()(const std::string& value) const { return json(value); }
    json operator()(const std::vector<std::string>& value) const { return json(value); }
    json operator()(const json& value) const { return value; }
};

void writeSectionToJson(const ConfigSectionState& section, json& document) {
    for (const auto& field : section.fields) {
        if (!field.descriptor) {
            continue;
        }
        json value = std::visit(ConfigValueToJson{}, field.currentValue);
        json& slot = ensureJsonPath(document, field.descriptor->id);
        slot = std::move(value);
    }
    for (const auto& child : section.children) {
        writeSectionToJson(child, document);
    }
}

struct ConfigValueEquality {
    template <typename T>
    bool operator()(const T& lhs, const ConfigValue& rhs) const {
        if (const auto* value = std::get_if<T>(&rhs)) {
            return *value == lhs;
        }
        return false;
    }

    bool operator()(const std::monostate&, const ConfigValue& rhs) const {
        return std::holds_alternative<std::monostate>(rhs);
    }

    bool operator()(const double& lhs, const ConfigValue& rhs) const {
        if (const auto* value = std::get_if<double>(&rhs)) {
            return *value == lhs;
        }
        if (const auto* intValue = std::get_if<std::int64_t>(&rhs)) {
            return static_cast<double>(*intValue) == lhs;
        }
        return false;
    }

    bool operator()(const std::int64_t& lhs, const ConfigValue& rhs) const {
        if (const auto* value = std::get_if<std::int64_t>(&rhs)) {
            return *value == lhs;
        }
        if (const auto* doubleValue = std::get_if<double>(&rhs)) {
            return lhs == static_cast<std::int64_t>(*doubleValue);
        }
        return false;
    }

    bool operator()(const std::vector<std::string>& lhs, const ConfigValue& rhs) const {
        if (const auto* value = std::get_if<std::vector<std::string>>(&rhs)) {
            return *value == lhs;
        }
        return false;
    }

    bool operator()(const nlohmann::json& lhs, const ConfigValue& rhs) const {
        if (const auto* value = std::get_if<nlohmann::json>(&rhs)) {
            return *value == lhs;
        }
        return false;
    }
};

bool configValuesEqual(const ConfigValue& lhs, const ConfigValue& rhs) {
    return std::visit(
        [&](const auto& value) {
            return ConfigValueEquality{}(value, rhs);
        },
        lhs);
}

const json* findJsonByPath(const json& document, std::string_view path) {
    if (path.empty()) {
        return nullptr;
    }

    const json* current = &document;
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t dot = path.find('.', start);
        const std::size_t length = dot == std::string_view::npos ? path.size() - start : dot - start;
        const std::string key(path.substr(start, length));
        if (!current->is_object()) {
            return nullptr;
        }
        auto it = current->find(key);
        if (it == current->end()) {
            return nullptr;
        }
        if (dot == std::string_view::npos) {
            return &(*it);
        }
        current = &(*it);
        start = dot + 1;
    }

    return current;
}

ConfigValue defaultForField(const ConfigFieldDesc& field) {
    return field.defaultValue;
}

ConfigValue valueFromJson(const json& document, const ConfigFieldDesc& field) {
    const json* value = findJsonByPath(document, field.id);
    if (!value) {
        return defaultForField(field);
    }

    switch (field.type) {
    case ConfigFieldType::Boolean:
        if (value->is_boolean()) {
            return value->get<bool>();
        }
        break;
    case ConfigFieldType::Integer:
        if (value->is_number_integer()) {
            return value->get<std::int64_t>();
        }
        if (value->is_number()) {
            return static_cast<std::int64_t>(value->get<double>());
        }
        break;
    case ConfigFieldType::Float:
        if (value->is_number()) {
            return value->get<double>();
        }
        break;
    case ConfigFieldType::Enum:
    case ConfigFieldType::String:
    case ConfigFieldType::Path:
        if (value->is_string()) {
            return value->get<std::string>();
        }
        break;
    case ConfigFieldType::List:
        if (value->is_array()) {
            std::vector<std::string> out;
            out.reserve(value->size());
            for (const auto& entry : *value) {
                if (entry.is_string()) {
                    out.push_back(entry.get<std::string>());
                }
            }
            return out;
        }
        break;
    case ConfigFieldType::JsonBlob:
    case ConfigFieldType::Hotkeys:
        return *value;
    }

    return defaultForField(field);
}

bool coerceValueToFieldType(const ConfigFieldDesc& field, ConfigValue& value) {
    switch (field.type) {
    case ConfigFieldType::Boolean:
        if (std::holds_alternative<bool>(value)) {
            return true;
        }
        return false;
    case ConfigFieldType::Integer:
        if (auto* intValue = std::get_if<std::int64_t>(&value)) {
            return true;
        }
        if (auto* doubleValue = std::get_if<double>(&value)) {
            value = static_cast<std::int64_t>(*doubleValue);
            return true;
        }
        return false;
    case ConfigFieldType::Float:
        if (std::holds_alternative<double>(value)) {
            return true;
        }
        if (const auto* intValue = std::get_if<std::int64_t>(&value)) {
            value = static_cast<double>(*intValue);
            return true;
        }
        return false;
    case ConfigFieldType::Enum:
    case ConfigFieldType::String:
    case ConfigFieldType::Path:
        return std::holds_alternative<std::string>(value);
    case ConfigFieldType::List:
        return std::holds_alternative<std::vector<std::string>>(value);
    case ConfigFieldType::JsonBlob:
    case ConfigFieldType::Hotkeys:
        return std::holds_alternative<nlohmann::json>(value);
    }
    return false;
}

ConfigFieldState buildFieldState(const json& document, const ConfigFieldDesc& fieldDesc) {
    ConfigFieldState state;
    state.descriptor = &fieldDesc;
    state.defaultValue = defaultForField(fieldDesc);
    state.originalValue = valueFromJson(document, fieldDesc);
    if (std::holds_alternative<std::monostate>(state.originalValue) && !std::holds_alternative<std::monostate>(state.defaultValue)) {
        state.originalValue = state.defaultValue;
    }
    state.currentValue = state.originalValue;
    state.validation = FieldValidationState{};
    state.clearHistory();
    return state;
}

ConfigSectionState buildSectionState(const json& document, const ConfigSectionDesc& sectionDesc) {
    ConfigSectionState state;
    state.descriptor = &sectionDesc;
    state.fields.reserve(sectionDesc.fields.size());
    for (const auto& fieldDesc : sectionDesc.fields) {
        state.fields.emplace_back(buildFieldState(document, fieldDesc));
    }
    state.children.reserve(sectionDesc.children.size());
    for (const auto& child : sectionDesc.children) {
        state.children.emplace_back(buildSectionState(document, child));
    }
    return state;
}

bool revertSectionFieldsToOriginal(ConfigSectionState& section) {
    bool changed = false;
    for (auto& field : section.fields) {
        changed |= field.revertToOriginal();
    }
    for (auto& child : section.children) {
        changed |= revertSectionFieldsToOriginal(child);
    }
    return changed;
}

bool revertSectionFieldsToDefault(ConfigSectionState& section) {
    bool changed = false;
    for (auto& field : section.fields) {
        changed |= field.revertToDefault();
    }
    for (auto& child : section.children) {
        changed |= revertSectionFieldsToDefault(child);
    }
    return changed;
}

void commitSectionState(ConfigSectionState& section) {
    for (auto& field : section.fields) {
        field.originalValue = field.currentValue;
        field.clearHistory();
    }
    for (auto& child : section.children) {
        commitSectionState(child);
    }
}

bool undoSectionHistory(ConfigSectionState& section) {
    bool changed = false;
    for (auto& field : section.fields) {
        changed |= field.undo();
    }
    for (auto& child : section.children) {
        changed |= undoSectionHistory(child);
    }
    return changed;
}

bool redoSectionHistory(ConfigSectionState& section) {
    bool changed = false;
    for (auto& field : section.fields) {
        changed |= field.redo();
    }
    for (auto& child : section.children) {
        changed |= redoSectionHistory(child);
    }
    return changed;
}

} // namespace

bool ConfigFieldState::isDirty() const {
    return !configValuesEqual(currentValue, originalValue);
}

bool ConfigFieldState::setValue(ConfigValue value) {
    if (!descriptor) {
        return false;
    }
    if (!coerceValueToFieldType(*descriptor, value)) {
        return false;
    }
    if (configValuesEqual(currentValue, value)) {
        validation.message.clear();
        validation.valid = true;
        return true;
    }
    undoValue = currentValue;
    redoValue.reset();
    currentValue = std::move(value);
    validation.message.clear();
    validation.valid = true;
    return true;
}

bool ConfigFieldState::revertToOriginal() {
    if (configValuesEqual(currentValue, originalValue)) {
        return false;
    }
    undoValue = currentValue;
    redoValue.reset();
    currentValue = originalValue;
    validation.message.clear();
    validation.valid = true;
    return true;
}

bool ConfigFieldState::revertToDefault() {
    if (std::holds_alternative<std::monostate>(defaultValue)) {
        return revertToOriginal();
    }
    if (configValuesEqual(currentValue, defaultValue)) {
        return false;
    }
    undoValue = currentValue;
    redoValue.reset();
    currentValue = defaultValue;
    validation.message.clear();
    validation.valid = true;
    return true;
}

void ConfigFieldState::setValidation(FieldValidationState state) {
    validation = std::move(state);
}

void ConfigFieldState::clearValidation() {
    validation.valid = true;
    validation.message.clear();
}

bool ConfigFieldState::canUndo() const noexcept {
    return undoValue.has_value();
}

bool ConfigFieldState::canRedo() const noexcept {
    return redoValue.has_value();
}

bool ConfigFieldState::undo() {
    if (!undoValue) {
        return false;
    }
    ConfigValue previous = currentValue;
    currentValue = std::move(*undoValue);
    redoValue = std::move(previous);
    undoValue.reset();
    clearValidation();
    return true;
}

bool ConfigFieldState::redo() {
    if (!redoValue) {
        return false;
    }
    undoValue = currentValue;
    currentValue = std::move(*redoValue);
    redoValue.reset();
    clearValidation();
    return true;
}

void ConfigFieldState::clearHistory() {
    undoValue.reset();
    redoValue.reset();
}

bool ConfigSectionState::isDirty() const {
    if (std::any_of(fields.begin(), fields.end(), [](const ConfigFieldState& field) { return field.isDirty(); })) {
        return true;
    }
    return std::any_of(children.begin(), children.end(), [](const ConfigSectionState& child) { return child.isDirty(); });
}

bool ConfigSectionState::hasInvalidFields() const {
    if (std::any_of(fields.begin(), fields.end(), [](const ConfigFieldState& field) { return !field.isValid(); })) {
        return true;
    }
    return std::any_of(children.begin(), children.end(), [](const ConfigSectionState& child) { return child.hasInvalidFields(); });
}

std::size_t ConfigSectionState::dirtyFieldCount() const {
    std::size_t count = 0;
    for (const auto& field : fields) {
        if (field.isDirty()) {
            ++count;
        }
    }
    for (const auto& child : children) {
        count += child.dirtyFieldCount();
    }
    return count;
}

std::size_t ConfigSectionState::invalidFieldCount() const {
    std::size_t count = 0;
    for (const auto& field : fields) {
        if (!field.isValid()) {
            ++count;
        }
    }
    for (const auto& child : children) {
        count += child.invalidFieldCount();
    }
    return count;
}

bool ConfigSectionState::revertToOriginal() {
    return revertSectionFieldsToOriginal(*this);
}

bool ConfigSectionState::revertToDefaults() {
    return revertSectionFieldsToDefault(*this);
}

ConfigurationEditorState ConfigurationEditorState::fromCurrent() {
    return fromJson(ConfigurationManager::raw(), ConfigurationManager::schema());
}

ConfigurationEditorState ConfigurationEditorState::fromJson(const nlohmann::json& document,
                                                             const ConfigurationSchema& schema) {
    ConfigurationEditorState state;
    state.sections_.reserve(schema.sections.size());
    for (const auto& section : schema.sections) {
        state.sections_.emplace_back(buildSectionState(document, section));
    }
    state.unknown_.original = buildUnknownEntries(document, schema);
    state.unknown_.current = state.unknown_.original;
    state.unknown_.resetValidation();
    state.rebuildIndices();
    return state;
}

bool ConfigurationEditorState::isDirty() const {
    if (std::any_of(sections_.begin(), sections_.end(), [](const ConfigSectionState& section) { return section.isDirty(); })) {
        return true;
    }
    return unknown_.isDirty();
}

bool ConfigurationEditorState::hasInvalidFields() const {
    if (std::any_of(sections_.begin(), sections_.end(), [](const ConfigSectionState& section) { return section.hasInvalidFields(); })) {
        return true;
    }
    return unknown_.validation.valid == false;
}

ConfigFieldState* ConfigurationEditorState::field(std::string_view id) noexcept {
    const std::string key(id);
    auto it = fieldIndex_.find(key);
    if (it == fieldIndex_.end()) {
        return nullptr;
    }
    return it->second;
}

const ConfigFieldState* ConfigurationEditorState::field(std::string_view id) const noexcept {
    const std::string key(id);
    auto it = fieldIndex_.find(key);
    if (it == fieldIndex_.end()) {
        return nullptr;
    }
    return it->second;
}

ConfigSectionState* ConfigurationEditorState::section(std::string_view id) noexcept {
    const std::string key(id);
    auto it = sectionIndex_.find(key);
    if (it == sectionIndex_.end()) {
        return nullptr;
    }
    return it->second;
}

const ConfigSectionState* ConfigurationEditorState::section(std::string_view id) const noexcept {
    const std::string key(id);
    auto it = sectionIndex_.find(key);
    if (it == sectionIndex_.end()) {
        return nullptr;
    }
    return it->second;
}

bool ConfigurationEditorState::setFieldValue(std::string_view id, ConfigValue value) {
    if (auto* state = field(id)) {
        return state->setValue(std::move(value));
    }
    return false;
}

bool ConfigurationEditorState::revertField(std::string_view id) {
    if (auto* state = field(id)) {
        return state->revertToOriginal();
    }
    return false;
}

bool ConfigurationEditorState::revertFieldToDefault(std::string_view id) {
    if (auto* state = field(id)) {
        return state->revertToDefault();
    }
    return false;
}

bool ConfigurationEditorState::revertSection(std::string_view id) {
    if (auto* s = section(id)) {
        return s->revertToOriginal();
    }
    return false;
}

bool ConfigurationEditorState::revertSectionToDefaults(std::string_view id) {
    if (auto* s = section(id)) {
        return s->revertToDefaults();
    }
    return false;
}

void ConfigurationEditorState::revertAll() {
    for (auto& section : sections_) {
        section.revertToOriginal();
    }
    revertUnknownEntries();
}

void ConfigurationEditorState::revertAllToDefaults() {
    for (auto& section : sections_) {
        section.revertToDefaults();
    }
    // Unknown entries do not have schema defaults; revert to original state.
    revertUnknownEntries();
}

bool ConfigurationEditorState::validateField(std::string_view id, ValidationPhase phase) {
    auto* state = field(id);
    if (!state || !state->descriptor) {
        return false;
    }
    state->setValidation(ConfigurationManager::validateFieldValue(*state->descriptor, state->currentValue, phase));
    return state->validation.valid;
}

bool ConfigurationEditorState::validateAll(ValidationPhase phase) {
    bool allValid = true;
    for (auto& section : sections_) {
        std::vector<ConfigSectionState*> stack;
        stack.push_back(&section);
        while (!stack.empty()) {
            ConfigSectionState* current = stack.back();
            stack.pop_back();
            for (auto& fieldState : current->fields) {
                if (fieldState.descriptor) {
                    fieldState.setValidation(ConfigurationManager::validateFieldValue(*fieldState.descriptor, fieldState.currentValue, phase));
                    allValid = allValid && fieldState.validation.valid;
                }
            }
            for (auto& child : current->children) {
                stack.push_back(&child);
            }
        }
    }
    return allValid;
}

bool ConfigurationEditorState::undoField(std::string_view id) {
    if (auto* state = field(id)) {
        return state->undo();
    }
    return false;
}

bool ConfigurationEditorState::redoField(std::string_view id) {
    if (auto* state = field(id)) {
        return state->redo();
    }
    return false;
}

bool ConfigurationEditorState::undoSection(std::string_view id) {
    if (auto* s = section(id)) {
        return undoSectionHistory(*s);
    }
    return false;
}

bool ConfigurationEditorState::redoSection(std::string_view id) {
    if (auto* s = section(id)) {
        return redoSectionHistory(*s);
    }
    return false;
}

void ConfigurationEditorState::undoAll() {
    for (auto& section : sections_) {
        undoSectionHistory(section);
    }
}

void ConfigurationEditorState::redoAll() {
    for (auto& section : sections_) {
        redoSectionHistory(section);
    }
}

bool ConfigurationEditorState::hasUnknownEntries() const noexcept {
    if (unknown_.current.is_object()) {
        return !unknown_.current.empty();
    }
    return !unknown_.current.is_null();
}

bool ConfigurationEditorState::isUnknownDirty() const noexcept {
    return unknown_.isDirty();
}

void ConfigurationEditorState::setUnknownEntries(nlohmann::json value) {
    unknown_.current = std::move(value);
    unknown_.resetValidation();
}

void ConfigurationEditorState::setUnknownValidation(FieldValidationState state) {
    unknown_.validation = std::move(state);
}

void ConfigurationEditorState::clearUnknownValidation() {
    unknown_.resetValidation();
}

void ConfigurationEditorState::revertUnknownEntries() {
    unknown_.current = unknown_.original;
    unknown_.resetValidation();
}

void ConfigurationEditorState::rebuildIndices() {
    fieldIndex_.clear();
    sectionIndex_.clear();
    for (auto& section : sections_) {
        indexSection(section);
    }
}

void ConfigurationEditorState::indexSection(ConfigSectionState& section) {
    if (section.descriptor) {
        sectionIndex_.emplace(section.descriptor->id, &section);
    }
    for (auto& field : section.fields) {
        if (field.descriptor) {
            fieldIndex_.emplace(field.descriptor->id, &field);
        }
    }
    for (auto& child : section.children) {
        indexSection(child);
    }
}

nlohmann::json ConfigurationEditorState::toJson() const {
    json document = json::object();
    if (unknown_.current.is_object()) {
        document = unknown_.current;
    } else if (!unknown_.current.is_null()) {
        document = unknown_.current;
    }

    for (const auto& section : sections_) {
        writeSectionToJson(section, document);
    }

    return document;
}

void ConfigurationEditorState::commitToCurrent() {
    for (auto& section : sections_) {
        commitSectionState(section);
    }
    unknown_.original = unknown_.current;
}

} // namespace gb2d
