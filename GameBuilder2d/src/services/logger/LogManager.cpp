#include "LogManager.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <mutex>
#include "ImGuiLogSink.h"

namespace gb2d::logging {
namespace {
    std::shared_ptr<spdlog::logger> g_logger;
    std::mutex g_mtx;
}

std::shared_ptr<spdlog::logger>& LogManager::logger() { return g_logger; }

int LogManager::to_spd(Level lvl) {
    using spd = spdlog::level::level_enum;
    switch (lvl) {
        case Level::trace: return (int)spd::trace;
        case Level::debug: return (int)spd::debug;
        case Level::info: return (int)spd::info;
        case Level::warn: return (int)spd::warn;
        case Level::err: return (int)spd::err;
        case Level::critical: return (int)spd::critical;
        case Level::off: default: return (int)spd::off;
    }
}

void LogManager::apply_config(const Config& cfg) {
    if (!g_logger) return;
    g_logger->set_level((spdlog::level::level_enum)to_spd(cfg.level));
    g_logger->set_pattern(cfg.pattern);
}

Status LogManager::init(const Config& cfg) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (g_logger) return Status::already_initialized;
    try {
        auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
        auto imgui_sink = create_imgui_sink();
        std::vector<spdlog::sink_ptr> sinks{ console_sink, imgui_sink };
        g_logger = std::make_shared<spdlog::logger>(cfg.name, sinks.begin(), sinks.end());
        spdlog::register_logger(g_logger);
        apply_config(cfg);
        return Status::ok;
    } catch (...) {
        return Status::error;
    }
}

bool LogManager::isInitialized() {
    std::lock_guard<std::mutex> lock(g_mtx);
    return (bool)g_logger;
}

Status LogManager::reconfigure(const Config& cfg) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (!g_logger) return Status::not_initialized;
    try { apply_config(cfg); return Status::ok; } catch (...) { return Status::error; }
}

Status LogManager::shutdown() {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (!g_logger) return Status::not_initialized;
    try {
        spdlog::drop(g_logger->name());
        g_logger.reset();
        return Status::ok;
    } catch (...) {
        return Status::error;
    }
}

void LogManager::log_string(Level lvl, std::string_view message) {
    std::shared_ptr<spdlog::logger> local;
    {
        std::lock_guard<std::mutex> lock(g_mtx);
        if (!g_logger) {
            Config cfg; (void)init(cfg);
        }
        local = g_logger;
    }
    if (!local) return;
    switch (lvl) {
        case Level::trace: local->trace("{}", message); break;
        case Level::debug: local->debug("{}", message); break;
        case Level::info:  local->info("{}", message); break;
        case Level::warn:  local->warn("{}", message); break;
        case Level::err:   local->error("{}", message); break;
        case Level::critical: local->critical("{}", message); break;
        case Level::off: default: break;
    }
}

std::vector<LogLine> read_log_lines_snapshot(size_t max_lines) {
    std::vector<LogEntry> raw;
    ImGuiLogBuffer::instance().snapshot(raw);
    std::vector<LogLine> out;
    if (raw.size() > max_lines) raw.erase(raw.begin(), raw.end() - (std::ptrdiff_t)max_lines);
    out.reserve(raw.size());
    for (auto& e : raw) {
        Level lvl = Level::info;
        switch (e.level) {
            case spdlog::level::trace: lvl = Level::trace; break;
            case spdlog::level::debug: lvl = Level::debug; break;
            case spdlog::level::info: lvl = Level::info; break;
            case spdlog::level::warn: lvl = Level::warn; break;
            case spdlog::level::err: lvl = Level::err; break;
            case spdlog::level::critical: lvl = Level::critical; break;
            default: lvl = Level::info; break;
        }
        out.push_back(LogLine{ lvl, e.message });
    }
    return out;
}

const char* level_to_label(Level l) {
    switch (l) {
        case Level::trace: return "TRACE";
        case Level::debug: return "DEBUG";
        case Level::info: return "INFO";
        case Level::warn: return "WARN";
        case Level::err: return "ERROR";
        case Level::critical: return "CRIT";
        case Level::off: default: return "OFF";
    }
}

void clear_log_buffer() { ImGuiLogBuffer::instance().clear(); }
void set_log_buffer_capacity(size_t cap) { ImGuiLogBuffer::instance().setCapacity(cap); }

} // namespace gb2d::logging
