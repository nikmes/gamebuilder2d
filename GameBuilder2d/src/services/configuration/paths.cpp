#include "paths.h"
#include <string>
#include <cstdlib>
#include <filesystem>

namespace gb2d::paths {
namespace {
#ifdef GB2D_INTERNAL_TESTING
	static std::string g_test_path;
#endif
}

#ifdef GB2D_INTERNAL_TESTING
// Global test hook to override config path during tests
void gb2d_set_config_path_for_tests(const std::string& p) { g_test_path = p; }
#endif

std::string configFilePath() {
#ifdef GB2D_INTERNAL_TESTING
	if (!g_test_path.empty()) return g_test_path;
#endif
	// Prefer explicit override for tests and power users
	if (const char* dir = std::getenv("GB2D_CONFIG_DIR"); dir && *dir) {
		std::filesystem::path p(dir);
		std::error_code ec; std::filesystem::create_directories(p, ec);
		return (p / "config.json").string();
	}
	// Default: current working directory
	return std::string("config.json");
}
}
