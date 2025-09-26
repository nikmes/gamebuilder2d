#pragma once
#include <functional>
#include <string>

namespace gb2d {

namespace logging { class LogManager; }
class FileDialogService; // optional future wrapper
class RecentFilesService; // optional future wrapper
class Notifications; // optional future wrapper
struct Config; // optional

struct WindowContext {
    // Services (nullable for now; wire gradually)
    logging::LogManager* log{nullptr};
    FileDialogService* files{nullptr};
    RecentFilesService* recent{nullptr};
    Notifications* notify{nullptr};
    const Config* config{nullptr};

    // Manager interactions (bound to current window by manager when invoking)
    std::function<void()> requestFocus;   // focus this window
    std::function<void()> requestUndock;  // undock this window
    std::function<void()> requestClose;   // close this window
};

} // namespace gb2d
