#include "WindowManager.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include "ImGuiFileDialog.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <memory>
#include <nlohmann/json.hpp>
#include "services/logger/LogManager.h"
#include "services/configuration/ConfigurationManager.h"
#include <unordered_set>
// ImGuiColorTextEdit
#include <TextEditor.h>
#include <functional>
// Modular windows
#include "ui/WindowRegistry.h"
#include "ui/WindowContext.h"
#include "ui/Windows/ConsoleLogWindow.h"
// New modular windows
#include "ui/FullscreenSession.h"
#include "ui/Windows/CodeEditorWindow.h"
#include "ui/Windows/FilePreviewWindow.h"
#include "ui/Windows/GameWindow.h"
#include "ui/Windows/HotkeysWindow.h"
#include "ui/Windows/ConfigurationWindow.h"
#include "ui/Windows/AudioManagerWindow.h"
#include "services/hotkey/HotKeyManager.h"
#include "services/hotkey/HotKeyActions.h"
#include <string>

namespace gb2d {

namespace {

std::string hotkeyShortcutLabel(const char* actionId) {
    using gb2d::hotkeys::HotKeyManager;
    if (actionId == nullptr || !HotKeyManager::isInitialized()) {
        return {};
    }
    const auto* binding = HotKeyManager::binding(actionId);
    if (binding == nullptr || !binding->valid || binding->humanReadable.empty()) {
        return {};
    }
    return binding->humanReadable;
}

const char* shortcutArg(const std::string& value) {
    return value.empty() ? nullptr : value.c_str();
}

} // namespace

static void RegisterBuiltinWindows(WindowRegistry& reg) {
    // Console Log window
    WindowTypeDesc consoleDesc;
    consoleDesc.typeId = "console-log";
    consoleDesc.displayName = "Console Log";
    consoleDesc.factory = [](WindowContext&) -> std::unique_ptr<IWindow> {
        return std::make_unique<ConsoleLogWindow>();
    };
    reg.registerType(std::move(consoleDesc));
    // Code Editor window
    WindowTypeDesc editorDesc;
    editorDesc.typeId = "code-editor";
    editorDesc.displayName = "Text Editor";
    editorDesc.factory = [](WindowContext&) -> std::unique_ptr<IWindow> {
        return std::make_unique<CodeEditorWindow>();
    };
    reg.registerType(std::move(editorDesc));
    // File Preview window
    WindowTypeDesc previewDesc;
    previewDesc.typeId = "file-preview";
    previewDesc.displayName = "File Preview";
    previewDesc.factory = [](WindowContext&) -> std::unique_ptr<IWindow> {
        return std::make_unique<FilePreviewWindow>();
    };
    reg.registerType(std::move(previewDesc));

    WindowTypeDesc configurationDesc;
    configurationDesc.typeId = "configuration";
    configurationDesc.displayName = "Configuration";
    configurationDesc.factory = [](WindowContext&) -> std::unique_ptr<IWindow> {
        return std::make_unique<ConfigurationWindow>();
    };
    reg.registerType(std::move(configurationDesc));

    WindowTypeDesc audioManagerDesc;
    audioManagerDesc.typeId = "audio_manager";
    audioManagerDesc.displayName = "Audio Manager";
    audioManagerDesc.factory = [](WindowContext&) -> std::unique_ptr<IWindow> {
        return std::make_unique<AudioManagerWindow>();
    };
    reg.registerType(std::move(audioManagerDesc));

    // General Game window (loads embedded games such as Space Invaders)
    WindowTypeDesc gameDesc;
    gameDesc.typeId = "game-window";
    gameDesc.displayName = "Game Window";
    gameDesc.factory = [](WindowContext&) -> std::unique_ptr<IWindow> {
        return std::make_unique<GameWindow>();
    };
    reg.registerType(std::move(gameDesc));

    WindowTypeDesc hotkeysDesc;
    hotkeysDesc.typeId = "hotkeys";
    hotkeysDesc.displayName = "Hotkeys";
    hotkeysDesc.factory = [](WindowContext&) -> std::unique_ptr<IWindow> {
        return std::make_unique<HotkeysWindow>();
    };
    reg.registerType(std::move(hotkeysDesc));
}

WindowManager::WindowManager() {
    // Initialize window registry (no behavior change yet)
    RegisterBuiltinWindows(window_registry_);
    editor_window_restore_width_ = std::max(320, static_cast<int>(ConfigurationManager::getInt("window::width", 1280)));
    editor_window_restore_height_ = std::max(240, static_cast<int>(ConfigurationManager::getInt("window::height", 720)));
    // Try auto-load last layout if ImGui context is alive
    if (ImGui::GetCurrentContext() != nullptr) {
        loadLayout("last");
    }
    // Console buffer capacity can be adjusted by ConsoleLogWindow; no manager default needed.
}
WindowManager::~WindowManager() {
    shutdown();
}

void WindowManager::setFullscreenSession(FullscreenSession* session) {
    fullscreen_session_ = session;
}

void WindowManager::shutdown() {
    if (shutting_down_) return;
    shutting_down_ = true;
    syncHotkeySuppression(nullptr, false);
    // Modular windows own their resources; just clear containers
    windows_.clear();
    undock_requests_.clear();
    toasts_.clear();
}

const Layout& WindowManager::getLayout() const { return layout_; }

std::string WindowManager::createWindow(const std::string& title, std::optional<Size> initialSize) {
    ManagedWindow w{};
    w.id = "win-" + std::to_string(next_id_++);
    w.title = title.empty() ? w.id : title;
    if (initialSize.has_value() && initialSize->width > 0 && initialSize->height > 0) {
        w.initialSize = initialSize;
        w.minSize = initialSize;
    } else {
        w.initialSize = Size{512, 512};
    }
    windows_.push_back(std::move(w));
    gb2d::logging::LogManager::debug("Created window: {} (title: {})", w.id, w.title);
    return w.id;
}

std::string WindowManager::spawnWindowByType(const std::string& typeId,
                                             std::optional<std::string> desiredTitle,
                                             std::optional<Size> initialSize) {
    // Construct a minimal context; services can be wired here later
    WindowContext ctx{};
    ctx.pushToast = [this](const std::string& text, float seconds) {
        this->addToast(text, seconds);
    };
    auto impl = window_registry_.create(typeId, ctx);
    if (!impl) return {};
    ManagedWindow w{};
    w.id = "win-" + std::to_string(next_id_++);
    // Prefer provided title, else impl title, else fallback to typeId
    if (desiredTitle.has_value() && !desiredTitle->empty()) w.title = *desiredTitle;
    else {
        std::string t = impl->title();
        w.title = t.empty() ? typeId : t;
    }
    if (initialSize.has_value() && initialSize->width > 0 && initialSize->height > 0) {
        w.initialSize = initialSize;
        w.minSize = initialSize;
    } else {
        w.initialSize = Size{512, 512};
    }
    w.impl = std::move(impl);
    windows_.push_back(std::move(w));
    return windows_.back().id;
}

bool WindowManager::dockWindow(const std::string& windowId, const std::string& targetRegionId, DockPosition position) {
    // Find window to dock
    auto it = std::find_if(windows_.begin(), windows_.end(), [&](const ManagedWindow& w){ return w.id == windowId; });
    if (it == windows_.end()) return false;
    const std::string label = makeLabel(*it);

    // Resolve target node
    ImGuiID target_node = dockspace_id_;
    if (!targetRegionId.empty() && targetRegionId != "root") {
        // Try by managed id, then by title
        const ManagedWindow* tgt = nullptr;
        auto it2 = std::find_if(windows_.begin(), windows_.end(), [&](const ManagedWindow& w){ return w.id == targetRegionId; });
        if (it2 != windows_.end()) tgt = &(*it2);
        if (!tgt) {
            auto it3 = std::find_if(windows_.begin(), windows_.end(), [&](const ManagedWindow& w){ return w.title == targetRegionId; });
            if (it3 != windows_.end()) tgt = &(*it3);
        }
        if (tgt) {
            std::string tgt_label = makeLabel(*tgt);
            if (ImGuiWindow* imgui_win = ImGui::FindWindowByName(tgt_label.c_str())) {
                if (imgui_win->DockNode) target_node = imgui_win->DockNode->ID;
                else if (imgui_win->DockId != 0) target_node = imgui_win->DockId;
            }
        }
    }

    // Compute destination
    ImGuiDir dir;
    float ratio = 0.25f;
    bool as_tab = false;
    switch (position) {
        case DockPosition::Left:   dir = ImGuiDir_Left; break;
        case DockPosition::Right:  dir = ImGuiDir_Right; break;
        case DockPosition::Top:    dir = ImGuiDir_Up; break;
        case DockPosition::Bottom: dir = ImGuiDir_Down; break;
        case DockPosition::Center: default: as_tab = true; dir = ImGuiDir_None; break;
    }

    if (as_tab) {
        ImGui::DockBuilderDockWindow(label.c_str(), target_node);
        ImGui::DockBuilderFinish(dockspace_id_);
        return true;
    } else {
        // Min-size guard: estimate dockspace available and prevent tiny panes
        ImGuiDockNode* tgt_node = ImGui::DockBuilderGetNode(target_node);
        ImVec2 avail = ImVec2(0,0);
        if (tgt_node && tgt_node->HostWindow)
            avail = tgt_node->HostWindow->ContentRegionRect.GetSize();
        else
            avail = ImGui::GetMainViewport()->WorkSize;
        float minW = (float)min_dock_width_;
        float minH = (float)min_dock_height_;
        if (it->minSize.has_value()) {
            minW = (float)std::max(min_dock_width_, it->minSize->width);
            minH = (float)std::max(min_dock_height_, it->minSize->height);
        }
        bool block = false;
        if (dir == ImGuiDir_Left || dir == ImGuiDir_Right) {
            if (avail.x < (minW * 2.0f)) block = true;
            else {
                float lo = minW / avail.x;
                float hi = 1.0f - lo;
                ratio = std::max(lo, std::min(ratio, hi));
            }
        } else if (dir == ImGuiDir_Up || dir == ImGuiDir_Down) {
            if (avail.y < (minH * 2.0f)) block = true;
            else {
                float lo = minH / avail.y;
                float hi = 1.0f - lo;
                ratio = std::max(lo, std::min(ratio, hi));
            }
        }
        if (block) {
            addToast("Not enough space to split");
            gb2d::logging::LogManager::warn("Dock split blocked: insufficient space for {}", label);
            return false;
        }
        ImGuiID out_node = ImGui::DockBuilderSplitNode(target_node, dir, ratio, nullptr, &target_node);
        ImGui::DockBuilderDockWindow(label.c_str(), out_node);
        ImGui::DockBuilderFinish(dockspace_id_);
        return true;
    }
}

bool WindowManager::undockWindow(const std::string& windowId) {
    undock_requests_.insert(windowId);
    gb2d::logging::LogManager::debug("Undock requested for {}", windowId);
    return true;
}

bool WindowManager::closeWindow(const std::string& windowId) {
    auto it = std::find_if(windows_.begin(), windows_.end(), [&](const ManagedWindow& w){ return w.id == windowId; });
    if (it == windows_.end()) return false;
    windows_.erase(it);
    gb2d::logging::LogManager::debug("Closed window {}", windowId);
    return true;
}

bool WindowManager::reorderTabs(const std::string& /*regionId*/, const std::vector<std::string>& /*newOrder*/) { return false; }

bool WindowManager::resizeRegion(const std::string& /*regionId*/, int /*deltaWidth*/, int /*deltaHeight*/) { return false; }

bool WindowManager::saveLayout(const std::optional<std::string>& name) {
    const std::string layoutName = name.value_or("last");
    namespace fs = std::filesystem;
    try {
        const fs::path base = fs::path("out") / "layouts";
        fs::create_directories(base);

        const fs::path windowsPath = base / (layoutName + ".wm.txt");
        const fs::path imguiPath   = base / (layoutName + ".imgui.ini");
        const fs::path jsonPath    = base / (layoutName + ".layout.json");

        auto escape = [](const std::string& s) {
            std::string out;
            out.reserve(s.size());
            for (char c : s) {
                if (c == '\\' || c == '|' || c == '\n' || c == '\r') {
                    out.push_back('\\');
                }
                out.push_back(c);
            }
            return out;
        };

        std::ofstream ofs(windowsPath, std::ios::trunc);
        if (!ofs) return false;
        ofs << "next_id=" << next_id_ << "\n";
        ofs << "last_folder=" << escape(last_folder_) << "\n";
        // Persist console settings
    // Console settings are now serialized by ConsoleLogWindow implementation.
        if (!recent_files_.empty()) {
            ofs << "recent=";
            for (size_t i = 0; i < recent_files_.size(); ++i) {
                if (i) ofs << ";";
                ofs << escape(recent_files_[i]);
            }
            ofs << "\n";
        }
        for (const auto& w : windows_) {
            ofs << "id=" << w.id
                << "|title=" << escape(w.title)
                << "|open=" << (w.open ? 1 : 0)
                << "\n";
        }
        ofs.close();

        // New: Write JSON layout in parallel
        using nlohmann::json;
        json j;
        j["version"] = 1;
        j["nextId"] = next_id_;
        j["lastFolder"] = last_folder_;
        j["recentFiles"] = recent_files_;
        json jwins = json::array();
        for (const auto& w : windows_) {
            json jw;
            jw["id"] = w.id;
            jw["title"] = w.title;
            jw["open"] = w.open;
            if (w.minSize.has_value()) {
                jw["minSize"] = { {"width", w.minSize->width}, {"height", w.minSize->height} };
            }
            std::string type = w.impl ? std::string(w.impl->typeId()) : std::string();
            jw["type"] = type;
            if (w.impl) {
                json state;
                try { w.impl->serialize(state); } catch (...) {}
                jw["state"] = std::move(state);
            }
            jwins.push_back(std::move(jw));
        }
        j["windows"] = std::move(jwins);
        std::ofstream jofs(jsonPath, std::ios::trunc);
        if (jofs) {
            jofs << j.dump(2);
        }

        // Save ImGui dock/positions
        ImGui::SaveIniSettingsToDisk(imguiPath.string().c_str());
        addToast("Saved layout '" + layoutName + "'");
        gb2d::logging::LogManager::info("Saved layout '{}'", layoutName);
        return true;
    } catch (...) {
        gb2d::logging::LogManager::error("Failed saving layout '{}'", layoutName);
        return false;
    }
}

bool WindowManager::loadLayout(const std::string& name) {
    namespace fs = std::filesystem;
    const std::string layoutName = name.empty() ? std::string("last") : name;
    const fs::path base = fs::path("out") / "layouts";
    const fs::path windowsPath = base / (layoutName + ".wm.txt");
    const fs::path imguiPath   = base / (layoutName + ".imgui.ini");
    const fs::path jsonPath    = base / (layoutName + ".layout.json");

    bool loadedAny = false;

    // Prefer JSON layout if present
    if (fs::exists(jsonPath)) {
        try {
            using nlohmann::json;
            std::ifstream jifs(jsonPath);
            json j; jifs >> j;
            // Reset state
            windows_.clear();
            // Metadata
            if (j.contains("nextId")) {
                try { next_id_ = std::max(next_id_, j.value("nextId", next_id_)); } catch (...) {}
            }
            last_folder_ = j.value(std::string("lastFolder"), std::string(""));
            recent_files_.clear();
            if (j.contains("recentFiles") && j["recentFiles"].is_array()) {
                for (const auto& rf : j["recentFiles"]) {
                    try { recent_files_.push_back(rf.get<std::string>()); } catch (...) {}
                }
            }
            // Windows
            if (j.contains("windows") && j["windows"].is_array()) {
                for (const auto& jw : j["windows"]) {
                    try {
                        ManagedWindow w{};
                        w.id = jw.value(std::string("id"), std::string(""));
                        if (w.id.empty()) { w.id = "win-" + std::to_string(next_id_++); }
                        w.title = jw.value(std::string("title"), std::string(""));
                        w.open = jw.value("open", true);
                        if (jw.contains("minSize") && jw["minSize"].is_object()) {
                            try {
                                Size s{}; s.width = jw["minSize"].value("width", 0); s.height = jw["minSize"].value("height", 0);
                                if (s.width > 0 && s.height > 0) w.minSize = s;
                            } catch (...) {}
                        }
                        std::string type = jw.value(std::string("type"), std::string(""));
                        if (!type.empty()) {
                            WindowContext ctx{};
                            ctx.pushToast = [this](const std::string& text, float seconds) {
                                this->addToast(text, seconds);
                            };
                            auto impl = window_registry_.create(type, ctx);
                            if (impl) {
                                // Deserialize state before title sync so window may set its own title; then apply explicit title if provided
                                if (jw.contains("state")) {
                                    try { impl->deserialize(jw["state"]); } catch (...) {}
                                }
                                if (!w.title.empty()) { impl->setTitle(w.title); }
                                else { w.title = impl->title(); }
                                w.impl = std::move(impl);
                            } else {
                                gb2d::logging::LogManager::warn("Unknown window type '{}' when loading layout '{}'", type, layoutName);
                            }
                        } else {
                            // Legacy/plain window without impl
                            if (w.title.empty()) w.title = w.id;
                        }
                        windows_.push_back(std::move(w));
                    } catch (...) {
                        // Skip malformed window entries
                    }
                }
            }
            loadedAny = true;
        } catch (...) {
            gb2d::logging::LogManager::error("Failed reading JSON layout '{}'", layoutName);
        }
    }

    // Fallback: Load legacy windows metadata
    if (!loadedAny && fs::exists(windowsPath)) {
        auto unescape = [](const std::string& s) {
            std::string out;
            out.reserve(s.size());
            bool esc = false;
            for (char c : s) {
                if (esc) { out.push_back(c); esc = false; }
                else if (c == '\\') { esc = true; }
                else { out.push_back(c); }
            }
            return out;
        };

        std::ifstream ifs(windowsPath);
        if (ifs) {
            windows_.clear();
            std::string line;
            while (std::getline(ifs, line)) {
                if (line.rfind("next_id=", 0) == 0) {
                    next_id_ = std::max(next_id_, std::stoi(line.substr(8)));
                    continue;
                }
                if (line.rfind("last_folder=", 0) == 0) {
                    last_folder_ = unescape(line.substr(12));
                    continue;
                }
                // Console settings are now owned by ConsoleLogWindow (modular window) and not read here.
                if (line.rfind("recent=", 0) == 0) {
                    recent_files_.clear();
                    std::string rest = line.substr(7);
                    std::string cur;
                    bool esc = false;
                    for (char c : rest) {
                        if (esc) { cur.push_back(c); esc = false; }
                        else if (c == '\\') { esc = true; }
                        else if (c == ';') { if (!cur.empty()) { recent_files_.push_back(cur); cur.clear(); } }
                        else { cur.push_back(c); }
                    }
                    if (!cur.empty()) recent_files_.push_back(cur);
                    continue;
                }
                // id=<id>|title=<title>|open=<0/1>
                std::string id, title, openStr;
                size_t p1 = line.find("id=");
                size_t p2 = line.find("|title=");
                size_t p3 = line.find("|open=");
                if (p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos) {
                    id = line.substr(p1 + 3, p2 - (p1 + 3));
                    title = unescape(line.substr(p2 + 7, p3 - (p2 + 7)));
                    openStr = line.substr(p3 + 6);
                    ManagedWindow w{ id, title, openStr == "1" };
                    windows_.push_back(std::move(w));
                }
            }
            loadedAny = true;
        }
    }

    // Load ImGui layout if present
    if (std::filesystem::exists(imguiPath)) {
        ImGui::LoadIniSettingsFromDisk(imguiPath.string().c_str());
        layout_built_ = true; // skip default builder when a layout was loaded
        addToast("Loaded layout '" + layoutName + "'");
        gb2d::logging::LogManager::info("Loaded layout '{}'", layoutName);
        loadedAny = true;
    }

    return loadedAny;
}

void WindowManager::addToast(const std::string& text, float seconds) {
    toasts_.push_back(Toast{ text, seconds });
}

void WindowManager::updateToasts(float dt) {
    for (auto& t : toasts_) t.remaining -= dt;
    toasts_.erase(std::remove_if(toasts_.begin(), toasts_.end(), [](const Toast& t){ return t.remaining <= 0.0f; }), toasts_.end());
}

void WindowManager::renderToasts() {
    if (toasts_.empty()) return;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 pos = ImVec2(vp->WorkPos.x + vp->WorkSize.x - 10.0f, vp->WorkPos.y + 10.0f);
    ImGui::SetNextWindowBgAlpha(0.5f);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(1,0));
    ImGui::Begin("##toasts", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav);
    for (const auto& t : toasts_) {
        ImGui::TextUnformatted(t.text.c_str());
    }
    ImGui::End();
}

std::string WindowManager::makeLabel(const ManagedWindow& w) const {
    std::string visibleTitle = w.title.empty() ? "Window" : w.title;
    return visibleTitle + "###" + w.id; // keep visible title stable, ID after ###
}

void WindowManager::renderDockTargetsOverlay() {
    if (!dragging_window_id_.has_value()) return;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 center = ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f);
    const float r = 28.0f; // target half-size
    const float gap = 80.0f;
    struct Hit { ImRect rect; DockPosition pos; };
    std::vector<Hit> hits;
    auto mk = [&](ImVec2 c, DockPosition pos){ ImVec2 a(c.x - r, c.y - r), b(c.x + r, c.y + r); hits.push_back({ ImRect(a,b), pos }); };
    mk(center, DockPosition::Center);
    mk(ImVec2(center.x - gap, center.y), DockPosition::Left);
    mk(ImVec2(center.x + gap, center.y), DockPosition::Right);
    mk(ImVec2(center.x, center.y - gap), DockPosition::Top);
    mk(ImVec2(center.x, center.y + gap), DockPosition::Bottom);

    ImDrawList* dl = ImGui::GetForegroundDrawList(vp);
    const ImVec2 mouse = ImGui::GetMousePos();
    int hovered = -1;
    for (int i = 0; i < (int)hits.size(); ++i) {
        bool h = hits[i].rect.Contains(mouse);
        if (h) hovered = i;
        ImU32 fill = h ? IM_COL32(80,160,255,160) : IM_COL32(80,80,80,120);
        dl->AddRectFilled(hits[i].rect.Min, hits[i].rect.Max, fill, 6.0f);
        dl->AddRect(hits[i].rect.Min, hits[i].rect.Max, IM_COL32(255,255,255,180), 6.0f, 0, 2.0f);
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (hovered >= 0) {
            dockWindow(dragging_window_id_.value(), "root", hits[hovered].pos);
        }
        dragging_window_id_.reset();
    }
}

void WindowManager::toggleEditorFullscreen() {
    bool currentlyFullscreen = IsWindowFullscreen();
    setEditorFullscreen(!currentlyFullscreen);
}

void WindowManager::setEditorFullscreen(bool enable) {
    if (fullscreen_session_ && fullscreen_session_->isActive()) {
        gb2d::logging::LogManager::warn("Cannot toggle editor fullscreen while a game fullscreen session is active.");
        return;
    }

    bool currentlyFullscreen = IsWindowFullscreen();

    if (enable) {
        if (!currentlyFullscreen) {
            editor_window_restore_width_ = std::max(GetScreenWidth(), 320);
            editor_window_restore_height_ = std::max(GetScreenHeight(), 240);
            ConfigurationManager::set("window::width", static_cast<int64_t>(editor_window_restore_width_));
            ConfigurationManager::set("window::height", static_cast<int64_t>(editor_window_restore_height_));
            ToggleFullscreen();
            currentlyFullscreen = IsWindowFullscreen();
        } else {
            if (editor_window_restore_width_ <= 0 || editor_window_restore_height_ <= 0) {
                editor_window_restore_width_ = std::max(320, static_cast<int>(ConfigurationManager::getInt("window::width", 1280)));
                editor_window_restore_height_ = std::max(240, static_cast<int>(ConfigurationManager::getInt("window::height", 720)));
            }
        }

        if (!currentlyFullscreen) {
            gb2d::logging::LogManager::warn("Failed to enter editor fullscreen mode.");
            return;
        }

    int monitorIndex = GetCurrentMonitor();
    int monitorWidth = GetMonitorWidth(monitorIndex);
    int monitorHeight = GetMonitorHeight(monitorIndex);
    if (monitorWidth <= 0) monitorWidth = GetScreenWidth();
    if (monitorHeight <= 0) monitorHeight = GetScreenHeight();

    int fullscreenWidth = std::max(320, static_cast<int>(ConfigurationManager::getInt("fullscreen::width", monitorWidth > 0 ? monitorWidth : 1920)));
    int fullscreenHeight = std::max(240, static_cast<int>(ConfigurationManager::getInt("fullscreen::height", monitorHeight > 0 ? monitorHeight : 1080)));
        SetWindowSize(fullscreenWidth, fullscreenHeight);
        ConfigurationManager::set("fullscreen::width", static_cast<int64_t>(fullscreenWidth));
        ConfigurationManager::set("fullscreen::height", static_cast<int64_t>(fullscreenHeight));
        ConfigurationManager::set("window::fullscreen", true);
        ConfigurationManager::save();
        gb2d::logging::LogManager::info("Editor fullscreen enabled: {}x{}", fullscreenWidth, fullscreenHeight);
        return;
    }

    int targetWidth = editor_window_restore_width_ > 0
        ? editor_window_restore_width_
        : std::max(320, static_cast<int>(ConfigurationManager::getInt("window::width", 1280)));
    int targetHeight = editor_window_restore_height_ > 0
        ? editor_window_restore_height_
        : std::max(240, static_cast<int>(ConfigurationManager::getInt("window::height", 720)));

    if (currentlyFullscreen) {
        ToggleFullscreen();
    }

    SetWindowSize(targetWidth, targetHeight);
    editor_window_restore_width_ = targetWidth;
    editor_window_restore_height_ = targetHeight;
    ConfigurationManager::set("window::fullscreen", false);
    ConfigurationManager::set("window::width", static_cast<int64_t>(targetWidth));
    ConfigurationManager::set("window::height", static_cast<int64_t>(targetHeight));
    ConfigurationManager::save();
    gb2d::logging::LogManager::info("Editor fullscreen disabled: {}x{}", targetWidth, targetHeight);
}

void WindowManager::openFileDialog(const char* dialogTitle, const char* filters) {
    IGFD::FileDialogConfig cfg;
    cfg.path = last_folder_.empty() ? std::string(".") : last_folder_;
    cfg.flags = ImGuiFileDialogFlags_Modal;
    ImGuiFileDialog::Instance()->OpenDialog("FileOpenDlg", dialogTitle, filters, cfg);
}

void WindowManager::syncHotkeySuppression(const ImGuiIO* imguiIO, bool imguiFrameActive) {
    using gb2d::hotkeys::HotKeyManager;
    using gb2d::hotkeys::HotKeySuppressionReason;

    if (!HotKeyManager::isInitialized()) {
        hotkey_suppressed_text_input_ = false;
        hotkey_suppressed_modal_ = false;
        return;
    }

    auto applySuppression = [&](HotKeySuppressionReason reason, bool shouldBeActive, bool& flag) {
        if (shouldBeActive == flag) {
            return;
        }
        if (shouldBeActive) {
            HotKeyManager::pushSuppression(reason);
        } else {
            HotKeyManager::popSuppression(reason);
        }
        flag = shouldBeActive;
    };

    const bool wantsKeyboard = imguiIO && imguiIO->WantCaptureKeyboard;
    bool anyItemActive = false;
    if (imguiFrameActive && ImGui::GetCurrentContext() != nullptr) {
        anyItemActive = ImGui::IsAnyItemActive();
    }
    const bool textInputActive = imguiIO && (imguiIO->WantTextInput || (wantsKeyboard && anyItemActive));
    applySuppression(HotKeySuppressionReason::TextInput, textInputActive, hotkey_suppressed_text_input_);

    bool modalActive = false;
    if (imguiFrameActive && ImGui::GetCurrentContext() != nullptr) {
        modalActive = ImGui::GetTopMostPopupModal() != nullptr;
    }
    applySuppression(HotKeySuppressionReason::ModalDialog, modalActive, hotkey_suppressed_modal_);

    if (!imguiIO && !imguiFrameActive) {
        applySuppression(HotKeySuppressionReason::TextInput, false, hotkey_suppressed_text_input_);
        applySuppression(HotKeySuppressionReason::ModalDialog, false, hotkey_suppressed_modal_);
    }
}

void WindowManager::processGlobalHotkeys() {
    using gb2d::hotkeys::HotKeyManager;
    using namespace gb2d::hotkeys::actions;

    if (!HotKeyManager::isInitialized()) {
        return;
    }

    if (HotKeyManager::isSuppressed()) {
        return;
    }

    auto spawnOrFocusWindow = [this](const char* typeId, const std::string& defaultTitle) {
        ManagedWindow* existing = findByTypeId(typeId);
        if (!existing) {
            std::string id = spawnWindowByType(typeId, defaultTitle);
            if (!id.empty()) {
                focus_request_window_id_ = id;
            }
        } else {
            existing->open = true;
            focus_request_window_id_ = existing->id;
        }
    };

    const bool sessionActive = fullscreen_session_ && fullscreen_session_->isActive();

    if (HotKeyManager::consumeTriggered(OpenFileDialog)) {
        constexpr const char* kAllFileFilters = "Images{.png,.jpg,.jpeg,.bmp,.gif}, Text{.txt,.md,.log}, Code{.h,.hpp,.c,.cpp,.cmake}, .*";
        openFileDialog("Open File", kAllFileFilters);
    }

    if (HotKeyManager::consumeTriggered(OpenImageDialog)) {
        constexpr const char* kImageFilters = "Images{.png,.jpg,.jpeg,.bmp,.gif,.tga,.dds,.psd,.hdr}";
        openFileDialog("Open Image", kImageFilters);
    }

    if (HotKeyManager::consumeTriggered(ToggleEditorFullscreen)) {
        if (sessionActive) {
            addToast("Exit game fullscreen before toggling the editor view.");
        } else {
            toggleEditorFullscreen();
        }
    }

    if (HotKeyManager::consumeTriggered(FocusTextEditor)) {
        spawnOrFocusWindow("code-editor", "Text Editor");
    }

    if (HotKeyManager::consumeTriggered(ShowConsole)) {
        spawnOrFocusWindow("console-log", "Console");
    }

    if (HotKeyManager::consumeTriggered(SpawnDockWindow)) {
        createWindow("Window " + std::to_string(next_id_));
    }

    if (HotKeyManager::consumeTriggered(OpenConfigurationWindow)) {
        spawnOrFocusWindow("configuration", "Configuration");
    }

    if (HotKeyManager::consumeTriggered(OpenAudioManagerWindow)) {
        spawnOrFocusWindow("audio_manager", "Audio Manager");
    }

    if (HotKeyManager::consumeTriggered(OpenHotkeySettings)) {
        spawnOrFocusWindow("hotkeys", "Hotkeys");
    }

    if (HotKeyManager::consumeTriggered(SaveLayout)) {
        saveLayout();
    }

    if (HotKeyManager::consumeTriggered(OpenLayoutManager)) {
        addToast("Layout Manager UI not implemented yet.");
    }
}

void WindowManager::renderUI() {
    if (shutting_down_) return; // guard against late calls during teardown
    ImGuiIO& io = ImGui::GetIO();
    updateToasts(io.DeltaTime);
    processGlobalHotkeys();
    if (!(io.ConfigFlags & ImGuiConfigFlags_DockingEnable)) {
        ImGui::TextUnformatted("Docking is disabled. Enable ImGuiConfigFlags_DockingEnable.");
        return;
    }

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(2);

    dockspace_id_ = ImGui::GetID("MyDockSpace");
    ImGui::DockSpace(dockspace_id_, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

    buildDefaultLayoutIfNeeded();

    // Render DnD dock targets overlay when dragging
    renderDockTargetsOverlay();

    // Toolbar to create windows
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            auto launchFileDialog = [&](const char* dialogTitle, const char* filters) {
                IGFD::FileDialogConfig cfg;
                cfg.path = last_folder_.empty() ? std::string(".") : last_folder_;
                cfg.flags = ImGuiFileDialogFlags_Modal;
                ImGuiFileDialog::Instance()->OpenDialog("FileOpenDlg", dialogTitle, filters, cfg);
            };

            const std::string openFileShortcut = hotkeyShortcutLabel(hotkeys::actions::OpenFileDialog);
            if (ImGui::MenuItem("Open File...", shortcutArg(openFileShortcut))) {
                const char* filters = "Images{.png,.jpg,.jpeg,.bmp,.gif}, Text{.txt,.md,.log}, Code{.h,.hpp,.c,.cpp,.cmake}, .*";
                launchFileDialog("Open File", filters);
            }
            const std::string openImageShortcut = hotkeyShortcutLabel(hotkeys::actions::OpenImageDialog);
            if (ImGui::MenuItem("Open Image...", shortcutArg(openImageShortcut))) {
                const char* imageFilters = "Images{.png,.jpg,.jpeg,.bmp,.gif,.tga,.dds,.psd,.hdr}";
                launchFileDialog("Open Image", imageFilters);
            }
            if (ImGui::BeginMenu("Open Recent")) {
                if (recent_files_.empty()) {
                    ImGui::MenuItem("(empty)", nullptr, false, false);
                } else {
                    for (size_t i = 0; i < recent_files_.size(); ++i) {
                        const std::string& path = recent_files_[i];
                        if (ImGui::MenuItem(path.c_str())) {
                            addRecentFile(path);
                            addToast(std::string("Opened: ") + path);
                            try { last_folder_ = std::filesystem::path(path).parent_path().string(); } catch (...) {}

                            std::string ext;
                            try { ext = std::filesystem::path(path).extension().string(); }
                            catch (...) { ext.clear(); }
                            for (auto& c : ext) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

                            if (CodeEditorWindow::isTextLikeExtension(ext)) {
                                ManagedWindow* existing = findByTypeId("code-editor");
                                if (!existing) {
                                    std::string id = spawnWindowByType("code-editor", std::string("Text Editor"));
                                    if (!id.empty()) focus_request_window_id_ = id;
                                    existing = findByTypeId("code-editor");
                                }
                                if (existing && existing->impl) {
                                    if (auto* ce = dynamic_cast<CodeEditorWindow*>(existing->impl.get())) {
                                        ce->openFile(path);
                                    }
                                }
                            } else {
                                std::string id = spawnWindowByType("file-preview", std::string("Preview: ") + std::filesystem::path(path).filename().string());
                                if (!id.empty()) {
                                    ManagedWindow* w = nullptr;
                                    for (auto& mw : windows_) if (mw.id == id) { w = &mw; break; }
                                    if (w && w->impl) {
                                        if (auto* pv = dynamic_cast<FilePreviewWindow*>(w->impl.get())) pv->open(path);
                                    }
                                }
                            }
                            break;
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Clear Recent")) recent_files_.clear();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("GameBuilder")) {
            const std::string focusEditorShortcut = hotkeyShortcutLabel(hotkeys::actions::FocusTextEditor);
            if (ImGui::MenuItem("Text Editor", shortcutArg(focusEditorShortcut))) {
                ManagedWindow* existing = findByTypeId("code-editor");
                if (!existing) {
                    std::string id = spawnWindowByType("code-editor", std::string("Text Editor"));
                    if (!id.empty()) focus_request_window_id_ = id;
                } else {
                    existing->open = true;
                    focus_request_window_id_ = existing->id;
                }
            }
            bool sessionActive = fullscreen_session_ && fullscreen_session_->isActive();
            bool editorFullscreen = IsWindowFullscreen();
            const std::string toggleEditorFullscreenShortcut = hotkeyShortcutLabel(hotkeys::actions::ToggleEditorFullscreen);
            if (ImGui::MenuItem("Editor Fullscreen", shortcutArg(toggleEditorFullscreenShortcut), editorFullscreen, !sessionActive)) {
                toggleEditorFullscreen();
            }
            if (sessionActive && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("Exit game fullscreen before toggling the editor view.");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Windows")) {
            const std::string newWindowShortcut = hotkeyShortcutLabel(hotkeys::actions::SpawnDockWindow);
            if (ImGui::MenuItem("New Window", shortcutArg(newWindowShortcut))) {
                createWindow("Window " + std::to_string(next_id_));
            }
            const std::string consoleShortcut = hotkeyShortcutLabel(hotkeys::actions::ShowConsole);
            if (ImGui::MenuItem("Console", shortcutArg(consoleShortcut))) {
                // Spawn or focus the modular Console window (registry-driven)
                ManagedWindow* existing = findByTypeId("console-log");
                if (!existing) {
                    std::string id = spawnWindowByType("console-log", std::string("Console"));
                    if (!id.empty()) focus_request_window_id_ = id;
                } else {
                    existing->open = true;
                    focus_request_window_id_ = existing->id;
                }
            }
            if (ImGui::BeginMenu("Games")) {
                auto games = GameWindow::availableGames();
                if (games.empty()) {
                    ImGui::MenuItem("(none)", nullptr, false, false);
                } else {
                    for (const auto& [gameId, gameName] : games) {
                        if (ImGui::MenuItem(gameName.c_str())) {
                            ManagedWindow* existing = findByTypeId("game-window");
                            if (!existing) {
                                std::string id = spawnWindowByType("game-window", gameName);
                                if (!id.empty()) {
                                    focus_request_window_id_ = id;
                                    existing = findByTypeId("game-window");
                                }
                            }
                            if (existing) {
                                existing->open = true;
                                focus_request_window_id_ = existing->id;
                                if (existing->impl) {
                                    if (auto* gw = dynamic_cast<GameWindow*>(existing->impl.get())) {
                                        if (gw->setGameById(gameId)) {
                                            gw->setTitle(gameName);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("File Previews")) {
                bool anyPreview = false;
                for (auto& mw : windows_) {
                    if (!mw.impl || std::string(mw.impl->typeId()) != "file-preview") continue;
                    anyPreview = true;
                    ImGui::PushID(mw.id.c_str());
                    bool wasOpen = mw.open;
                    if (ImGui::MenuItem(mw.title.c_str(), nullptr, &mw.open)) {
                        if (!wasOpen && mw.open) {
                            focus_request_window_id_ = mw.id;
                        }
                    }
                    ImGui::PopID();
                }
                if (!anyPreview) {
                    ImGui::MenuItem("(none)", nullptr, false, false);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Settings")) {
            const std::string configurationShortcut = hotkeyShortcutLabel(hotkeys::actions::OpenConfigurationWindow);
            if (ImGui::MenuItem("Configuration...", shortcutArg(configurationShortcut))) {
                ManagedWindow* existing = findByTypeId("configuration");
                if (!existing) {
                    std::string id = spawnWindowByType("configuration", std::string("Configuration"));
                    if (!id.empty()) {
                        focus_request_window_id_ = id;
                    }
                } else {
                    existing->open = true;
                    focus_request_window_id_ = existing->id;
                }
            }
            const std::string audioManagerShortcut = hotkeyShortcutLabel(hotkeys::actions::OpenAudioManagerWindow);
            if (ImGui::MenuItem("Audio Manager...", shortcutArg(audioManagerShortcut))) {
                ManagedWindow* existing = findByTypeId("audio_manager");
                if (!existing) {
                    std::string id = spawnWindowByType("audio_manager", std::string("Audio Manager"));
                    if (!id.empty()) {
                        focus_request_window_id_ = id;
                    }
                } else {
                    existing->open = true;
                    focus_request_window_id_ = existing->id;
                }
            }
            ImGui::Separator();
            const std::string hotkeysShortcut = hotkeyShortcutLabel(hotkeys::actions::OpenHotkeySettings);
            if (ImGui::MenuItem("Hotkeys...", shortcutArg(hotkeysShortcut))) {
                ManagedWindow* existing = findByTypeId("hotkeys");
                if (!existing) {
                    std::string id = spawnWindowByType("hotkeys", std::string("Hotkeys"));
                    if (!id.empty()) {
                        focus_request_window_id_ = id;
                    }
                } else {
                    existing->open = true;
                    focus_request_window_id_ = existing->id;
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Layouts")) {
            static char nameBuf[64] = {0};
            ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf));
            ImGui::SameLine();
            bool hasName = nameBuf[0] != '\0';
            bool saveClicked = false;
            if (!hasName) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Button("Save")) {
                saveClicked = true;
            }
            if (!hasName) {
                ImGui::EndDisabled();
            }
            const std::string saveLayoutShortcut = hotkeyShortcutLabel(hotkeys::actions::SaveLayout);
            if (!saveLayoutShortcut.empty()) {
                ImGui::SameLine();
                ImGui::TextDisabled("%s", saveLayoutShortcut.c_str());
            }
            if (saveClicked && hasName) {
                saveLayout(std::string(nameBuf));
            }

            // List existing layouts
            namespace fs = std::filesystem;
            const fs::path base = fs::path("out") / "layouts";
            std::vector<std::string> layouts;
            if (fs::exists(base)) {
                for (auto& p : fs::directory_iterator(base)) {
                    if (!p.is_regular_file()) continue;
                    auto fname = p.path().filename().string();
                    // Accept both legacy and JSON layout files; dedupe names
                    auto add_if = [&](const std::string& suffix){
                        if (fname.size() > suffix.size() && fname.rfind(suffix) == fname.size() - suffix.size()) {
                            auto baseName = fname.substr(0, fname.size() - suffix.size());
                            if (std::find(layouts.begin(), layouts.end(), baseName) == layouts.end())
                                layouts.emplace_back(std::move(baseName));
                        }
                    };
                    add_if(".wm.txt");
                    add_if(".layout.json");
                }
            }
            static std::string pendingDelete;
            if (ImGui::BeginListBox("Saved")) {
                for (const auto& l : layouts) {
                    ImGui::PushID(l.c_str());
                    ImGui::TextUnformatted(l.c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Load")) { loadLayout(l); }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Delete")) {
                        pendingDelete = l;
                        ImGui::OpenPopup("Confirm Delete Layout");
                    }
                    ImGui::PopID();
                }
                ImGui::EndListBox();
            }
            if (ImGui::BeginPopupModal("Confirm Delete Layout", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Delete layout '%s'? This cannot be undone.", pendingDelete.c_str());
                ImGui::Separator();
                bool doClose = false;
                if (ImGui::Button("Delete", ImVec2(120,0))) {
                    try {
                        fs::remove(base / (pendingDelete + ".wm.txt"));
                        fs::remove(base / (pendingDelete + ".imgui.ini"));
                        fs::remove(base / (pendingDelete + ".layout.json"));
                        addToast("Deleted layout '" + pendingDelete + "'");
                    } catch (...) {}
                    doClose = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120,0))) {
                    doClose = true;
                }
                if (doClose) {
                    pendingDelete.clear();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // Render each managed window as an ImGui window (dockable)
    std::vector<std::string> filePreviewCloseQueue;
    for (auto& w : windows_) {
        bool isFilePreview = w.impl && w.impl->typeId() && std::strcmp(w.impl->typeId(), "file-preview") == 0;
        if (!w.open) {
            if (isFilePreview) filePreviewCloseQueue.push_back(w.id);
            continue;
        }
        // If modular impl exists, keep title in sync
        if (w.impl) {
            // Pull title from impl if different
            std::string implTitle = w.impl->title();
            if (!implTitle.empty() && implTitle != w.title) w.title = implTitle;
        }
        std::string label = makeLabel(w);
        bool open = w.open;

        ImVec2 firstUseSize(512.0f, 512.0f);
        if (w.initialSize.has_value()) {
            const Size& size = *w.initialSize;
            if (size.width > 0 && size.height > 0) {
                firstUseSize = ImVec2(static_cast<float>(size.width), static_cast<float>(size.height));
            }
        }
        ImGui::SetNextWindowSize(firstUseSize, ImGuiCond_FirstUseEver);

        WindowContext ctx{};
        const bool hasImpl = (w.impl != nullptr);
        if (hasImpl) {
            const std::string windowId = w.id;
            ctx.requestFocus = [this, windowId]() { this->focus_request_window_id_ = windowId; };
            ctx.requestUndock = [this, windowId]() { this->undock_requests_.insert(windowId); };
            ctx.requestClose = [this, &open, windowId]() {
                open = false;
                this->pending_close_requests_.push_back(windowId);
            };
            ctx.fullscreen = fullscreen_session_;
            ctx.pushToast = [this](const std::string& text, float seconds) {
                this->addToast(text, seconds);
            };
        }

        ImGuiWindowFlags windowFlags = 0;
        if (isFilePreview) windowFlags |= ImGuiWindowFlags_NoSavedSettings;
        if (ImGui::Begin(label.c_str(), &open, windowFlags)) {
            if (hasImpl) {
                // TODO: wire services into ctx as they are extracted
                w.impl->render(ctx);
            } else {
                ImGui::Text("ID: %s", w.id.c_str());
                if (ImGui::Button("Undock")) {
                    undock_requests_.insert(w.id);
                }
                ImGui::SameLine();
                if (ImGui::Button("Close")) {
                    cleanupPreview(w.id);
                    closeWindow(w.id);
                    ImGui::End();
                    break; // vector modified; restart next frame
                }
                // Quick docking controls relative to root
                if (ImGui::BeginMenuBar()) {
                    if (ImGui::BeginMenu("Dock")) {
                        if (ImGui::MenuItem("Left"))  dockWindow(w.id, "root", DockPosition::Left);
                        if (ImGui::MenuItem("Right")) dockWindow(w.id, "root", DockPosition::Right);
                        if (ImGui::MenuItem("Top"))   dockWindow(w.id, "root", DockPosition::Top);
                        if (ImGui::MenuItem("Bottom"))dockWindow(w.id, "root", DockPosition::Bottom);
                        if (ImGui::MenuItem("Center (Tab)")) dockWindow(w.id, "root", DockPosition::Center);
                        ImGui::EndMenu();
                    }
                    ImGui::SameLine();
                    // Drag handle: start drag-drop with our window id
                    ImGui::InvisibleButton("##drag_handle", ImVec2(16, 16));
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    ImVec2 pmin = ImGui::GetItemRectMin();
                    ImVec2 pmax = ImGui::GetItemRectMax();
                    dl->AddRect(pmin, pmax, IM_COL32(200,200,200,180));
                    dl->AddLine(ImVec2(pmin.x+4, pmin.y+5), ImVec2(pmax.x-4, pmin.y+5), IM_COL32(200,200,200,180), 1.0f);
                    dl->AddLine(ImVec2(pmin.x+4, pmin.y+9), ImVec2(pmax.x-4, pmin.y+9), IM_COL32(200,200,200,180), 1.0f);
                    dl->AddLine(ImVec2(pmin.x+4, pmin.y+13), ImVec2(pmax.x-4, pmin.y+13), IM_COL32(200,200,200,180), 1.0f);
                    if (ImGui::IsItemActive() || ImGui::IsItemHovered()) {
                        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                            dragging_window_id_ = w.id;
                            ImGui::SetDragDropPayload("GB2D_WIN_ID", w.id.c_str(), (int)w.id.size()+1);
                            ImGui::Text("Dock %s", w.title.c_str());
                            ImGui::EndDragDropSource();
                        }
                    } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                        if (dragging_window_id_ == w.id) dragging_window_id_.reset();
                    }
                    ImGui::EndMenuBar();
                }

                // Sample content per title
                if (!w.impl && w.title == "Scene") {
                    ImGui::TextUnformatted("Scene view placeholder");
                } else if (!w.impl && w.title == "Inspector") {
                    ImGui::TextUnformatted("Inspector placeholder");
                }
            }
        }
        ImGui::End();

        if (hasImpl && w.open && !open) {
            if (!w.impl->handleCloseRequest(ctx)) {
                open = true;
            } else {
                pending_close_requests_.push_back(w.id);
            }
        }

        w.open = open;
        if (!w.open && isFilePreview) {
            filePreviewCloseQueue.push_back(w.id);
        }
    }

    if (!pending_close_requests_.empty() || !filePreviewCloseQueue.empty()) {
        std::unordered_set<std::string> processed;
        processed.reserve(pending_close_requests_.size() + filePreviewCloseQueue.size());

        for (const auto& id : pending_close_requests_) {
            if (processed.insert(id).second) {
                cleanupPreview(id);
                closeWindow(id);
            }
        }
        pending_close_requests_.clear();

        for (const auto& id : filePreviewCloseQueue) {
            if (processed.insert(id).second) {
                cleanupPreview(id);
                closeWindow(id);
            }
        }
    }

    // Process undock requests: in ImGui, undock by setting next window dock id to 0
    for (const auto& id : undock_requests_) {
        for (auto& w : windows_) {
            if (w.id == id) {
                ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
            }
        }
    }
    undock_requests_.clear();

    // Process focus requests once windows are visible
    if (focus_request_window_id_.has_value()) {
        for (auto& w : windows_) {
            if (w.id == *focus_request_window_id_) {
                std::string label = makeLabel(w);
                if (ImGuiWindow* win = ImGui::FindWindowByName(label.c_str())) {
                    ImGui::FocusWindow(win);
                }
                break;
            }
        }
        focus_request_window_id_.reset();
    }

    // File dialog rendering and result handling
    if (ImGuiFileDialog::Instance()->Display("FileOpenDlg", ImGuiWindowFlags_NoCollapse, ImVec2(600, 400), ImVec2(FLT_MAX, FLT_MAX))) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            auto filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            addToast(std::string("Opened: ") + filePathName);
            try {
                last_folder_ = std::filesystem::path(filePathName).parent_path().string();
            } catch (...) {}
            addRecentFile(filePathName);
            // Route text-like files into the modular Code Editor; else spawn a File Preview window
            std::string ext;
            try { ext = std::filesystem::path(filePathName).extension().string(); } catch (...) { ext.clear(); }
            for (auto& c : ext) c = (char)tolower((unsigned char)c);
            bool isText = CodeEditorWindow::isTextLikeExtension(ext);
            if (isText) {
                ManagedWindow* existing = findByTypeId("code-editor");
                if (!existing) {
                    std::string id = spawnWindowByType("code-editor", std::string("Text Editor"));
                    if (!id.empty()) focus_request_window_id_ = id;
                    existing = findByTypeId("code-editor");
                }
                if (existing && existing->impl) {
                    if (auto* ce = dynamic_cast<CodeEditorWindow*>(existing->impl.get())) {
                        ce->openFile(filePathName);
                    }
                }
            } else {
                // Spawn a preview window per file
                std::string id = spawnWindowByType("file-preview", std::string("Preview: ") + std::filesystem::path(filePathName).filename().string());
                if (!id.empty()) {
                    ManagedWindow* w = nullptr;
                    for (auto& mw : windows_) if (mw.id == id) { w = &mw; break; }
                    if (w && w->impl) {
                        if (auto* pv = dynamic_cast<FilePreviewWindow*>(w->impl.get())) pv->open(filePathName);
                    }
                }
            }
            // Optionally, create a window showing the file name
            // createWindow(std::filesystem::path(filePathName).filename().string());
        }
        ImGuiFileDialog::Instance()->Close();
    }

    renderToasts();
    ImGui::End();
}

void WindowManager::cleanupPreview(const std::string& windowId) {
    (void)windowId; // no-op; textures owned by FilePreviewWindow now
}

void WindowManager::addRecentFile(const std::string& path, size_t cap) {
    // Deduplicate and move to front
    auto it = std::find(recent_files_.begin(), recent_files_.end(), path);
    if (it != recent_files_.end()) recent_files_.erase(it);
    recent_files_.insert(recent_files_.begin(), path);
    if (recent_files_.size() > cap) recent_files_.resize(cap);
}

void WindowManager::buildDefaultLayoutIfNeeded() {
    if (layout_built_) return;

    // Create initial windows if not present
    auto ensureWindow = [&](const char* title){
        if (!findByTitle(title)) createWindow(title);
    };
    ensureWindow("Scene");
    ensureWindow("Inspector");
    // Prefer modular Console window if registry has it; fallback to legacy title-based
    if (!findByTypeId("console-log")) {
        std::string id = spawnWindowByType("console-log", std::string("Console"));
        (void)id;
    }

    if (ConfigurationManager::getBool("window::resume_fullscreen", false)) {
        if (!findByTypeId("game-window")) {
            std::string id = spawnWindowByType("game-window", std::string("Game Window"));
            (void)id;
        }
    }

    // Build dock layout: split root into left (Scene), right (Inspector), bottom (Console)
    if (ImGui::DockBuilderGetNode(dockspace_id_) == nullptr)
        ImGui::DockBuilderAddNode(dockspace_id_, ImGuiDockNodeFlags_DockSpace);

    ImGui::DockBuilderRemoveNodeChildNodes(dockspace_id_);
    ImGuiID dock_main_id = dockspace_id_;
    ImGuiID dock_id_right = 0;
    ImGuiID dock_id_down = 0;

    dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
    dock_id_down  = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);

    const ManagedWindow* scene = findByTitle("Scene");
    const ManagedWindow* inspector = findByTitle("Inspector");
    const ManagedWindow* console = findByTitle("Console");
    if (scene)    ImGui::DockBuilderDockWindow(makeLabel(*scene).c_str(),    dock_main_id);
    if (inspector)ImGui::DockBuilderDockWindow(makeLabel(*inspector).c_str(),dock_id_right);
    if (console)  ImGui::DockBuilderDockWindow(makeLabel(*console).c_str(),  dock_id_down);

    ImGui::DockBuilderFinish(dockspace_id_);
    layout_built_ = true;
}

const WindowManager::ManagedWindow* WindowManager::findByTitle(const std::string& title) const {
    auto it = std::find_if(windows_.begin(), windows_.end(), [&](const ManagedWindow& w){ return w.title == title; });
    if (it == windows_.end()) return nullptr;
    return &(*it);
}

WindowManager::ManagedWindow* WindowManager::findByTitle(const std::string& title) {
    auto it = std::find_if(windows_.begin(), windows_.end(), [&](const ManagedWindow& w){ return w.title == title; });
    if (it == windows_.end()) return nullptr;
    return &(*it);
}

} // namespace gb2d
