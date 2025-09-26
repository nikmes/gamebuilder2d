#pragma once
#include "ui/Window.h"
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <TextEditor.h>

namespace gb2d {

class CodeEditorWindow : public IWindow {
public:
    CodeEditorWindow() = default;
    ~CodeEditorWindow() override = default;

    const char* typeId() const override { return "code-editor"; }
    const char* displayName() const override { return "Text Editor"; }

    std::string title() const override { return title_; }
    void setTitle(std::string t) override { title_ = std::move(t); }

    void render(WindowContext& ctx) override;

    // Persistence (minimal for now)
    void serialize(nlohmann::json& out) const override;
    void deserialize(const nlohmann::json& in) override;

    // External actions
    void openFile(const std::string& path);
    void newUntitled();
    bool saveCurrent(bool saveAs);
    bool saveAll();
    void closeCurrent();
    void closeAll();

private:
    struct Tab {
        std::string path;
        std::string title;
        std::unique_ptr<TextEditor> editor;
        bool dirty{false};
        std::string langName;
    };

    std::string title_ { "Text Editor" };
    std::vector<Tab> tabs_{};
    int current_{-1};
    std::optional<int> pending_save_as_index_{}; // index of tab awaiting Save As dialog result

public:
    static bool isTextLikeExtension(const std::string& ext);
    static const TextEditor::LanguageDefinition& languageForExtension(const std::string& ext, std::string& outName);
};

} // namespace gb2d
