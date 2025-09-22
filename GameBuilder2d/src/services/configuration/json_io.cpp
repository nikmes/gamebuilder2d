#include "json_io.h"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace gb2d::jsonio {
std::optional<nlohmann::json> readJson(const std::string& path) {
	std::ifstream ifs(path, std::ios::binary);
	if (!ifs) return std::nullopt;
	try {
		nlohmann::json j;
		ifs >> j;
		return j;
	} catch (...) {
		return std::nullopt;
	}
}

bool writeJsonAtomic(const std::string& path, const nlohmann::json& j) {
	namespace fs = std::filesystem;
	fs::path target(path);
	fs::path dir = target.parent_path();
	if (!dir.empty()) {
		std::error_code ec;
		fs::create_directories(dir, ec);
	}
	// Use a unique temp name to avoid collisions
	fs::path tmp = target;
	tmp += ".tmp";
	tmp += std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	{
		std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
		if (!ofs) return false;
		ofs << j.dump(2);
		ofs.flush();
		ofs.close();
	}
	std::error_code ec;
	// Try atomic-ish replace
	fs::rename(tmp, target, ec);
	if (ec) {
		// If rename fails due to existing target (common on Windows), replace content
		ec.clear();
		fs::remove(target, ec);
		ec.clear();
		fs::rename(tmp, target, ec);
	}
	if (ec) {
		// Cleanup tmp if still present
		std::error_code ec2;
		fs::remove(tmp, ec2);
		return false;
	}
	return true;
}
}
