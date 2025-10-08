#include "ui/Windows/ConfigurationWindow.h"
#include "ui/WindowContext.h"
#include "services/configuration/ConfigurationManager.h"
#include "services/logger/LogManager.h"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace gb2d {
namespace {
constexpr float kNavigationWidth = 240.0f;
constexpr float kIndentPerLevel = 16.0f;
constexpr std::string_view kUnknownSectionId = "__unknown__";
constexpr float kUnknownEditorHeight = 220.0f;
const ImVec4 kDirtyColor = ImVec4(0.90f, 0.72f, 0.18f, 1.0f);
const ImVec4 kInvalidColor = ImVec4(0.94f, 0.33f, 0.24f, 1.0f);
const ImVec4 kSearchHighlightColor = ImVec4(0.38f, 0.69f, 1.0f, 1.0f);
constexpr const char* kRevertAllModalId = "config-revert-all";
constexpr const char* kCloseUnappliedModalId = "config-close-unapplied";
constexpr const char* kCloseUnsavedModalId = "config-close-unsaved";

template <typename... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

template <typename... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

[[nodiscard]] std::string sectionDisplayName(const ConfigSectionDesc& desc) {
    if (!desc.label.empty()) return desc.label;
    if (!desc.id.empty()) return desc.id;
    return std::string{"Section"};
}

[[nodiscard]] std::string fieldDisplayName(const ConfigFieldDesc& desc) {
    if (!desc.label.empty()) return desc.label;
    if (!desc.id.empty()) return desc.id;
    return std::string{"Field"};
}

[[nodiscard]] std::string joinStrings(const std::vector<std::string>& values, std::string_view separator = ", ") {
    std::ostringstream oss;
    bool first = true;
    for (const auto& entry : values) {
        if (!first) {
            oss << separator;
        }
        first = false;
        oss << entry;
    }
    return oss.str();
}

[[nodiscard]] std::string configValueToString(const ConfigValue& value) {
    return std::visit(
        Overloaded{
            [](std::monostate) { return std::string{}; },
            [](bool v) { return v ? std::string{"true"} : std::string{"false"}; },
            [](std::int64_t v) { return std::to_string(v); },
            [](double v) {
                std::ostringstream oss;
                oss << std::setprecision(6) << std::defaultfloat << v;
                return oss.str();
            },
            [](const std::string& v) { return v; },
            [](const std::vector<std::string>& v) { return joinStrings(v); },
            [](const nlohmann::json& v) { return v.dump(2); }
        },
        value);
}

[[nodiscard]] std::string hintString(const ConfigFieldDesc& desc, std::string_view key) {
    auto it = desc.uiHints.find(std::string(key));
    if (it == desc.uiHints.end()) {
        return {};
    }
    if (!it->second.is_string()) {
        return {};
    }
    return it->second.get<std::string>();
}

[[nodiscard]] std::unordered_map<std::string, std::string> enumLabels(const ConfigFieldDesc& desc) {
    std::unordered_map<std::string, std::string> labels;
    auto it = desc.uiHints.find("enumLabels");
    if (it != desc.uiHints.end() && it->second.is_object()) {
        for (const auto& [key, value] : it->second.items()) {
            if (value.is_string()) {
                labels.emplace(key, value.get<std::string>());
            }
        }
    }
    return labels;
}

[[nodiscard]] bool hasTooltipContent(const ConfigFieldDesc& desc) {
    if (!desc.description.empty()) {
        return true;
    }
    const std::string tooltip = hintString(desc, "tooltip");
    if (!tooltip.empty()) {
        return true;
    }
    const std::string defaultValue = configValueToString(desc.defaultValue);
    return !defaultValue.empty();
}

[[nodiscard]] float numericSpeed(const ConfigFieldDesc& desc, float fallback) {
    if (desc.validation.step && *desc.validation.step > 0.0) {
        return static_cast<float>(*desc.validation.step);
    }
    return fallback;
}

std::string toLowerCopy(std::string_view value) {
    std::string out(value);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return out;
}

bool containsCaseInsensitive(std::string_view haystack, const std::string& needleLower) {
    if (needleLower.empty()) {
        return true;
    }
    const auto first = std::search(haystack.begin(), haystack.end(), needleLower.begin(), needleLower.end(),
                                   [](char lhs, char rhs) {
                                       return static_cast<char>(std::tolower(static_cast<unsigned char>(lhs))) == rhs;
                                   });
    return first != haystack.end();
}

const ConfigSectionState* findFirstMatchingSection(const std::vector<ConfigSectionState>& sections,
                                                   const std::function<bool(const ConfigSectionState&)>& predicate) {
    for (const auto& section : sections) {
        if (predicate(section)) {
            return &section;
        }
        if (!section.children.empty()) {
            if (const auto* child = findFirstMatchingSection(section.children, predicate)) {
                return child;
            }
        }
    }
    return nullptr;
}
} // namespace

ConfigurationWindow::ConfigurationWindow() {
    searchBuffer_.fill('\0');
    unknownJsonBufferDirty_ = true;
    unknownJsonParseValid_ = true;
    unknownJsonParseError_.clear();
}

void ConfigurationWindow::ensureEditorState() {
    if (stateInitialized_) {
        return;
    }
    editorState_ = ConfigurationEditorState::fromCurrent();
    stateInitialized_ = true;
    selectionNeedsValidation_ = true;
    unknownJsonBufferDirty_ = true;
    unknownJsonParseValid_ = editorState_.unknownValidation().valid;
    unknownJsonParseError_ = editorState_.unknownValidation().message;
    baselineSnapshot_ = editorState_.toJson();
    lastAppliedSnapshot_ = baselineSnapshot_;
    stagedSnapshotCacheValid_ = false;
    hasUnappliedChanges_ = false;
    hasAppliedUnsavedChanges_ = false;
    pendingAction_ = PendingAction::None;
    closePrompt_ = ClosePrompt::None;
    requestCloseCallback_ = {};
}

const ConfigSectionState* ConfigurationWindow::findSectionState(std::string_view id) const {
    if (id.empty()) {
        return nullptr;
    }
    return editorState_.section(id);
}

const ConfigSectionState* ConfigurationWindow::findFirstNavigableSection() const {
    auto predicate = [this](const ConfigSectionState& section) {
        return isSectionDisplayable(section);
    };
    return findFirstMatchingSection(editorState_.sections(), predicate);
}

bool ConfigurationWindow::passesVisibilityFilters(const ConfigSectionDesc& desc) const {
    if ((desc.flags & ConfigSectionFlags::Hidden) != ConfigSectionFlags::None) {
        return false;
    }
    if (!showAdvanced_ && (desc.flags & ConfigSectionFlags::Advanced) != ConfigSectionFlags::None) {
        return false;
    }
    if (!showExperimental_ && (desc.flags & ConfigSectionFlags::Experimental) != ConfigSectionFlags::None) {
        return false;
    }
    return true;
}

bool ConfigurationWindow::matchesFieldSearch(const ConfigFieldState& field) const {
    if (searchQueryLower_.empty()) {
        return true;
    }
    if (!field.descriptor) {
        return false;
    }
    if (!passesFieldVisibility(*field.descriptor)) {
        return false;
    }
    if (containsCaseInsensitive(field.descriptor->label, searchQueryLower_)) {
        return true;
    }
    if (containsCaseInsensitive(field.descriptor->id, searchQueryLower_)) {
        return true;
    }
    if (containsCaseInsensitive(field.descriptor->description, searchQueryLower_)) {
        return true;
    }
    return false;
}

bool ConfigurationWindow::passesFieldVisibility(const ConfigFieldDesc& desc) const {
    if ((desc.flags & ConfigFieldFlags::Hidden) != ConfigFieldFlags::None) {
        return false;
    }
    if (!showAdvanced_ && (desc.flags & ConfigFieldFlags::Advanced) != ConfigFieldFlags::None) {
        return false;
    }
    if (!showExperimental_ && (desc.flags & ConfigFieldFlags::Experimental) != ConfigFieldFlags::None) {
        return false;
    }
    return true;
}

bool ConfigurationWindow::isFieldDisplayable(const ConfigFieldState& field) const {
    if (!field.descriptor) {
        return false;
    }
    if (!passesFieldVisibility(*field.descriptor)) {
        return false;
    }
    return true;
}

bool ConfigurationWindow::matchesSearch(const ConfigSectionState& section) const {
    if (searchQueryLower_.empty()) {
        return true;
    }
    if (!section.descriptor) {
        return false;
    }
    if (containsCaseInsensitive(section.descriptor->label, searchQueryLower_)) {
        return true;
    }
    if (containsCaseInsensitive(section.descriptor->id, searchQueryLower_)) {
        return true;
    }
    if (containsCaseInsensitive(section.descriptor->description, searchQueryLower_)) {
        return true;
    }
    for (const auto& field : section.fields) {
        if (matchesFieldSearch(field)) {
            return true;
        }
    }
    for (const auto& child : section.children) {
        if (child.descriptor && passesVisibilityFilters(*child.descriptor) && matchesSearch(child)) {
            return true;
        }
    }
    return false;
}

bool ConfigurationWindow::isSectionDisplayable(const ConfigSectionState& section) const {
    if (!section.descriptor) {
        return false;
    }
    if (!passesVisibilityFilters(*section.descriptor)) {
        return false;
    }
    return matchesSearch(section);
}

void ConfigurationWindow::syncSearchBuffer() {
    std::fill(searchBuffer_.begin(), searchBuffer_.end(), '\0');
    if (!searchQuery_.empty()) {
        std::strncpy(searchBuffer_.data(), searchQuery_.c_str(), searchBuffer_.size() - 1);
    }
    searchBufferDirty_ = false;
    searchQueryLower_ = toLowerCopy(searchQuery_);
}

void ConfigurationWindow::ensureValidSelection() {
    const ConfigSectionState* selection = nullptr;
    if (!selectedSectionId_.empty() && selectedSectionId_ != kUnknownSectionId) {
        selection = findSectionState(selectedSectionId_);
    }
    if (selectedSectionId_ == kUnknownSectionId) {
        if (shouldDisplayUnknownSection()) {
            selectionNeedsValidation_ = false;
            return;
        }
    } else if (selection && isSectionDisplayable(*selection)) {
        selectionNeedsValidation_ = false;
        return;
    }

    const auto* fallback = findFirstNavigableSection();
    if (fallback && fallback->descriptor) {
        selectedSectionId_ = fallback->descriptor->id;
    } else if (shouldDisplayUnknownSection()) {
        selectedSectionId_ = std::string(kUnknownSectionId);
    } else {
        selectedSectionId_.clear();
    }
    selectionNeedsValidation_ = false;
}

void ConfigurationWindow::render(WindowContext& ctx) {
    ensureEditorState();

    if (!stateInitialized_) {
        return;
    }

    requestCloseCallback_ = ctx.requestClose;

    if (selectionNeedsValidation_) {
        ensureValidSelection();
    }

    ImGui::PushID(this);

    renderToolbar();

    if (selectionNeedsValidation_) {
        ensureValidSelection();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginChild("config-window-content", ImVec2(0.0f, 0.0f), false);

    ImGui::BeginChild("config-window-nav", ImVec2(kNavigationWidth, 0.0f), true);
    renderSectionNavigation();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("config-window-detail", ImVec2(0.0f, 0.0f), true);
    renderSectionDetails();
    ImGui::EndChild();

    ImGui::EndChild();

    renderModals();
    processPendingActions(ctx);

    ImGui::PopID();
}

void ConfigurationWindow::renderToolbar() {
    if (searchBufferDirty_) {
        syncSearchBuffer();
    }

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputTextWithHint("##config-search", "Search settings...", searchBuffer_.data(), searchBuffer_.size())) {
        searchQuery_ = searchBuffer_.data();
        searchQueryLower_ = toLowerCopy(searchQuery_);
        selectionNeedsValidation_ = true;
    }

    ImGui::Spacing();

    bool filtersChanged = false;
    filtersChanged |= ImGui::Checkbox("Show advanced", &showAdvanced_);
    ImGui::SameLine();
    filtersChanged |= ImGui::Checkbox("Show experimental", &showExperimental_);
    if (filtersChanged) {
        selectionNeedsValidation_ = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Toggle visibility for advanced or experimental settings.");

    ImGui::Spacing();

    const bool validationErrors = hasValidationErrors();
    const bool canApply = hasUnappliedChanges_ && !validationErrors;
    const bool canSave = (hasUnappliedChanges_ || hasAppliedUnsavedChanges_) && !validationErrors;
    const bool canRevert = (editorState_.isDirty() || editorState_.isUnknownDirty() || hasUnappliedChanges_ || hasAppliedUnsavedChanges_);

    ImGui::BeginDisabled(!canApply);
    if (ImGui::Button("Apply")) {
        pendingAction_ = PendingAction::Apply;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("Update the running editor without saving to disk.");
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!canSave);
    if (ImGui::Button("Save")) {
        pendingAction_ = PendingAction::Save;
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("Apply changes and write to config.json.");
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!canRevert);
    if (ImGui::Button("Revert All")) {
        ImGui::OpenPopup(kRevertAllModalId);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::SetTooltip("Discard staged changes and restore values from disk.");
    }

    if (validationErrors) {
        ImGui::SameLine();
        ImGui::TextColored(kInvalidColor, "Resolve validation errors to enable Apply or Save.");
    }
}

void ConfigurationWindow::renderSectionNavigation() {
    bool anyRendered = false;
    const bool forceExpand = !searchQueryLower_.empty();
    for (const auto& section : editorState_.sections()) {
        anyRendered |= renderSectionNode(section, 0, forceExpand);
    }
    anyRendered |= renderUnknownNavigationItem();
    if (!anyRendered) {
        if (!searchQueryLower_.empty()) {
            ImGui::TextDisabled("No sections match your search.");
        } else {
            ImGui::TextDisabled("No configuration sections available.");
        }
    }
}

void ConfigurationWindow::renderSectionFields(ConfigSectionState& section) {
    bool anyRendered = false;
    const bool hasFields = !section.fields.empty();

    for (auto& field : section.fields) {
        if (!isFieldDisplayable(field)) {
            continue;
        }
        renderField(field);
        ImGui::Spacing();
        ImGui::Spacing();
        anyRendered = true;
    }

    if (!anyRendered) {
        if (!hasFields) {
            ImGui::TextDisabled("No editable fields in this section yet.");
        } else if (!searchQueryLower_.empty()) {
            ImGui::TextDisabled("No fields in this section match your search.");
        } else {
            ImGui::TextDisabled("All fields in this section are currently hidden by filters. Enable \"Show advanced\" or \"Show experimental\" above to reveal them.");
        }
    }
}

void ConfigurationWindow::renderField(ConfigFieldState& field) {
    const auto* desc = field.descriptor;
    if (!desc) {
        return;
    }

    ImGui::PushID(desc->id.c_str());

    const bool highlightMatch = !searchQueryLower_.empty() && matchesFieldSearch(field);
    renderFieldHeader(field, highlightMatch);

    ImGui::Spacing();

    const bool invalid = !field.validation.valid;
    if (invalid) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.32f, 0.16f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.36f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.40f, 0.24f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, kInvalidColor);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    }

    bool widgetInteracted = false;
    bool valueChanged = false;

    switch (desc->type) {
    case ConfigFieldType::Boolean: {
        bool value = false;
        if (const auto* current = std::get_if<bool>(&field.currentValue)) {
            value = *current;
        }
        if (ImGui::Checkbox("##value", &value)) {
            widgetInteracted = true;
            ConfigValue newValue = value;
            valueChanged |= editorState_.setFieldValue(desc->id, std::move(newValue));
        }
        break;
    }
    case ConfigFieldType::Integer: {
        std::int64_t value = 0;
        if (const auto* current = std::get_if<std::int64_t>(&field.currentValue)) {
            value = *current;
        }
        std::int64_t minValue = 0;
        std::int64_t maxValue = 0;
        const std::int64_t* minPtr = nullptr;
        const std::int64_t* maxPtr = nullptr;
        if (desc->validation.min) {
            minValue = static_cast<std::int64_t>(*desc->validation.min);
            minPtr = &minValue;
        }
        if (desc->validation.max) {
            maxValue = static_cast<std::int64_t>(*desc->validation.max);
            maxPtr = &maxValue;
        }
        ImGui::SetNextItemWidth(-1.0f);
        const float speed = numericSpeed(*desc, 1.0f);
        if (ImGui::DragScalar("##value", ImGuiDataType_S64, &value, speed, minPtr, maxPtr, "%lld")) {
            widgetInteracted = true;
            ConfigValue newValue = value;
            valueChanged |= editorState_.setFieldValue(desc->id, std::move(newValue));
        }
        break;
    }
    case ConfigFieldType::Float: {
        double value = 0.0;
        if (const auto* current = std::get_if<double>(&field.currentValue)) {
            value = *current;
        } else if (const auto* intCurrent = std::get_if<std::int64_t>(&field.currentValue)) {
            value = static_cast<double>(*intCurrent);
        }
        double minValue = 0.0;
        double maxValue = 0.0;
        const double* minPtr = nullptr;
        const double* maxPtr = nullptr;
        if (desc->validation.min) {
            minValue = *desc->validation.min;
            minPtr = &minValue;
        }
        if (desc->validation.max) {
            maxValue = *desc->validation.max;
            maxPtr = &maxValue;
        }
        std::string format;
        if (desc->validation.precision) {
            char buffer[16];
            std::snprintf(buffer, sizeof(buffer), "%%.%df", *desc->validation.precision);
            format = buffer;
        } else {
            format = "%.3f";
        }
        ImGui::SetNextItemWidth(-1.0f);
        const float speed = numericSpeed(*desc, 0.01f);
        if (ImGui::DragScalar("##value", ImGuiDataType_Double, &value, speed, minPtr, maxPtr, format.c_str())) {
            widgetInteracted = true;
            ConfigValue newValue = value;
            valueChanged |= editorState_.setFieldValue(desc->id, std::move(newValue));
        }
        break;
    }
    case ConfigFieldType::Enum: {
        const auto& options = desc->validation.enumValues;
        if (options.empty()) {
            std::string value;
            if (const auto* current = std::get_if<std::string>(&field.currentValue)) {
                value = *current;
            }
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText("##value", &value)) {
                widgetInteracted = true;
                ConfigValue newValue = value;
                valueChanged |= editorState_.setFieldValue(desc->id, std::move(newValue));
            }
            break;
        }

        std::string currentValue;
        if (const auto* current = std::get_if<std::string>(&field.currentValue)) {
            currentValue = *current;
        }
        auto labels = enumLabels(*desc);
        int currentIndex = -1;
        for (std::size_t i = 0; i < options.size(); ++i) {
            if (options[i] == currentValue) {
                currentIndex = static_cast<int>(i);
                break;
            }
        }
        std::string preview;
        if (currentIndex >= 0) {
            const auto previewIt = labels.find(options[static_cast<std::size_t>(currentIndex)]);
            if (previewIt != labels.end()) {
                preview = previewIt->second;
            } else {
                preview = options[static_cast<std::size_t>(currentIndex)];
            }
        } else {
            preview = currentValue;
        }
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##value", preview.empty() ? "Select value" : preview.c_str())) {
            for (std::size_t i = 0; i < options.size(); ++i) {
                const std::string& option = options[i];
                const auto labelIt = labels.find(option);
                const std::string& itemLabel = labelIt != labels.end() ? labelIt->second : option;
                const bool selected = static_cast<int>(i) == currentIndex;
                if (ImGui::Selectable(itemLabel.c_str(), selected)) {
                    widgetInteracted = true;
                    ConfigValue newValue = option;
                    bool changed = editorState_.setFieldValue(desc->id, std::move(newValue));
                    valueChanged = valueChanged || changed;
                    currentIndex = static_cast<int>(i);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        break;
    }
    case ConfigFieldType::String: {
        std::string value;
        if (const auto* current = std::get_if<std::string>(&field.currentValue)) {
            value = *current;
        }
        const std::string placeholder = hintString(*desc, "placeholder");
        ImGui::SetNextItemWidth(-1.0f);
        if (!placeholder.empty()) {
            if (ImGui::InputTextWithHint("##value", placeholder.c_str(), &value)) {
                widgetInteracted = true;
                ConfigValue newValue = value;
                valueChanged |= editorState_.setFieldValue(desc->id, std::move(newValue));
            }
        } else {
            if (ImGui::InputText("##value", &value)) {
                widgetInteracted = true;
                ConfigValue newValue = value;
                valueChanged |= editorState_.setFieldValue(desc->id, std::move(newValue));
            }
        }
        break;
    }
    case ConfigFieldType::Path: {
        std::string value;
        if (const auto* current = std::get_if<std::string>(&field.currentValue)) {
            value = *current;
        }
        const std::string placeholder = hintString(*desc, "placeholder");
        const ImGuiStyle& style = ImGui::GetStyle();
        const float buttonWidth = ImGui::CalcTextSize("Browse...").x + style.FramePadding.x * 2.0f;
        const float avail = ImGui::GetContentRegionAvail().x;
        const float inputWidth = std::max(120.0f, avail - buttonWidth - style.ItemInnerSpacing.x);
        ImGui::SetNextItemWidth(inputWidth);
        bool textChanged = false;
        if (!placeholder.empty()) {
            if (ImGui::InputTextWithHint("##value", placeholder.c_str(), &value)) {
                widgetInteracted = true;
                textChanged = true;
            }
        } else {
            if (ImGui::InputText("##value", &value)) {
                widgetInteracted = true;
                textChanged = true;
            }
        }
        if (textChanged) {
            ConfigValue newValue = value;
            valueChanged |= editorState_.setFieldValue(desc->id, std::move(newValue));
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(true);
        if (ImGui::Button("Browse...")) {}
        ImGui::EndDisabled();
        break;
    }
    case ConfigFieldType::List: {
        std::vector<std::string> values;
        if (const auto* current = std::get_if<std::vector<std::string>>(&field.currentValue)) {
            values = *current;
        }
        const std::string placeholder = hintString(*desc, "itemPlaceholder");
        bool pendingUpdate = false;

        if (values.empty()) {
            ImGui::TextDisabled("No entries.");
        }

        for (std::size_t i = 0; i < values.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            const ImGuiStyle& style = ImGui::GetStyle();
            const float buttonWidth = ImGui::CalcTextSize("Remove").x + style.FramePadding.x * 2.0f;
            const float avail = ImGui::GetContentRegionAvail().x;
            const float inputWidth = std::max(80.0f, avail - buttonWidth - style.ItemInnerSpacing.x);
            ImGui::SetNextItemWidth(inputWidth);
            if (!placeholder.empty()) {
                if (ImGui::InputTextWithHint("##item", placeholder.c_str(), &values[i])) {
                    pendingUpdate = true;
                    widgetInteracted = true;
                }
            } else {
                if (ImGui::InputText("##item", &values[i])) {
                    pendingUpdate = true;
                    widgetInteracted = true;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove")) {
                widgetInteracted = true;
                values.erase(values.begin() + static_cast<std::ptrdiff_t>(i));
                pendingUpdate = true;
                ImGui::PopID();
                --i;
                continue;
            }
            ImGui::PopID();
        }

        if (ImGui::Button("Add Entry")) {
            widgetInteracted = true;
            values.emplace_back();
            pendingUpdate = true;
        }

        if (pendingUpdate) {
            ConfigValue newValue = std::move(values);
            valueChanged |= editorState_.setFieldValue(desc->id, std::move(newValue));
        }
        break;
    }
    case ConfigFieldType::JsonBlob:
    case ConfigFieldType::Hotkeys: {
        std::string preview;
        if (const auto* current = std::get_if<nlohmann::json>(&field.currentValue)) {
            preview = current->dump(2);
        } else {
            preview = "{}";
        }
        ImGui::BeginDisabled(true);
        ImGui::InputTextMultiline("##value", &preview, ImVec2(-1.0f, 140.0f));
        ImGui::EndDisabled();
        ImGui::TextDisabled("Editing for this field type will be available in a future update.");
        break;
    }
    }

    if (invalid) {
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    }

    if (valueChanged) {
        updateAfterStateMutation();
        editorState_.validateField(desc->id, ValidationPhase::OnEdit);
    } else if (widgetInteracted) {
        editorState_.validateField(desc->id, ValidationPhase::OnEdit);
    }

    if (!field.validation.valid) {
        ImGui::Spacing();
        renderFieldValidation(field);
    }

    ImGui::PopID();
}

void ConfigurationWindow::renderFieldHeader(const ConfigFieldState& field, bool highlightMatch) const {
    const auto* desc = field.descriptor;
    if (!desc) {
        return;
    }

    const std::string label = fieldDisplayName(*desc);
    if (highlightMatch) {
        ImGui::PushStyleColor(ImGuiCol_Text, kSearchHighlightColor);
    }
    ImGui::TextUnformatted(label.c_str());
    if (highlightMatch) {
        ImGui::PopStyleColor();
    }

    const bool hasTooltip = renderFieldTooltip(*desc, field);
    renderFieldBadges(field);
    if (hasTooltip) {
        ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
        ImGui::TextDisabled("(?)");
        renderFieldTooltip(*desc, field);
    }
}

bool ConfigurationWindow::renderFieldTooltip(const ConfigFieldDesc& desc, const ConfigFieldState& field) const {
    const bool hasDescription = !desc.description.empty();
    const std::string tooltipHint = hintString(desc, "tooltip");
    const std::string defaultValue = configValueToString(desc.defaultValue);
    const bool hasTooltip = hasDescription || !tooltipHint.empty() || !defaultValue.empty();
    if (!hasTooltip) {
        return false;
    }

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::BeginTooltip();
        if (hasDescription) {
            ImGui::TextWrapped("%s", desc.description.c_str());
        }
        if (!tooltipHint.empty()) {
            if (hasDescription) {
                ImGui::Spacing();
            }
            ImGui::TextWrapped("%s", tooltipHint.c_str());
        }
        if (!defaultValue.empty()) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Default: %s", defaultValue.c_str());
        }
        if (field.isDirty()) {
            ImGui::Spacing();
            ImGui::TextDisabled("Current value differs from saved configuration.");
        }
        ImGui::EndTooltip();
    }

    return true;
}

void ConfigurationWindow::renderFieldBadges(const ConfigFieldState& field) const {
    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    if (field.isDirty()) {
        ImGui::SameLine(0.0f, spacing);
        ImGui::TextColored(kDirtyColor, "Dirty");
    }
    if (!field.validation.valid) {
        ImGui::SameLine(0.0f, spacing);
        ImGui::TextColored(kInvalidColor, "Invalid");
    }
}

void ConfigurationWindow::renderFieldValidation(const ConfigFieldState& field) const {
    if (field.validation.valid) {
        return;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, kInvalidColor);
    if (!field.validation.message.empty()) {
        ImGui::TextWrapped("%s", field.validation.message.c_str());
    } else {
        ImGui::TextUnformatted("Value is invalid.");
    }
    ImGui::PopStyleColor();
}

void ConfigurationWindow::renderSectionBadges(const ConfigSectionState& sectionState) const {
    const std::size_t dirtyCount = sectionState.dirtyFieldCount();
    const std::size_t invalidCount = sectionState.invalidFieldCount();
    if (dirtyCount == 0 && invalidCount == 0) {
        return;
    }

    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    ImGui::SameLine(0.0f, spacing);
    if (dirtyCount > 0) {
        ImGui::TextColored(ImVec4(0.90f, 0.72f, 0.18f, 1.0f), "Dirty %zu", dirtyCount);
        if (invalidCount > 0) {
            ImGui::SameLine(0.0f, spacing * 0.75f);
            ImGui::TextColored(ImVec4(0.94f, 0.33f, 0.24f, 1.0f), "Invalid %zu", invalidCount);
        }
    } else if (invalidCount > 0) {
        ImGui::TextColored(ImVec4(0.94f, 0.33f, 0.24f, 1.0f), "Invalid %zu", invalidCount);
    }
}

bool ConfigurationWindow::renderSectionNode(const ConfigSectionState& sectionState, int depth, bool forceExpand) {
    const auto* desc = sectionState.descriptor;
    if (!desc) {
        return false;
    }
    if (!passesVisibilityFilters(*desc)) {
        return false;
    }
    if (!matchesSearch(sectionState)) {
        return false;
    }

    std::vector<std::reference_wrapper<const ConfigSectionState>> visibleChildren;
    visibleChildren.reserve(sectionState.children.size());
    for (const auto& child : sectionState.children) {
        if (!child.descriptor) {
            continue;
        }
        if (!passesVisibilityFilters(*child.descriptor)) {
            continue;
        }
        if (!matchesSearch(child)) {
            continue;
        }
        visibleChildren.emplace_back(child);
    }

    const bool hasChildren = !visibleChildren.empty();

    if (depth > 0) {
        ImGui::Indent(kIndentPerLevel);
    }

    ImGui::PushID(desc->id.c_str());
    const std::string display = sectionDisplayName(*desc);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
    if (!selectedSectionId_.empty() && selectedSectionId_ == desc->id) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (!hasChildren) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    if (forceExpand && hasChildren) {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
    }

    const bool open = ImGui::TreeNodeEx("##section", flags, "%s", display.c_str());
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
        selectedSectionId_ = desc->id;
        selectionNeedsValidation_ = false;
    }

    renderSectionBadges(sectionState);

    if (hasChildren && open) {
        for (const auto& child : visibleChildren) {
            renderSectionNode(child.get(), depth + 1, forceExpand);
        }
        ImGui::TreePop();
    }

    ImGui::PopID();

    if (depth > 0) {
        ImGui::Unindent(kIndentPerLevel);
    }

    return true;
}

void ConfigurationWindow::renderSectionDetails() {
    if (selectedSectionId_ == kUnknownSectionId) {
        if (shouldDisplayUnknownSection()) {
            renderUnknownSectionDetails();
        } else if (!searchQueryLower_.empty()) {
            ImGui::TextDisabled("No configuration sections match your current search.");
        } else {
            ImGui::TextDisabled("Select a configuration section from the left to get started.");
        }
        return;
    }

    ConfigSectionState* section = nullptr;
    if (!selectedSectionId_.empty()) {
        section = editorState_.section(selectedSectionId_);
    }

    if (!section || !section->descriptor || !isSectionDisplayable(*section)) {
        if (!searchQueryLower_.empty()) {
            ImGui::TextDisabled("No configuration sections match your current search.");
        } else {
            ImGui::TextDisabled("Select a configuration section from the left to get started.");
        }
        return;
    }

    const auto& desc = *section->descriptor;
    const std::string title = sectionDisplayName(desc);
    ImGui::TextUnformatted(title.c_str());
    if (!desc.description.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", desc.description.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    renderSectionFields(*section);

    std::vector<const ConfigSectionState*> visibleChildren;
    visibleChildren.reserve(section->children.size());
    for (auto& child : section->children) {
        if (!child.descriptor) {
            continue;
        }
        if (!passesVisibilityFilters(*child.descriptor)) {
            continue;
        }
        if (!matchesSearch(child)) {
            continue;
        }
        visibleChildren.push_back(&child);
    }

    if (!visibleChildren.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("Subsections:");
        ImGui::Indent(kIndentPerLevel);
        for (const auto* child : visibleChildren) {
            const std::string childName = sectionDisplayName(*child->descriptor);
            ImGui::BulletText("%s", childName.c_str());
        }
        ImGui::Unindent(kIndentPerLevel);
    }
}

bool ConfigurationWindow::renderUnknownNavigationItem() {
    if (!shouldDisplayUnknownSection()) {
        return false;
    }
    if (!unknownSectionMatchesSearch()) {
        return false;
    }

    const bool isSelected = selectedSectionId_ == kUnknownSectionId;
    ImGui::PushID(kUnknownSectionId.data());
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding |
                              ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (isSelected) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    const char* label = editorState_.hasUnknownEntries() ? "Additional JSON" : "Custom JSON";
    ImGui::TreeNodeEx("##unknown", flags, "%s", label);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        selectedSectionId_ = std::string(kUnknownSectionId);
        selectionNeedsValidation_ = false;
    }
    renderUnknownBadges();
    ImGui::PopID();
    return true;
}

void ConfigurationWindow::renderUnknownBadges() const {
    const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    if (editorState_.isUnknownDirty()) {
        ImGui::SameLine(0.0f, spacing);
        ImGui::TextColored(kDirtyColor, "Dirty");
    }
    if (!editorState_.unknownValidation().valid) {
        ImGui::SameLine(0.0f, spacing);
        ImGui::TextColored(kInvalidColor, "Invalid");
    }
}

bool ConfigurationWindow::unknownSectionMatchesSearch() {
    if (searchQueryLower_.empty()) {
        return true;
    }
    if (!shouldDisplayUnknownSection()) {
        return false;
    }
    syncUnknownJsonBuffer();
    if (containsCaseInsensitive("Additional JSON", searchQueryLower_)) {
        return true;
    }
    if (containsCaseInsensitive("Custom JSON", searchQueryLower_)) {
        return true;
    }
    return !unknownJsonBufferLower_.empty() && unknownJsonBufferLower_.find(searchQueryLower_) != std::string::npos;
}

bool ConfigurationWindow::shouldDisplayUnknownSection() const {
    if (editorState_.hasUnknownEntries()) {
        return true;
    }
    if (editorState_.isUnknownDirty()) {
        return true;
    }
    if (!editorState_.unknownValidation().valid) {
        return true;
    }
    return showAdvanced_;
}

void ConfigurationWindow::syncUnknownJsonBuffer() {
    if (!unknownJsonBufferDirty_) {
        return;
    }
    const auto& unknownJson = editorState_.unknownEntries();
    if (unknownJson.is_null()) {
        unknownJsonBuffer_ = "{}";
    } else if (unknownJson.is_object() || unknownJson.is_array()) {
        unknownJsonBuffer_ = unknownJson.dump(2);
    } else {
        unknownJsonBuffer_ = unknownJson.dump();
    }
    unknownJsonBufferLower_ = toLowerCopy(unknownJsonBuffer_);
    const auto& validation = editorState_.unknownValidation();
    unknownJsonParseValid_ = validation.valid;
    unknownJsonParseError_ = validation.message;
    unknownJsonBufferDirty_ = false;
}

void ConfigurationWindow::renderUnknownSectionDetails() {
    syncUnknownJsonBuffer();

    const bool highlight = !searchQueryLower_.empty() &&
                           (!unknownJsonBufferLower_.empty() && unknownJsonBufferLower_.find(searchQueryLower_) != std::string::npos);
    const char* title = editorState_.hasUnknownEntries() ? "Additional JSON Settings" : "Custom JSON Overrides";
    if (highlight) {
        ImGui::PushStyleColor(ImGuiCol_Text, kSearchHighlightColor);
    }
    ImGui::TextUnformatted(title);
    if (highlight) {
        ImGui::PopStyleColor();
    }
    renderUnknownBadges();

    ImGui::Spacing();
    ImGui::TextWrapped("Keys that are not defined in the configuration schema remain editable here as raw JSON.");
    ImGui::TextWrapped("Changes will be preserved when saving, even if newer versions introduce additional settings.");

    ImGui::Spacing();

    const bool invalid = !editorState_.unknownValidation().valid;
    if (invalid) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.32f, 0.16f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.36f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.40f, 0.24f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, kInvalidColor);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    }

    if (ImGui::InputTextMultiline("##unknown-json", &unknownJsonBuffer_, ImVec2(-1.0f, kUnknownEditorHeight), ImGuiInputTextFlags_AllowTabInput)) {
        nlohmann::json parsed;
        const std::string_view trimmed = unknownJsonBuffer_;
        bool parsedSuccessfully = false;
        bool contentChanged = false;
        if (trimmed.find_first_not_of(" \t\r\n") == std::string_view::npos) {
            parsed = nlohmann::json::object();
            parsedSuccessfully = true;
        } else {
            try {
                parsed = nlohmann::json::parse(unknownJsonBuffer_);
                parsedSuccessfully = true;
            } catch (const nlohmann::json::parse_error& ex) {
                unknownJsonParseValid_ = false;
                unknownJsonParseError_ = ex.what();
                FieldValidationState state;
                state.valid = false;
                state.message = std::string("Invalid JSON: ") + ex.what();
                editorState_.setUnknownValidation(std::move(state));
            }
        }

        if (parsedSuccessfully) {
            contentChanged = parsed != editorState_.unknownEntries();
            editorState_.setUnknownEntries(std::move(parsed));
            editorState_.clearUnknownValidation();
            unknownJsonParseValid_ = true;
            unknownJsonParseError_.clear();
        }
        unknownJsonBufferLower_ = toLowerCopy(unknownJsonBuffer_);
        if (parsedSuccessfully && contentChanged) {
            updateAfterStateMutation();
        }
    }

    if (invalid) {
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);
    }

    if (!editorState_.unknownValidation().valid) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, kInvalidColor);
        const auto& validation = editorState_.unknownValidation();
        if (!validation.message.empty()) {
            ImGui::TextWrapped("%s", validation.message.c_str());
        } else if (!unknownJsonParseError_.empty()) {
            ImGui::TextWrapped("%s", unknownJsonParseError_.c_str());
        } else {
            ImGui::TextUnformatted("JSON content is invalid.");
        }
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    ImGui::BeginDisabled(!editorState_.isUnknownDirty());
    if (ImGui::Button("Revert JSON to original")) {
        const bool wasDirty = editorState_.isUnknownDirty();
        editorState_.revertUnknownEntries();
        unknownJsonBufferDirty_ = true;
        syncUnknownJsonBuffer();
        if (wasDirty) {
            updateAfterStateMutation();
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::TextDisabled("Reverts additional keys to the values loaded from disk.");
}

void ConfigurationWindow::renderModals() {
    renderRevertAllModal();
    renderCloseModals();
}

void ConfigurationWindow::renderRevertAllModal() {
    if (ImGui::BeginPopupModal(kRevertAllModalId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Discard all staged configuration changes and restore values from disk?");
        ImGui::Spacing();
        if (ImGui::Button("Revert", ImVec2(120.0f, 0.0f))) {
            pendingAction_ = PendingAction::RevertAll;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void ConfigurationWindow::renderCloseModals() {
    const auto cancelAction = [this]() {
        closePrompt_ = ClosePrompt::None;
    };

    if (closePrompt_ == ClosePrompt::UnappliedChanges) {
        if (ImGui::BeginPopupModal(kCloseUnappliedModalId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("You have unapplied configuration changes. Apply them now, discard them, or cancel to keep editing.");
            ImGui::Spacing();
            if (ImGui::Button("Apply", ImVec2(110.0f, 0.0f))) {
                pendingAction_ = PendingAction::ApplyAndClose;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard", ImVec2(110.0f, 0.0f))) {
                pendingAction_ = PendingAction::DiscardAndClose;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(110.0f, 0.0f))) {
                cancelAction();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    if (closePrompt_ == ClosePrompt::UnsavedChanges) {
        if (ImGui::BeginPopupModal(kCloseUnsavedModalId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("You applied changes that haven't been saved to disk. Save them now, discard them, or cancel to keep editing.");
            ImGui::Spacing();
            if (ImGui::Button("Save", ImVec2(110.0f, 0.0f))) {
                pendingAction_ = PendingAction::SaveAndClose;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard", ImVec2(110.0f, 0.0f))) {
                pendingAction_ = PendingAction::DiscardAndClose;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(110.0f, 0.0f))) {
                cancelAction();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}

void ConfigurationWindow::openClosePrompt(ClosePrompt prompt) {
    closePrompt_ = prompt;
    switch (prompt) {
    case ClosePrompt::UnappliedChanges:
        ImGui::OpenPopup(kCloseUnappliedModalId);
        break;
    case ClosePrompt::UnsavedChanges:
        ImGui::OpenPopup(kCloseUnsavedModalId);
        break;
    case ClosePrompt::None:
        break;
    }
}

ConfigurationWindow::ApplyAttemptResult ConfigurationWindow::attemptApply() {
    ApplyAttemptResult result{};
    const bool hadPendingChanges = hasUnappliedChanges_;
    const bool hadValidationIssues = hasValidationErrors();

    if (!hadPendingChanges && !hadValidationIssues) {
        result.success = true;
        result.hadChanges = false;
        return result;
    }

    if (!editorState_.validateAll(ValidationPhase::OnApply)) {
        result.validationFailed = true;
        return result;
    }

    const auto& snapshot = currentSnapshot();
    const bool runtimeWillChange = (snapshot != lastAppliedSnapshot_);
    if (!ConfigurationManager::applyRuntime(snapshot)) {
        result.runtimeFailed = true;
        return result;
    }

    lastAppliedSnapshot_ = snapshot;
    hasUnappliedChanges_ = false;
    hasAppliedUnsavedChanges_ = (snapshot != baselineSnapshot_);

    result.success = true;
    result.hadChanges = hadPendingChanges || runtimeWillChange;
    return result;
}

ConfigurationWindow::SaveAttemptResult ConfigurationWindow::attemptSave() {
    SaveAttemptResult result{};

    ApplyAttemptResult applyResult = attemptApply();
    result.validationFailed = applyResult.validationFailed;
    result.runtimeFailed = applyResult.runtimeFailed;

    if (!applyResult.success) {
        return result;
    }

    result.hadChanges = hasAppliedUnsavedChanges_;
    const bool requestBackup = !backupCreatedThisSession_;
    result.backupRequested = requestBackup;

    bool backupCreated = false;
    if (!ConfigurationManager::save(requestBackup, requestBackup ? &backupCreated : nullptr)) {
        result.writeFailed = true;
        result.backupCreated = backupCreated;
        return result;
    }

    if (backupCreated) {
        backupCreatedThisSession_ = true;
    }
    result.backupCreated = backupCreated;

    editorState_.commitToCurrent();
    baselineSnapshot_ = lastAppliedSnapshot_;
    hasAppliedUnsavedChanges_ = false;

    result.success = true;
    return result;
}

void ConfigurationWindow::emitApplyFeedback(const ApplyAttemptResult& result, WindowContext& ctx, bool showSuccessToast) {
    using gb2d::logging::LogManager;

    if (result.success) {
        if (showSuccessToast && ctx.pushToast) {
            ctx.pushToast("Configuration applied.", 2.5f);
        }
        LogManager::info("Configuration applied.");
        return;
    }

    if (result.validationFailed) {
        if (ctx.pushToast) {
            ctx.pushToast("Resolve validation errors to apply configuration.", 3.5f);
        }
        LogManager::warn("Configuration apply blocked by validation errors.");
        return;
    }

    if (result.runtimeFailed) {
        if (ctx.pushToast) {
            ctx.pushToast("Configuration apply failed. See logs for details.", 3.5f);
        }
        LogManager::error("Configuration apply failed during runtime update.");
    }
}

void ConfigurationWindow::emitSaveFeedback(const SaveAttemptResult& result, WindowContext& ctx) {
    using gb2d::logging::LogManager;

    if (result.success) {
        if (ctx.pushToast) {
            ctx.pushToast("Configuration saved to config.json.", 2.5f);
        }
        LogManager::info("Configuration saved to config.json.");
        if (result.backupRequested) {
            if (result.backupCreated) {
                if (ctx.pushToast) {
                    ctx.pushToast("Backup created: config.backup.json", 2.5f);
                }
                LogManager::info("Configuration backup created at config.backup.json.");
            } else {
                if (ctx.pushToast) {
                    ctx.pushToast("Warning: Backup not created (config.backup.json).", 3.5f);
                }
                LogManager::warn("Configuration backup was requested but could not be created.");
            }
        }
        return;
    }

    if (result.validationFailed) {
        if (ctx.pushToast) {
            ctx.pushToast("Resolve validation errors to save configuration.", 3.5f);
        }
        LogManager::warn("Configuration save blocked by validation errors.");
        return;
    }

    if (result.runtimeFailed) {
        if (ctx.pushToast) {
            ctx.pushToast("Configuration save aborted: apply step failed.", 3.5f);
        }
        LogManager::error("Configuration save aborted because runtime apply failed.");
        return;
    }

    if (result.writeFailed) {
        if (ctx.pushToast) {
            ctx.pushToast("Configuration save failed: unable to write config.json. Your changes are still staged.", 4.0f);
        }
        LogManager::error("Configuration save failed while writing config.json; changes remain staged.");
        if (result.backupRequested && !result.backupCreated) {
            LogManager::warn("Configuration backup was requested but could not be created.");
        }
        return;
    }

    if (ctx.pushToast) {
        ctx.pushToast("Configuration save failed.", 3.0f);
    }
    LogManager::error("Configuration save failed for an unknown reason.");
}

void ConfigurationWindow::performRevertAll() {
    editorState_.revertAll();
    unknownJsonBufferDirty_ = true;
    syncUnknownJsonBuffer();
    updateAfterStateMutation();
}

void ConfigurationWindow::performDiscardChanges() {
    reloadEditorState();
}

void ConfigurationWindow::finalizeClose() {
    closePrompt_ = ClosePrompt::None;
    if (requestCloseCallback_) {
        auto callback = requestCloseCallback_;
        requestCloseCallback_ = {};
        callback();
    }
}

void ConfigurationWindow::updateAfterStateMutation() {
    invalidateSnapshotCache();
    const auto& snapshot = currentSnapshot();
    hasUnappliedChanges_ = (snapshot != lastAppliedSnapshot_);
}

void ConfigurationWindow::invalidateSnapshotCache() {
    stagedSnapshotCacheValid_ = false;
}

const nlohmann::json& ConfigurationWindow::currentSnapshot() {
    if (!stagedSnapshotCacheValid_) {
        stagedSnapshotCache_ = editorState_.toJson();
        stagedSnapshotCacheValid_ = true;
    }
    return stagedSnapshotCache_;
}

void ConfigurationWindow::reloadEditorState() {
    editorState_ = ConfigurationEditorState::fromCurrent();
    stateInitialized_ = true;
    selectionNeedsValidation_ = true;
    unknownJsonBufferDirty_ = true;
    unknownJsonParseValid_ = editorState_.unknownValidation().valid;
    unknownJsonParseError_ = editorState_.unknownValidation().message;
    baselineSnapshot_ = editorState_.toJson();
    lastAppliedSnapshot_ = baselineSnapshot_;
    invalidateSnapshotCache();
    hasUnappliedChanges_ = false;
    hasAppliedUnsavedChanges_ = false;
    pendingAction_ = PendingAction::None;
    closePrompt_ = ClosePrompt::None;
}

bool ConfigurationWindow::hasValidationErrors() const {
    if (editorState_.hasInvalidFields()) {
        return true;
    }
    if (!editorState_.unknownValidation().valid) {
        return true;
    }
    return false;
}

void ConfigurationWindow::processPendingActions(WindowContext& ctx) {
    if (pendingAction_ == PendingAction::None) {
        return;
    }

    const PendingAction action = pendingAction_;
    pendingAction_ = PendingAction::None;

    auto reopenPromptIfNeeded = [this]() {
        if (closePrompt_ != ClosePrompt::None) {
            openClosePrompt(closePrompt_);
        }
    };

    switch (action) {
    case PendingAction::Apply:
        emitApplyFeedback(attemptApply(), ctx, true);
        break;
    case PendingAction::Save:
        emitSaveFeedback(attemptSave(), ctx);
        break;
    case PendingAction::RevertAll:
        performRevertAll();
        break;
    case PendingAction::ApplyAndClose:
        {
            const auto result = attemptApply();
            emitApplyFeedback(result, ctx, true);
            if (result.success) {
                finalizeClose();
            } else {
                reopenPromptIfNeeded();
            }
        }
        break;
    case PendingAction::SaveAndClose:
        {
            const auto result = attemptSave();
            emitSaveFeedback(result, ctx);
            if (result.success) {
                finalizeClose();
            } else {
                reopenPromptIfNeeded();
            }
        }
        break;
    case PendingAction::DiscardAndClose:
        performDiscardChanges();
        finalizeClose();
        break;
    case PendingAction::None:
        break;
    }

    (void)ctx;
}

bool ConfigurationWindow::handleCloseRequest(WindowContext& ctx) {
    requestCloseCallback_ = ctx.requestClose;

    if (closePrompt_ != ClosePrompt::None) {
        openClosePrompt(closePrompt_);
        return false;
    }

    if (hasUnappliedChanges_) {
        openClosePrompt(ClosePrompt::UnappliedChanges);
        return false;
    }

    if (hasAppliedUnsavedChanges_) {
        openClosePrompt(ClosePrompt::UnsavedChanges);
        return false;
    }

    if (editorState_.isDirty()) {
        openClosePrompt(ClosePrompt::UnsavedChanges);
        return false;
    }

    return true;
}

void ConfigurationWindow::serialize(nlohmann::json& out) const {
    out["title"] = title_;
    out["selectedSection"] = selectedSectionId_;
    out["search"] = searchQuery_;
    out["showAdvanced"] = showAdvanced_;
    out["showExperimental"] = showExperimental_;
}

void ConfigurationWindow::deserialize(const nlohmann::json& in) {
    if (auto it = in.find("title"); it != in.end() && it->is_string()) {
        title_ = *it;
    }
    if (auto it = in.find("selectedSection"); it != in.end() && it->is_string()) {
        selectedSectionId_ = *it;
    }
    if (auto it = in.find("search"); it != in.end() && it->is_string()) {
        searchQuery_ = *it;
    } else {
        searchQuery_.clear();
    }
    if (auto it = in.find("showAdvanced"); it != in.end() && it->is_boolean()) {
        showAdvanced_ = it->get<bool>();
    }
    if (auto it = in.find("showExperimental"); it != in.end() && it->is_boolean()) {
        showExperimental_ = it->get<bool>();
    }

    searchQueryLower_ = toLowerCopy(searchQuery_);
    searchBufferDirty_ = true;
    stateInitialized_ = false;
    selectionNeedsValidation_ = true;
    unknownJsonBufferDirty_ = true;
    unknownJsonParseValid_ = true;
    unknownJsonParseError_.clear();
    unknownJsonBuffer_.clear();
    unknownJsonBufferLower_.clear();
    hasUnappliedChanges_ = false;
    hasAppliedUnsavedChanges_ = false;
    baselineSnapshot_ = nlohmann::json::object();
    lastAppliedSnapshot_ = nlohmann::json::object();
    invalidateSnapshotCache();
    closePrompt_ = ClosePrompt::None;
    pendingAction_ = PendingAction::None;
    requestCloseCallback_ = {};
}

} // namespace gb2d
