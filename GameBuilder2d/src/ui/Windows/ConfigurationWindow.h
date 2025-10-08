#pragma once

#include "ui/Window.h"

#include "services/configuration/ConfigurationEditorState.h"

#include <array>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

namespace gb2d {

class ConfigurationWindow : public IWindow {
public:
    ConfigurationWindow();

    const char* typeId() const override { return "configuration"; }
    const char* displayName() const override { return "Configuration"; }

    std::string title() const override { return title_; }
    void setTitle(std::string title) override { title_ = std::move(title); }

    void render(WindowContext& ctx) override;
    bool handleCloseRequest(WindowContext& ctx) override;
    void serialize(nlohmann::json& out) const override;
    void deserialize(const nlohmann::json& in) override;

private:
    enum class ClosePrompt {
        None,
        UnappliedChanges,
        UnsavedChanges,
    };

    enum class PendingAction {
        None,
        Apply,
        Save,
        RevertAll,
        ApplyAndClose,
        SaveAndClose,
        DiscardAndClose,
    };

    struct ApplyAttemptResult {
        bool success{false};
        bool validationFailed{false};
        bool runtimeFailed{false};
        bool hadChanges{false};
    };

    struct SaveAttemptResult {
        bool success{false};
        bool validationFailed{false};
        bool runtimeFailed{false};
        bool writeFailed{false};
        bool hadChanges{false};
        bool backupRequested{false};
        bool backupCreated{false};
    };

    void ensureEditorState();
    void ensureValidSelection();
    void syncSearchBuffer();
    void renderToolbar();
    void renderSectionNavigation();
    void renderSectionDetails();
    void renderSectionFields(ConfigSectionState& section);
    void renderField(ConfigFieldState& field);
    void renderFieldHeader(const ConfigFieldState& field, bool highlightMatch) const;
    bool renderFieldTooltip(const ConfigFieldDesc& desc, const ConfigFieldState& field) const;
    void renderFieldBadges(const ConfigFieldState& field) const;
    void renderFieldValidation(const ConfigFieldState& field) const;

    bool renderUnknownNavigationItem();
    void renderUnknownSectionDetails();
    void renderUnknownBadges() const;
    bool unknownSectionMatchesSearch();
    bool shouldDisplayUnknownSection() const;
    void syncUnknownJsonBuffer();

    bool renderSectionNode(const ConfigSectionState& sectionState, int depth, bool forceExpand);
    void renderSectionBadges(const ConfigSectionState& sectionState) const;
    const ConfigSectionState* findSectionState(std::string_view id) const;
    const ConfigSectionState* findFirstNavigableSection() const;
    bool passesVisibilityFilters(const ConfigSectionDesc& desc) const;
    bool isSectionDisplayable(const ConfigSectionState& section) const;
    bool matchesSearch(const ConfigSectionState& section) const;
    bool matchesFieldSearch(const ConfigFieldState& field) const;
    bool passesFieldVisibility(const ConfigFieldDesc& desc) const;
    bool isFieldDisplayable(const ConfigFieldState& field) const;
    void renderModals();
    void renderRevertAllModal();
    void renderCloseModals();
    void openClosePrompt(ClosePrompt prompt);
    void processPendingActions(WindowContext& ctx);
    ApplyAttemptResult attemptApply();
    SaveAttemptResult attemptSave();
    void emitApplyFeedback(const ApplyAttemptResult& result, WindowContext& ctx, bool showSuccessToast);
    void emitSaveFeedback(const SaveAttemptResult& result, WindowContext& ctx);
    void performRevertAll();
    void performDiscardChanges();
    void finalizeClose();
    void updateAfterStateMutation();
    void invalidateSnapshotCache();
    const nlohmann::json& currentSnapshot();
    void reloadEditorState();
    bool hasValidationErrors() const;

    std::string title_{"Configuration"};
    bool stateInitialized_{false};
    ConfigurationEditorState editorState_{};
    std::string selectedSectionId_{};
    std::string searchQuery_{};
    std::string searchQueryLower_{};

    bool showAdvanced_{false};
    bool showExperimental_{false};

    bool searchBufferDirty_{true};
    std::array<char, 256> searchBuffer_{};
    bool selectionNeedsValidation_{false};

    bool unknownJsonBufferDirty_{true};
    std::string unknownJsonBuffer_{};
    std::string unknownJsonBufferLower_{};
    bool unknownJsonParseValid_{true};
    std::string unknownJsonParseError_{};

    bool hasUnappliedChanges_{false};
    bool hasAppliedUnsavedChanges_{false};
    bool backupCreatedThisSession_{false};
    nlohmann::json baselineSnapshot_{};
    nlohmann::json lastAppliedSnapshot_{};
    nlohmann::json stagedSnapshotCache_{};
    bool stagedSnapshotCacheValid_{false};

    ClosePrompt closePrompt_{ClosePrompt::None};
    PendingAction pendingAction_{PendingAction::None};
    std::function<void()> requestCloseCallback_{};
};

} // namespace gb2d
