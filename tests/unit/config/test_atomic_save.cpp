#include <catch2/catch_test_macros.hpp>
#include "services/configuration/ConfigurationManager.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>

static void set_env(const char* k, const char* v) {
#if defined(_WIN32)
    _putenv_s(k, v);
#else
    setenv(k, v, 1);
#endif
}

static std::filesystem::path prepare_config_dir(const char* sub) {
    namespace fs = std::filesystem;
    auto base = fs::temp_directory_path() / fs::path(sub);
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base, ec);
    auto envDir = base / "config";
    fs::create_directories(envDir, ec);
    set_env("GB2D_CONFIG_DIR", envDir.string().c_str());
    return base;
}

TEST_CASE("atomic save writes and replaces cleanly", "[config]") {
    namespace fs = std::filesystem;
    auto dir = prepare_config_dir("gb2d_configdir_atomic");

    // Start with defaults and save
    gb2d::ConfigurationManager::loadOrDefault();
    gb2d::ConfigurationManager::set("window.width", (int64_t)1111);
    REQUIRE(gb2d::ConfigurationManager::save());

    auto cfgPath = dir / "config.json";
    REQUIRE(fs::exists(cfgPath));

    // Modify and save again
    gb2d::ConfigurationManager::set("window.width", (int64_t)2222);
    REQUIRE(gb2d::ConfigurationManager::save());

    // Ensure content reflects latest save
    std::ifstream ifs(cfgPath, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    REQUIRE(content.find("2222") != std::string::npos);

    // Ensure no leftover temp files
    size_t tmpCount = 0;
    for (auto& p : fs::directory_iterator(dir)) {
        auto name = p.path().filename().string();
        if (name.find("config.json.tmp") != std::string::npos) tmpCount++;
    }
    REQUIRE(tmpCount == 0);
}
