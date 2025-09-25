#pragma once
#include <string>
#include <string_view>
#include <memory>
#include <vector>
#include <spdlog/fmt/fmt.h>

namespace spdlog { class logger; }

namespace gb2d::logging {

enum class Level { trace, debug, info, warn, err, critical, off };

struct Config {
    std::string name = "GB2D";
    Level level = Level::info;
    std::string pattern = "[%H:%M:%S] [%l] %v";
};

enum class Status { ok, already_initialized, not_initialized, error };

class LogManager {
public:
    static Status init(const Config& cfg = {});
    static bool isInitialized();
    static Status reconfigure(const Config& cfg);
    static Status shutdown();

    template <typename... Args>
    static void trace(std::string_view fmt, Args&&... args) { log(Level::trace, fmt, std::forward<Args>(args)...); }
    template <typename... Args>
    static void debug(std::string_view fmt, Args&&... args) { log(Level::debug, fmt, std::forward<Args>(args)...); }
    template <typename... Args>
    static void info(std::string_view fmt, Args&&... args)  { log(Level::info,  fmt, std::forward<Args>(args)...); }
    template <typename... Args>
    static void warn(std::string_view fmt, Args&&... args)  { log(Level::warn,  fmt, std::forward<Args>(args)...); }
    template <typename... Args>
    static void error(std::string_view fmt, Args&&... args) { log(Level::err,   fmt, std::forward<Args>(args)...); }
    template <typename... Args>
    static void critical(std::string_view fmt, Args&&... args) { log(Level::critical, fmt, std::forward<Args>(args)...); }

private:
    template <typename... Args>
    static void log(Level lvl, std::string_view fmt, Args&&... args) {
        try {
            auto s = fmt::vformat(fmt, fmt::make_format_args(args...));
            log_string(lvl, s);
        } catch (...) {
            // ignore formatting errors
        }
    }
    static void apply_config(const Config& cfg);
    static int to_spd(Level lvl);
    static std::shared_ptr<spdlog::logger>& logger();
    static void log_string(Level lvl, std::string_view message);
};

// UI helpers
struct LogLine { Level level; std::string text; };
std::vector<LogLine> read_log_lines_snapshot(size_t max_lines = 1000);
const char* level_to_label(Level l);
void clear_log_buffer();
void set_log_buffer_capacity(size_t cap);

}
