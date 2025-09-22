#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdlib>

#include "services/configuration/ConfigurationManager.h"
#include "services/configuration/validate.h"

namespace fs = std::filesystem;

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

static fs::path make_temp_config_dir(const std::string& name) {
    auto base = fs::temp_directory_path() / ("gb2d_cfg_boundaries_" + name);
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(base, ec);
    return base;
}

TEST_CASE("Large config file is rejected; defaults loaded and .bak created", "[config][bounds]") {
    auto dir = make_temp_config_dir("large");
    set_env("GB2D_CONFIG_DIR", dir.string().c_str());

    const auto cfg = dir / "config.json";

    // Create valid JSON payload > 1 MiB to trigger size guard
    const size_t targetSize = 2u * 1024u * 1024u; // 2 MiB
    const std::string bigValue(targetSize - 64, 'a'); // room for JSON syntax
    {
        std::ofstream os(cfg, std::ios::binary | std::ios::trunc);
        os << "{\"blob\":\"" << bigValue << "\"}";
    }

    bool ok = gb2d::ConfigurationManager::load();
    REQUIRE_FALSE(ok);

    // Defaults applied
    REQUIRE(gb2d::ConfigurationManager::getInt("window::width", -1) == 1280);
    REQUIRE(gb2d::ConfigurationManager::getInt("window::height", -1) == 720);

    // .bak present
    REQUIRE(fs::exists(dir / "config.json.bak"));
}

TEST_CASE("Unsupported JSON types are rejected by validate helpers", "[config][bounds]") {
    using namespace gb2d;

    // Objects as values are unsupported
    nlohmann::json obj = nlohmann::json::object({{"nested", 1}});
    REQUIRE_FALSE(cfgvalidate::isSupportedJson(obj));

    // Mixed arrays are unsupported
    nlohmann::json mixed = nlohmann::json::array({"a", 2});
    REQUIRE_FALSE(cfgvalidate::isSupportedJson(mixed));

    // Array of strings is supported
    nlohmann::json arr = nlohmann::json::array({"a", "b"});
    REQUIRE(cfgvalidate::isSupportedJson(arr));
}
