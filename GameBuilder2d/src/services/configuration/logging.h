#pragma once
#include <cstddef>
#include <string>
#include <functional>

namespace gb2d::cfglog {
enum class Level { Info, Warning, Debug };

void logf(Level level, const char* fmt, ...);
void info(const char* fmt, ...);
void warning(const char* fmt, ...);
void debug(const char* fmt, ...);

// Hook for the app to replace the sink (e.g., wire to lwlog) if desired.
using sink_fn = std::function<void(Level level, const char* message)>;
void set_sink(sink_fn fn);
}
