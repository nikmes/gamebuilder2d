#include <catch2/catch_test_macros.hpp>
#include "services/configuration/ConfigurationManager.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>

static void set_env(const char* k, const char* v) {
#if defined(_WIN32)
    _putenv_s(k, v);
#else
    if (v == nullptr || v[0] == '\0') {
        unsetenv(k);
    } else {
        setenv(k, v, 1);
    }
#endif
}

static std::filesystem::path prepare_env_base(const char* sub) {
    namespace fs = std::filesystem;
    auto base = fs::temp_directory_path() / fs::path(sub);
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base, ec);
    set_env("GB2D_CONFIG_DIR", base.string().c_str());
    return base;
}

TEST_CASE("config end-to-end flow", "[integration][config]") {
    namespace fs = std::filesystem;
    auto base = prepare_env_base("gb2d_configdir_e2e");
    auto cfgPath = base / "config.json";

    // 1) Start with defaults and apply env override
    set_env("GB2D_UI__THEME", "ocean");
    gb2d::ConfigurationManager::loadOrDefault();
    REQUIRE(gb2d::ConfigurationManager::getString("ui.theme", "") == std::string("ocean"));

    // 2) Change a value and save
    gb2d::ConfigurationManager::set("window.width", (int64_t)1440);
    REQUIRE(gb2d::ConfigurationManager::save());
    REQUIRE(fs::exists(cfgPath));

    // 3) Clear env override and reload from disk
    set_env("GB2D_UI__THEME", "");
    REQUIRE(gb2d::ConfigurationManager::load());
    REQUIRE(gb2d::ConfigurationManager::getInt("window.width", 0) == 1440);

    // 4) Corrupt file and ensure fallback + .bak
    {
        std::ofstream ofs(cfgPath, std::ios::binary | std::ios::trunc);
        ofs << "{ not json";
    }
    bool ok = gb2d::ConfigurationManager::load();
    REQUIRE_FALSE(ok);
    REQUIRE(gb2d::ConfigurationManager::getString("ui.theme", "") == std::string("dark"));
    REQUIRE(fs::exists(cfgPath.parent_path() / "config.json.bak"));
}
