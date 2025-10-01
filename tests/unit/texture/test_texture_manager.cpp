#include <catch2/catch_test_macros.hpp>

#include "services/texture/TextureManager.h"
#include "services/configuration/ConfigurationManager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using gb2d::ConfigurationManager;
using gb2d::textures::AcquireResult;
using gb2d::textures::TextureManager;
using gb2d::textures::TextureMetrics;
using gb2d::textures::ReloadResult;

namespace {

struct ResetGuard {
    ~ResetGuard() { TextureManager::resetForTesting(); }
};

struct TempDir {
    TempDir() {
        auto base = std::filesystem::temp_directory_path();
        auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        path_ = base / ("gb2d_texture_tests_" + std::to_string(stamp));
        std::filesystem::create_directories(path_);
    }

    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_{};
};

TextureManager::LoadedTexture makeStubTexture(int id, int width = 4, int height = 4) {
    TextureManager::LoadedTexture stub;
    stub.texture.id = id;
    stub.texture.width = width;
    stub.texture.height = height;
    stub.texture.mipmaps = 1;
    stub.texture.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    stub.bytes = static_cast<std::size_t>(width * height * 4);
    stub.ownsTexture = false;
    return stub;
}

void writePlaceholderGenerator() {
    TextureManager::setPlaceholderGeneratorForTesting([]() -> std::optional<TextureManager::LoadedTexture> {
        return makeStubTexture(999, 2, 2);
    });
}

} // namespace

TEST_CASE("TextureManager caches textures and reference counts") {
    ConfigurationManager::loadOrDefault();
    ResetGuard guard;
    TextureManager::resetForTesting();

    TempDir dir;
    const auto asset = dir.path() / "ship.png";
    {
        std::ofstream out(asset, std::ios::binary);
        out << "stub";
    }
    ConfigurationManager::set("textures::search_paths", std::vector<std::string>{ asset.parent_path().string() });

    writePlaceholderGenerator();

    int loadCount = 0;
    TextureManager::setLoaderForTesting([&](const std::filesystem::path& path, bool, int) -> std::optional<TextureManager::LoadedTexture> {
        (void)path;
        return makeStubTexture(100 + ++loadCount);
    });

    REQUIRE(TextureManager::init());

    AcquireResult first = TextureManager::acquire("ship.png");
    REQUIRE(first.texture != nullptr);
    REQUIRE_FALSE(first.placeholder);
    REQUIRE(first.newlyLoaded);
    REQUIRE(loadCount == 1);

    AcquireResult second = TextureManager::acquire("ship.png");
    REQUIRE(second.texture == first.texture);
    REQUIRE_FALSE(second.placeholder);
    REQUIRE_FALSE(second.newlyLoaded);
    REQUIRE(loadCount == 1);

    TextureMetrics metrics = TextureManager::metrics();
    REQUIRE(metrics.totalTextures == 1);
    REQUIRE(metrics.placeholderTextures == 0);

    REQUIRE(TextureManager::release(first.key));
    metrics = TextureManager::metrics();
    REQUIRE(metrics.totalTextures == 1);

    REQUIRE(TextureManager::release(second.key));
    metrics = TextureManager::metrics();
    REQUIRE(metrics.totalTextures == 0);

    AcquireResult third = TextureManager::acquire("ship.png");
    REQUIRE(third.texture != nullptr);
    REQUIRE(third.newlyLoaded);
    REQUIRE(loadCount == 2);

    REQUIRE(TextureManager::release(third.key));
}

TEST_CASE("TextureManager returns placeholder on failed load and recovers after force unload") {
    ConfigurationManager::loadOrDefault();
    ResetGuard guard;
    TextureManager::resetForTesting();

    TempDir dir;
    const auto asset = dir.path() / "missing.png";
    {
        std::ofstream out(asset, std::ios::binary);
        out << "stub";
    }
    ConfigurationManager::set("textures::search_paths", std::vector<std::string>{ asset.parent_path().string() });

    writePlaceholderGenerator();

    bool succeed = false;
    int loadCount = 0;
    TextureManager::setLoaderForTesting([&](const std::filesystem::path& path, bool, int) -> std::optional<TextureManager::LoadedTexture> {
        (void)path;
        ++loadCount;
        if (!succeed) {
            return std::nullopt;
        }
        return makeStubTexture(300 + loadCount, 8, 8);
    });

    REQUIRE(TextureManager::init());

    AcquireResult missing = TextureManager::acquire("missing.png");
    REQUIRE(missing.texture != nullptr);
    REQUIRE(missing.placeholder);
    REQUIRE(loadCount == 1);

    REQUIRE(TextureManager::release(missing.key));

    AcquireResult again = TextureManager::acquire("missing.png");
    REQUIRE(again.placeholder);
    REQUIRE(loadCount == 1);
    REQUIRE(TextureManager::release(again.key));

    REQUIRE(TextureManager::forceUnload(missing.key));

    succeed = true;
    AcquireResult recovered = TextureManager::acquire("missing.png");
    REQUIRE_FALSE(recovered.placeholder);
    REQUIRE(loadCount == 2);

    REQUIRE(TextureManager::release(recovered.key));
}

TEST_CASE("TextureManager reloadAll refreshes textures and attempts placeholders") {
    ConfigurationManager::loadOrDefault();
    ResetGuard guard;
    TextureManager::resetForTesting();

    TempDir dir;
    const auto goodAsset = dir.path() / "good.png";
    const auto badAsset = dir.path() / "bad.png";
    {
        std::ofstream out(goodAsset, std::ios::binary);
        out << "good";
    }
    {
        std::ofstream out(badAsset, std::ios::binary);
        out << "bad";
    }
    ConfigurationManager::set("textures::search_paths", std::vector<std::string>{ dir.path().string() });

    writePlaceholderGenerator();

    bool reloadPhase = false;
    int goodLoads = 0;
    TextureManager::setLoaderForTesting([&](const std::filesystem::path& path, bool, int) -> std::optional<TextureManager::LoadedTexture> {
        if (path.filename() == "good.png") {
            return makeStubTexture(500 + ++goodLoads, 16, 16);
        }
        if (path.filename() == "bad.png") {
            if (!reloadPhase) {
                return std::nullopt;
            }
            return makeStubTexture(900, 32, 32);
        }
        return std::nullopt;
    });

    REQUIRE(TextureManager::init());

    AcquireResult good = TextureManager::acquire("good.png");
    REQUIRE_FALSE(good.placeholder);
    const Texture2D* goodBefore = good.texture;

    AcquireResult bad = TextureManager::acquire("bad.png");
    REQUIRE(bad.placeholder);
    const Texture2D* placeholderPtr = bad.texture;

    TextureMetrics metrics = TextureManager::metrics();
    REQUIRE(metrics.totalTextures == 1);
    REQUIRE(metrics.placeholderTextures == 1);

    reloadPhase = true;
    ReloadResult reload = TextureManager::reloadAll();
    REQUIRE(reload.attempted == 2);
    REQUIRE(reload.succeeded == 2);
    REQUIRE(reload.placeholders == 0);

    const Texture2D* goodAfter = TextureManager::tryGet(good.key);
    REQUIRE(goodAfter != nullptr);
    REQUIRE(goodAfter != goodBefore);

    const Texture2D* badAfter = TextureManager::tryGet(bad.key);
    REQUIRE(badAfter != nullptr);
    REQUIRE(badAfter != placeholderPtr);

    metrics = TextureManager::metrics();
    REQUIRE(metrics.totalTextures == 2);
    REQUIRE(metrics.placeholderTextures == 0);

    REQUIRE(TextureManager::release(good.key));
    REQUIRE(TextureManager::release(bad.key));
}
