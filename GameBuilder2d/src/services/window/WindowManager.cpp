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
#include "services/logger/LogManager.h"
// ImGuiColorTextEdit
#include <TextEditor.h>

namespace gb2d {

WindowManager::WindowManager() {
    // Try auto-load last layout if ImGui context is alive
    if (ImGui::GetCurrentContext() != nullptr) {
        loadLayout("last");
    }
    // Apply default console buffer capacity at startup
    gb2d::logging::set_log_buffer_capacity(console_buffer_cap_);
}
WindowManager::~WindowManager() {
    shutdown();
}

void WindowManager::shutdown() {
    if (shutting_down_) return;
    shutting_down_ = true;

    // Release editor tabs explicitly (TextEditor may reference ImGui state; do this before ImGui shutdown)
    editor_.tabs.clear();
    editor_.current = -1;

    // Clear previews & unload any image textures still held
    for (auto& kv : previews_) {
        Preview& p = kv.second;
        if (p.kind == Preview::Kind::Image && p.loaded && p.texId != 0) {
            Texture2D tex; tex.id = p.texId; tex.width = p.imgWidth; tex.height = p.imgHeight; tex.mipmaps = 1; tex.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            UnloadTexture(tex);
        }
    }
    previews_.clear();

    // Windows, toasts, etc.
    windows_.clear();
    undock_requests_.clear();
    toasts_.clear();
}

const Layout& WindowManager::getLayout() const { return layout_; }

std::string WindowManager::createWindow(const std::string& title, std::optional<Size> initialSize) {
    ManagedWindow w{};
    w.id = "win-" + std::to_string(next_id_++);
    w.title = title.empty() ? w.id : title;
    if (initialSize.has_value()) w.minSize = initialSize;
    windows_.push_back(w);
    gb2d::logging::LogManager::debug("Created window: {} (title: {})", w.id, w.title);
    return w.id;
}

bool WindowManager::setWindowTitle(const std::string& windowId, const std::string& newTitle) {
    auto it = std::find_if(windows_.begin(), windows_.end(), [&](const ManagedWindow& w){ return w.id == windowId; });
    if (it == windows_.end()) return false;
    if (newTitle.empty() || newTitle == it->title) return true; // treat empty as no-op (validation handled by caller)
    gb2d::logging::LogManager::debug("Renaming window {}: '{}' -> '{}'", windowId, it->title, newTitle);
    it->title = newTitle;
    return true;
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
        ofs << "console_autoscroll=" << (console_autoscroll_ ? 1 : 0) << "\n";
        ofs << "console_max_lines=" << console_max_lines_ << "\n";
        ofs << "console_buffer_cap=" << console_buffer_cap_ << "\n";
        ofs << "console_level_mask=" << console_level_mask_ << "\n";
        ofs << "console_text_filter=" << escape(console_text_filter_) << "\n";
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

    bool loadedAny = false;

    // Load windows metadata
    if (fs::exists(windowsPath)) {
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
                if (line.rfind("console_autoscroll=", 0) == 0) {
                    console_autoscroll_ = (std::stoi(line.substr(19)) != 0);
                    continue;
                }
                if (line.rfind("console_max_lines=", 0) == 0) {
                    console_max_lines_ = std::max(100, std::stoi(line.substr(18)));
                    continue;
                }
                if (line.rfind("console_buffer_cap=", 0) == 0) {
                    int v = std::stoi(line.substr(19));
                    if (v < 1000) v = 1000;
                    console_buffer_cap_ = (size_t)v;
                    gb2d::logging::set_log_buffer_capacity(console_buffer_cap_);
                    continue;
                }
                if (line.rfind("console_level_mask=", 0) == 0) {
                    uint32_t v = 0;
                    try { v = (uint32_t)std::stoul(line.substr(20)); } catch (...) { v = 0x3F; }
                    if (v == 0) v = 0x3F;
                    console_level_mask_ = v;
                    continue;
                }
                if (line.rfind("console_text_filter=", 0) == 0) {
                    console_text_filter_ = unescape(line.substr(20));
                    continue;
                }
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
                    windows_.push_back(w);
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
    return w.title + "###" + w.id; // keep visible title stable, ID after ###
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

void WindowManager::renderUI() {
    if (shutting_down_) return; // avoid rendering during teardown
    ImGuiIO& io = ImGui::GetIO();
    if (!(io.ConfigFlags & ImGuiConfigFlags_DockingEnable)) {
        ImGui::TextUnformatted("Docking is disabled. Enable ImGuiConfigFlags_DockingEnable.");
        return;
    }
    updateToasts(io.DeltaTime);

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
            if (ImGui::MenuItem("Open...")) {
                IGFD::FileDialogConfig cfg;
                cfg.path = last_folder_.empty() ? std::string(".") : last_folder_;
                const char* filters = "Images{.png,.jpg,.jpeg,.bmp,.gif}, Text{.txt,.md,.log}, Code{.h,.hpp,.c,.cpp,.cmake}, .*";
                ImGuiFileDialog::Instance()->OpenDialog("FileOpenDlg", "Open File", filters, cfg);
            }
            if (ImGui::BeginMenu("Open Recent")) {
                if (recent_files_.empty()) {
                    ImGui::MenuItem("(empty)", nullptr, false, false);
                } else {
                    for (size_t i = 0; i < recent_files_.size(); ++i) {
                        const std::string& p = recent_files_[i];
                        if (ImGui::MenuItem(p.c_str())) {
                            addRecentFile(p);
                            addToast(std::string("Opened: ") + p);
                            try { last_folder_ = std::filesystem::path(p).parent_path().string(); } catch (...) {}
                            openFilePreview(p);
                        }
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Clear Recent")) recent_files_.clear();
                ImGui::EndMenu();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("GameBuilder")) {
            if (ImGui::MenuItem("Text Editor")) {
                ensureEditorWindow(true);
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Windows")) {
            if (ImGui::MenuItem("New Window")) {
                createWindow("Window " + std::to_string(next_id_));
            }
            if (ImGui::MenuItem("Console")) {
                // Ensure a Console window exists; reopen if closed; request focus
                ManagedWindow* console = findByTitle("Console");
                if (!console) {
                    createWindow("Console");
                    console = findByTitle("Console");
                }
                if (console) {
                    console->open = true;
                    focus_request_window_id_ = console->id;
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Layouts")) {
            static char nameBuf[64] = {0};
            ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf));
            ImGui::SameLine();
            bool hasName = nameBuf[0] != '\0';
            if (!hasName) ImGui::BeginDisabled();
            if (ImGui::Button("Save")) {
                saveLayout(std::string(nameBuf));
            }
            if (!hasName) ImGui::EndDisabled();

            // List existing layouts
            namespace fs = std::filesystem;
            const fs::path base = fs::path("out") / "layouts";
            std::vector<std::string> layouts;
            if (fs::exists(base)) {
                for (auto& p : fs::directory_iterator(base)) {
                    if (!p.is_regular_file()) continue;
                    auto fname = p.path().filename().string();
                    const std::string suffix = ".wm.txt";
                    if (fname.size() > suffix.size() && fname.ends_with(suffix)) {
                        layouts.emplace_back(fname.substr(0, fname.size() - suffix.size()));
                    }
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
    for (auto& w : windows_) {
        if (!w.open) continue;
        std::string label = makeLabel(w);
        bool open = w.open;
        if (ImGui::Begin(label.c_str(), &open)) {
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
            if (w.title == "Scene") {
                ImGui::TextUnformatted("Scene view placeholder");
            } else if (w.title == "Inspector") {
                ImGui::TextUnformatted("Inspector placeholder");
            } else if (w.title == "Text Editor") {
                renderEditorWindow();
            } else if (w.title == "Console") {
                // Controls row: level filter, search, max lines, buffer cap, autoscroll, clear
                ImGui::SetNextItemWidth(120);
                ImGui::InputInt("Max lines", &console_max_lines_);
                if (console_max_lines_ < 100) console_max_lines_ = 100;
                ImGui::SameLine();
                ImGui::SetNextItemWidth(120);
                static int bufCapTmp = 0;
                bufCapTmp = (int)console_buffer_cap_;
                if (ImGui::InputInt("Buffer cap", &bufCapTmp)) {
                    if (bufCapTmp < 1000) bufCapTmp = 1000;
                    console_buffer_cap_ = (size_t)bufCapTmp;
                    gb2d::logging::set_log_buffer_capacity(console_buffer_cap_);
                }
                ImGui::SameLine();
                ImGui::Checkbox("Autoscroll", &console_autoscroll_);
                ImGui::SameLine();
                if (ImGui::Button("Clear")) { gb2d::logging::clear_log_buffer(); }

                // Level filter as toggle buttons
                auto lvlBtn = [&](const char* label, uint32_t bit){
                    bool on = (console_level_mask_ & bit) != 0;
                    if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.6f,0.2f,1.0f));
                    if (ImGui::SmallButton(label)) {
                        console_level_mask_ ^= bit;
                        if ((console_level_mask_ & 0x3F) == 0) console_level_mask_ = 0x3F; // avoid empty
                    }
                    if (on) ImGui::PopStyleColor();
                    ImGui::SameLine();
                };
                lvlBtn("Trace", 1u<<0);
                lvlBtn("Debug", 1u<<1);
                lvlBtn("Info",  1u<<2);
                lvlBtn("Warn",  1u<<3);
                lvlBtn("Error", 1u<<4);
                lvlBtn("Crit",  1u<<5);
                ImGui::NewLine();

                // Text search
                ImGui::SetNextItemWidth(300);
                char filterBuf[256];
                std::strncpy(filterBuf, console_text_filter_.c_str(), sizeof(filterBuf));
                filterBuf[sizeof(filterBuf)-1] = '\0';
                if (ImGui::InputText("##filter", filterBuf, IM_ARRAYSIZE(filterBuf))) {
                    console_text_filter_ = filterBuf;
                }

                // Render lines
                auto lines = gb2d::logging::read_log_lines_snapshot((size_t)console_max_lines_);
                ImGui::Separator();
                ImGui::BeginChild("log_lines", ImVec2(0,0), false, ImGuiWindowFlags_HorizontalScrollbar);
                for (const auto& ln : lines) {
                    uint32_t bit = 0;
                    switch (ln.level) {
                        case gb2d::logging::Level::trace: bit = 1u<<0; break;
                        case gb2d::logging::Level::debug: bit = 1u<<1; break;
                        case gb2d::logging::Level::info:  bit = 1u<<2; break;
                        case gb2d::logging::Level::warn:  bit = 1u<<3; break;
                        case gb2d::logging::Level::err:   bit = 1u<<4; break;
                        case gb2d::logging::Level::critical: bit = 1u<<5; break;
                        case gb2d::logging::Level::off: break;
                    }
                    if ((console_level_mask_ & bit) == 0) continue;
                    if (!console_text_filter_.empty()) {
                        // Case-insensitive substring match
                        auto hay = ln.text;
                        auto needle = console_text_filter_;
                        std::transform(hay.begin(), hay.end(), hay.begin(), [](unsigned char c){ return (char)tolower(c); });
                        std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char c){ return (char)tolower(c); });
                        if (hay.find(needle) == std::string::npos) continue;
                    }

                    ImVec4 col = ImVec4(0.8f,0.8f,0.8f,1.0f);
                    switch (ln.level) {
                        case gb2d::logging::Level::trace: col = ImVec4(0.6f,0.6f,0.6f,1.0f); break;
                        case gb2d::logging::Level::debug: col = ImVec4(0.7f,0.7f,0.9f,1.0f); break;
                        case gb2d::logging::Level::info: col = ImVec4(0.8f,0.8f,0.8f,1.0f); break;
                        case gb2d::logging::Level::warn: col = ImVec4(1.0f,0.9f,0.6f,1.0f); break;
                        case gb2d::logging::Level::err: col = ImVec4(1.0f,0.6f,0.6f,1.0f); break;
                        case gb2d::logging::Level::critical: col = ImVec4(1.0f,0.3f,0.3f,1.0f); break;
                        case gb2d::logging::Level::off: break;
                    }
                    ImGui::PushStyleColor(ImGuiCol_Text, col);
                    ImGui::Text("[%s] %s", gb2d::logging::level_to_label(ln.level), ln.text.c_str());
                    ImGui::PopStyleColor();
                }

                if (console_autoscroll_) {
                    float at_bottom = ImGui::GetScrollY() - ImGui::GetScrollMaxY();
                    if (at_bottom >= -2.0f) ImGui::SetScrollHereY(1.0f);
                }
                ImGui::EndChild();
            } else if (w.title.rfind("Preview:", 0) == 0) {
                auto itp = previews_.find(w.id);
                if (itp != previews_.end()) {
                    Preview& p = itp->second;
                    ImGui::TextUnformatted(p.path.c_str());
                    if (p.kind == Preview::Kind::Text) {
                        ImGui::Separator();
                        ImGui::BeginChild("text", ImVec2(0,0), false, ImGuiWindowFlags_HorizontalScrollbar);
                        ImGui::TextUnformatted(p.text.c_str());
                        ImGui::EndChild();
                    } else if (p.kind == Preview::Kind::Image && p.loaded && p.texId != 0) {
                        float availW = ImGui::GetContentRegionAvail().x;
                        float scale = (float) p.imgWidth > 0 ? availW / (float)p.imgWidth : 1.0f;
                        ImGui::Image((ImTextureID)(intptr_t)p.texId, ImVec2(p.imgWidth * scale, p.imgHeight * scale));
                    } else {
                        ImGui::TextUnformatted("(no preview)");
                    }
                } else {
                    ImGui::TextUnformatted("(preview missing)");
                }
            }
        }
        ImGui::End();
        w.open = open;
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
    if (ImGuiFileDialog::Instance()->Display("FileOpenDlg")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            auto filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            addToast(std::string("Opened: ") + filePathName);
            try {
                last_folder_ = std::filesystem::path(filePathName).parent_path().string();
            } catch (...) {}
            addRecentFile(filePathName);
            // Route text-like files into the editor
            std::string ext;
            try { ext = std::filesystem::path(filePathName).extension().string(); } catch (...) { ext.clear(); }
            for (auto& c : ext) c = (char)tolower((unsigned char)c);
            if (isTextLikeExtension(ext)) {
                ensureEditorWindow(true);
                openEditorFile(filePathName);
            } else {
                openFilePreview(filePathName);
            }
            // Optionally, create a window showing the file name
            // createWindow(std::filesystem::path(filePathName).filename().string());
        }
        ImGuiFileDialog::Instance()->Close();
    }

    renderToasts();
    ImGui::End();
}

void WindowManager::openFilePreview(const std::string& path) {
    std::string ext;
    try { ext = std::filesystem::path(path).extension().string(); }
    catch (...) { ext.clear(); }
    auto toLower = [](std::string s){ for (auto& c : s) c = (char)tolower((unsigned char)c); return s; };
    ext = toLower(ext);

    ManagedWindow w{};
    w.id = "win-" + std::to_string(next_id_++);
    w.title = std::string("Preview: ") + std::filesystem::path(path).filename().string();
    windows_.push_back(w);

    Preview p{};
    p.path = path;
    if (ext == ".txt" || ext == ".md" || ext == ".log" || ext == ".cmake" || ext == ".h" || ext == ".hpp" || ext == ".c" || ext == ".cpp") {
        p.kind = Preview::Kind::Text;
        std::ifstream ifs(path);
        std::ostringstream oss;
        oss << ifs.rdbuf();
        p.text = oss.str();
        p.loaded = true;
    } else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif") {
        p.kind = Preview::Kind::Image;
        Image img = LoadImage(path.c_str());
        if (img.data) {
            Texture2D tex = LoadTextureFromImage(img);
            p.texId = tex.id;
            p.imgWidth = tex.width;
            p.imgHeight = tex.height;
            p.loaded = true;
            UnloadImage(img);
        }
    }
    previews_.emplace(w.id, std::move(p));
}

void WindowManager::cleanupPreview(const std::string& windowId) {
    auto it = previews_.find(windowId);
    if (it != previews_.end()) {
        Preview& p = it->second;
        if (p.kind == Preview::Kind::Image && p.loaded && p.texId != 0) {
            Texture2D tex; tex.id = p.texId; tex.width = p.imgWidth; tex.height = p.imgHeight; tex.mipmaps = 1; tex.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
            UnloadTexture(tex);
        }
        previews_.erase(it);
    }
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
    ensureWindow("Console");

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

// ===== Editor helpers implementation =====
namespace {
    inline std::string toLower(std::string s){ for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
}

void gb2d::WindowManager::ensureEditorWindow(bool focus) {
    if (!editor_.exists) {
        std::string id = createWindow("Text Editor");
        editor_.exists = true;
        editor_.open = true;
        editor_.id = id;
        editor_.current = -1;
    } else {
        if (ManagedWindow* e = findByTitle("Text Editor")) e->open = true;
    }
    if (focus) focus_request_window_id_ = editor_.id;
}

bool gb2d::WindowManager::isTextLikeExtension(const std::string& ext) {
    static const char* exts[] = { ".txt", ".md", ".log", ".cmake", ".ini", ".json", ".yaml", ".yml",
        ".h", ".hpp", ".c", ".cpp", ".cc", ".cxx", ".glsl", ".vert", ".frag", ".hlsl", ".lua", ".sql" };
    for (auto* e : exts) if (ext == e) return true; return false;
}

const TextEditor::LanguageDefinition& gb2d::WindowManager::languageForExtension(const std::string& ext, std::string& outName) {
    std::string e = toLower(ext);
    if (e == ".h" || e == ".hpp" || e == ".c" || e == ".cpp" || e == ".cc" || e == ".cxx") { outName = "C/C++"; return TextEditor::LanguageDefinition::CPlusPlus(); }
    if (e == ".glsl" || e == ".vert" || e == ".frag") { outName = "GLSL"; return TextEditor::LanguageDefinition::GLSL(); }
    if (e == ".hlsl") { outName = "HLSL"; return TextEditor::LanguageDefinition::HLSL(); }
    if (e == ".c") { outName = "C"; return TextEditor::LanguageDefinition::C(); }
    if (e == ".sql") { outName = "SQL"; return TextEditor::LanguageDefinition::SQL(); }
    if (e == ".lua") { outName = "Lua"; return TextEditor::LanguageDefinition::Lua(); }
    outName = "Plain"; return TextEditor::LanguageDefinition::CPlusPlus();
}

void gb2d::WindowManager::openEditorFile(const std::string& path) {
    ensureEditorWindow(true);
    // If already open, focus it
    for (int i = 0; i < (int)editor_.tabs.size(); ++i) {
        if (editor_.tabs[i].path == path) { editor_.current = i; return; }
    }
    EditorTab tab;
    tab.path = path;
    tab.title = std::filesystem::path(path).filename().string();
    tab.editor = std::make_unique<TextEditor>();
    tab.editor->SetShowWhitespaces(false);
    tab.editor->SetPalette(TextEditor::GetDarkPalette());
    std::string ext;
    try { ext = std::filesystem::path(path).extension().string(); } catch (...) { ext.clear(); }
    tab.editor->SetLanguageDefinition(languageForExtension(ext, tab.langName));
    // Load content
    try {
        std::ifstream ifs(path, std::ios::binary);
        std::ostringstream oss; oss << ifs.rdbuf();
        tab.editor->SetText(oss.str());
        tab.dirty = false;
    } catch (...) {
        tab.editor->SetText("");
        tab.dirty = false;
    }
    editor_.tabs.push_back(std::move(tab));
    editor_.current = (int)editor_.tabs.size() - 1;
}

bool gb2d::WindowManager::saveEditorTab(int index, bool saveAs) {
    if (index < 0 || index >= (int)editor_.tabs.size()) return false;
    auto& t = editor_.tabs[index];
    std::string savePath = t.path;
    if (saveAs || savePath.empty()) {
        IGFD::FileDialogConfig cfg; cfg.path = last_folder_.empty() ? std::string(".") : last_folder_;
        ImGuiFileDialog::Instance()->OpenDialog("EditorSaveAsDlg", "Save File As", ".*", cfg);
        return false;
    }
    try {
        std::ofstream ofs(savePath, std::ios::binary | std::ios::trunc);
        auto content = t.editor->GetText();
        ofs.write(content.data(), (std::streamsize)content.size());
        t.dirty = false;
        addToast("Saved: " + savePath);
        addRecentFile(savePath);
        return true;
    } catch (...) {
        addToast("Failed to save: " + savePath);
        return false;
    }
}

void gb2d::WindowManager::renderEditorWindow() {
    // Editor menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) {
                EditorTab tab; tab.title = "Untitled"; tab.editor = std::make_unique<TextEditor>();
                tab.editor->SetPalette(TextEditor::GetDarkPalette()); tab.editor->SetShowWhitespaces(false);
                tab.editor->SetLanguageDefinition(TextEditor::LanguageDefinition::CPlusPlus());
                tab.editor->SetText(""); tab.dirty = false;
                editor_.tabs.push_back(std::move(tab)); editor_.current = (int)editor_.tabs.size() - 1;
            }
            if (ImGui::MenuItem("Open...")) {
                IGFD::FileDialogConfig cfg; cfg.path = last_folder_.empty() ? std::string(".") : last_folder_;
                ImGuiFileDialog::Instance()->OpenDialog("EditorOpenDlg", "Open File", ".*", cfg);
            }
            bool canSave = editor_.current >= 0 && editor_.current < (int)editor_.tabs.size();
            if (!canSave) ImGui::BeginDisabled();
            if (ImGui::MenuItem("Save")) { saveEditorTab(editor_.current, false); }
            if (ImGui::MenuItem("Save As...")) { saveEditorTab(editor_.current, true); }
            if (!canSave) ImGui::EndDisabled();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Theme")) {
            bool canApply = editor_.current >= 0 && editor_.current < (int)editor_.tabs.size();
            if (!canApply) ImGui::BeginDisabled();
            if (ImGui::MenuItem("Dark") && canApply) editor_.tabs[editor_.current].editor->SetPalette(TextEditor::GetDarkPalette());
            if (ImGui::MenuItem("Light") && canApply) editor_.tabs[editor_.current].editor->SetPalette(TextEditor::GetLightPalette());
            if (ImGui::MenuItem("Retro Blue") && canApply) editor_.tabs[editor_.current].editor->SetPalette(TextEditor::GetRetroBluePalette());
            if (!canApply) ImGui::EndDisabled();
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Tabs body
    if (ImGui::BeginTabBar("EditorTabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
        for (int i = 0; i < (int)editor_.tabs.size(); ++i) {
            auto& t = editor_.tabs[i];
            std::string label = t.title; if (t.dirty) label += " *";
            if (ImGui::BeginTabItem(label.c_str(), nullptr)) {
                editor_.current = i;
                if (t.editor->IsTextChanged()) t.dirty = true;
                ImGui::TextUnformatted(t.path.empty() ? "(unsaved)" : t.path.c_str());
                if (!t.langName.empty()) { ImGui::SameLine(); ImGui::TextDisabled("[%s]", t.langName.c_str()); }
                t.editor->Render("##text");
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    // Editor Open dialog results
    if (ImGuiFileDialog::Instance()->Display("EditorOpenDlg")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            auto path = ImGuiFileDialog::Instance()->GetFilePathName();
            addRecentFile(path);
            try { last_folder_ = std::filesystem::path(path).parent_path().string(); } catch (...) {}
            openEditorFile(path);
        }
        ImGuiFileDialog::Instance()->Close();
    }

    // Editor Save As dialog results
    if (ImGuiFileDialog::Instance()->Display("EditorSaveAsDlg")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            auto savePath = ImGuiFileDialog::Instance()->GetFilePathName();
            if (editor_.current >= 0 && editor_.current < (int)editor_.tabs.size()) {
                auto& t = editor_.tabs[editor_.current];
                try {
                    std::ofstream ofs(savePath, std::ios::binary | std::ios::trunc);
                    auto content = t.editor->GetText();
                    ofs.write(content.data(), (std::streamsize)content.size());
                    t.dirty = false;
                    t.path = savePath;
                    t.title = std::filesystem::path(savePath).filename().string();
                    std::string ext; try { ext = std::filesystem::path(savePath).extension().string(); } catch (...) { ext.clear(); }
                    t.editor->SetLanguageDefinition(languageForExtension(ext, t.langName));
                    addToast("Saved: " + savePath);
                    addRecentFile(savePath);
                } catch (...) {
                    addToast("Failed to save: " + savePath);
                }
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }
}
