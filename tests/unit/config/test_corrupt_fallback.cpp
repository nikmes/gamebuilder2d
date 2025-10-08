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

static std::filesystem::path prepare_corrupt_config_env(const char* sub) {
    namespace fs = std::filesystem;
    auto base = fs::temp_directory_path() / fs::path(sub);
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base, ec);
    auto envDir = base / "config";
    fs::create_directories(envDir, ec);
    set_env("GB2D_CONFIG_DIR", envDir.string().c_str());
    auto cfg = base / "config.json";
    std::ofstream ofs(cfg, std::ios::binary);
    ofs << "{ this is not valid json ";
    ofs.close();
    return base;
}

TEST_CASE("corrupt file fallback and bak", "[config]") {
    namespace fs = std::filesystem;
    auto base = prepare_corrupt_config_env("gb2d_configdir_corrupt");
    auto cfgDir = base;
    auto cfg = cfgDir / "config.json";
    auto bak = cfgDir / "config.json.bak";

    // Ensure no bak yet
    REQUIRE_FALSE(fs::exists(bak));

    // Attempt to load corrupt file should return false and reset to defaults
    bool ok = gb2d::ConfigurationManager::load();
    REQUIRE_FALSE(ok);

    // Defaults should be present
    REQUIRE(gb2d::ConfigurationManager::getInt("window.width", 0) == 1280);
    REQUIRE(gb2d::ConfigurationManager::getInt("window.height", 0) == 720);
    REQUIRE(gb2d::ConfigurationManager::getString("ui.theme", "") == std::string("dark"));

    // Bak should be created
    REQUIRE(fs::exists(bak));
}
