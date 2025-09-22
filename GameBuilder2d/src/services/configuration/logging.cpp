#include "logging.h"
#include <cstdarg>
#include <cstdio>

namespace gb2d::cfglog {
static sink_fn g_sink{};

static void default_sink(Level level, const char* msg) {
    const char* lvl = (level == Level::Info) ? "INFO" : (level == Level::Warning) ? "WARN" : "DEBUG";
    std::fprintf(stdout, "[Config][%s] %s\n", lvl, msg);
}

void set_sink(sink_fn fn) { g_sink = std::move(fn); }

static void vlogf(Level level, const char* fmt, va_list args) {
    char buf[1024];
    std::vsnprintf(buf, sizeof(buf), fmt, args);
    if (g_sink) g_sink(level, buf); else default_sink(level, buf);
}

void logf(Level level, const char* fmt, ...) {
    va_list args; va_start(args, fmt); vlogf(level, fmt, args); va_end(args);
}

void info(const char* fmt, ...) {
    va_list args; va_start(args, fmt); vlogf(Level::Info, fmt, args); va_end(args);
}
void warning(const char* fmt, ...) {
    va_list args; va_start(args, fmt); vlogf(Level::Warning, fmt, args); va_end(args);
}
void debug(const char* fmt, ...) {
    va_list args; va_start(args, fmt); vlogf(Level::Debug, fmt, args); va_end(args);
}
}
