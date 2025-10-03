#include "paths.h"
#include <string>
#include <cstdlib>
#include <filesystem>
#include <vector>
#include <unordered_set>

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
		return (p / "../config.json").string();
	}

	// Search current working directory and parents for an existing config.json
	std::vector<std::filesystem::path> candidates;
	std::unordered_set<std::string> seen;
	auto addCandidate = [&](const std::filesystem::path& base) {
		if (base.empty()) return;
		std::filesystem::path candidate = base / "config.json";
		std::string key;
		std::error_code canonEc;
		std::filesystem::path canonical = std::filesystem::weakly_canonical(candidate, canonEc);
		if (!canonEc) {
			key = canonical.lexically_normal().string();
		} else {
			key = candidate.lexically_normal().string();
		}
		if (seen.insert(key).second) {
			candidates.push_back(candidate);
		}
	};

	std::error_code ec;
	std::filesystem::path cwd = std::filesystem::current_path(ec);
	if (ec) {
		cwd = std::filesystem::path(".");
	}

	std::filesystem::path cur = cwd;
	for (int depth = 0; depth < 6; ++depth) {
		addCandidate(cur);
		if (!cur.has_parent_path()) break;
		cur = cur.parent_path();
	}

	for (const auto& candidate : candidates) {
		std::error_code existsEc;
		if (std::filesystem::exists(candidate, existsEc) && !existsEc) {
			std::error_code absEc;
			std::filesystem::path absPath = std::filesystem::absolute(candidate, absEc);
			return (!absEc ? absPath : candidate).string();
		}
	}

	// Default: current working directory
	return (cwd / "config.json").string();
}
}
