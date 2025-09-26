#pragma once
#include "ui/Window.h"
#include <string>
#include <optional>
#include <vector>
#include "raylib.h"

namespace gb2d {

class FilePreviewWindow : public IWindow {
public:
    FilePreviewWindow() = default;
    explicit FilePreviewWindow(std::string path) : path_(std::move(path)) {}
    ~FilePreviewWindow() override { unload(); }

    const char* typeId() const override { return "file-preview"; }
    const char* displayName() const override { return "File Preview"; }

    std::string title() const override { return title_; }
    void setTitle(std::string t) override { title_ = std::move(t); }

    void render(WindowContext& ctx) override;
    void serialize(nlohmann::json& out) const override;
    void deserialize(const nlohmann::json& in) override;

    void open(const std::string& path);

private:
    enum class Kind { None, Text, Image };
    std::string title_ { "Preview" };
    std::string path_{};
    Kind kind_{Kind::None};
    std::string text_{};
    int imgW_{0}, imgH_{0};
    unsigned int texId_{0};
    bool loaded_{false};

    void unload();
};

} // namespace gb2d
