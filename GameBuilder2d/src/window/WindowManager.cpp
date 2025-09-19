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

namespace gb2d {

WindowManager::WindowManager() {
    // Try auto-load last layout if ImGui context is alive
    if (ImGui::GetCurrentContext() != nullptr) {
        loadLayout("last");
    }
}
WindowManager::~WindowManager() = default;

const Layout& WindowManager::getLayout() const { return layout_; }

std::string WindowManager::createWindow(const std::string& title, std::optional<Size> initialSize) {
    ManagedWindow w{};
    w.id = "win-" + std::to_string(next_id_++);
    w.title = title.empty() ? w.id : title;
    if (initialSize.has_value()) w.minSize = initialSize;
    windows_.push_back(w);
    return w.id;
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
    return true;
}

bool WindowManager::closeWindow(const std::string& windowId) {
    auto it = std::find_if(windows_.begin(), windows_.end(), [&](const ManagedWindow& w){ return w.id == windowId; });
    if (it == windows_.end()) return false;
    windows_.erase(it);
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
        return true;
    } catch (...) {
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
        if (ImGui::BeginMenu("Windows")) {
            if (ImGui::MenuItem("New Window")) {
                createWindow("Window " + std::to_string(next_id_));
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
            } else if (w.title == "Console") {
                ImGui::TextUnformatted("Console output placeholder");
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

    // File dialog rendering and result handling
    if (ImGuiFileDialog::Instance()->Display("FileOpenDlg")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            auto filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
            addToast(std::string("Opened: ") + filePathName);
            try {
                last_folder_ = std::filesystem::path(filePathName).parent_path().string();
            } catch (...) {}
            addRecentFile(filePathName);
            openFilePreview(filePathName);
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
