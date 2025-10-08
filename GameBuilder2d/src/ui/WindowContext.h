#pragma once
#include <functional>
#include <string>

namespace gb2d {

namespace logging { class LogManager; }
class FileDialogService; // optional future wrapper
class RecentFilesService; // optional future wrapper
class Notifications; // optional future wrapper
class FullscreenSession; // forward decl for fullscreen controller
namespace games { class Game; }
struct Config; // optional

struct WindowContext {
    // Services (nullable for now; wire gradually)
    logging::LogManager* log{nullptr};
    FileDialogService* files{nullptr};
    RecentFilesService* recent{nullptr};
    Notifications* notify{nullptr};
    const Config* config{nullptr};

    // Runtime subsystems
    FullscreenSession* fullscreen{nullptr};

    // Manager interactions (bound to current window by manager when invoking)
    std::function<void()> requestFocus;   // focus this window
    std::function<void()> requestUndock;  // undock this window
    std::function<void()> requestClose;   // close this window
    std::function<void(const std::string&, float)> pushToast; // transient toast notifications
};

} // namespace gb2d
