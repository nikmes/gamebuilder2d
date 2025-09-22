#include <catch2/catch_test_macros.hpp>
#include "services/configuration/ConfigurationManager.h"
#include <atomic>
#include <filesystem>
#include <cstdlib>

static void set_env(const char* k, const char* v) {
#if defined(_WIN32)
    _putenv_s(k, v);
#else
    setenv(k, v, 1);
#endif
}

static void set_config_dir_temp(const char* sub) {
    auto base = std::filesystem::temp_directory_path() / sub;
    std::filesystem::create_directories(base);
    set_env("GB2D_CONFIG_DIR", base.string().c_str());
}

TEST_CASE("change notifications fire after save", "[config]") {
    set_config_dir_temp("gb2d_configdir_change");

    std::atomic<int> count{0};
    int id = gb2d::ConfigurationManager::subscribeOnChange([&]{ count++; });

    gb2d::ConfigurationManager::loadOrDefault();

    // load() should not trigger
    gb2d::ConfigurationManager::load();
    REQUIRE(count.load() == 0);

    // save() should trigger
    gb2d::ConfigurationManager::set("ui.theme", std::string("monokai"));
    REQUIRE(gb2d::ConfigurationManager::save());
    REQUIRE(count.load() == 1);

    // unsubscribe should stop further callbacks
    gb2d::ConfigurationManager::unsubscribe(id);
    gb2d::ConfigurationManager::set("ui.theme", std::string("dracula"));
    REQUIRE(gb2d::ConfigurationManager::save());
    REQUIRE(count.load() == 1);
}
