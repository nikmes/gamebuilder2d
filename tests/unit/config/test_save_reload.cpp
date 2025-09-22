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

// Helper: set GB2D_CONFIG_DIR to a fresh temp dir
static std::filesystem::path prepare_clean_config_dir(const char* sub) {
    namespace fs = std::filesystem;
    auto base = fs::temp_directory_path() / fs::path(sub);
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base, ec);
    set_env("GB2D_CONFIG_DIR", base.string().c_str());
    return base;
}

TEST_CASE("save and reload roundtrip", "[config]") {
    namespace fs = std::filesystem;
    auto base = prepare_clean_config_dir("gb2d_configdir_roundtrip");

    // Start with defaults then override some values
    gb2d::ConfigurationManager::loadOrDefault();
    gb2d::ConfigurationManager::set("window.width", (int64_t)1600);
    gb2d::ConfigurationManager::set("window.height", (int64_t)900);
    gb2d::ConfigurationManager::set("ui.theme", std::string("solarized"));

    REQUIRE(gb2d::ConfigurationManager::save());

    // Simulate fresh run by reloading from disk
    // (In current design, load() replaces the in-memory JSON.)
    REQUIRE(gb2d::ConfigurationManager::load());

    REQUIRE(gb2d::ConfigurationManager::getInt("window.width", 0) == 1600);
    REQUIRE(gb2d::ConfigurationManager::getInt("window.height", 0) == 900);
    REQUIRE(gb2d::ConfigurationManager::getString("ui.theme", "") == std::string("solarized"));
}
