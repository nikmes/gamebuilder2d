#include <catch2/catch_test_macros.hpp>
#include "services/configuration/ConfigurationManager.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cstdlib>

using nlohmann::json;

static void set_env(const char* k, const char* v) {
#if defined(_WIN32)
    _putenv_s(k, v);
#else
    setenv(k, v, 1);
#endif
}

static std::string write_temp_config(const std::filesystem::path& baseDir, const json& j) {
    namespace fs = std::filesystem;
    fs::create_directories(baseDir);
    auto path = baseDir / "config.json";
    std::ofstream ofs(path, std::ios::binary);
    ofs << j.dump(2);
    ofs.close();
    return path.string();
}

TEST_CASE("load existing valid file", "[config]") {
    json j;
    j["window"]["width"] = 1024;
    j["window"]["height"] = 600;
    j["ui"]["theme"] = "light";

    namespace fs = std::filesystem;
    auto base = fs::temp_directory_path() / fs::path("gb2d_configdir_test");
    std::error_code ec;
    fs::create_directories(base, ec);
    auto envDir = base / "config";
    fs::create_directories(envDir, ec);
    // Override config dir to temp folder
    set_env("GB2D_CONFIG_DIR", envDir.string().c_str());

    auto path = write_temp_config(base, j);

    // Call loadOrDefault then load from disk.
    gb2d::ConfigurationManager::loadOrDefault();

    REQUIRE(gb2d::ConfigurationManager::load());

    REQUIRE(gb2d::ConfigurationManager::getInt("window.width", 0) == 1024);
    REQUIRE(gb2d::ConfigurationManager::getInt("window.height", 0) == 600);
    REQUIRE(gb2d::ConfigurationManager::getString("ui.theme", "") == std::string("light"));
}
