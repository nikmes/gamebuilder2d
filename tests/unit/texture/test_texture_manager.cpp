#include <catch2/catch_test_macros.hpp>

#include "services/texture/TextureManager.h"
#include "services/configuration/ConfigurationManager.h"
#include "services/logger/LogManager.h"

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

void writeAtlasFiles(const std::filesystem::path& jsonPath,
                     const std::filesystem::path& pngPath,
                     const std::vector<std::string>& frameNames) {
    std::ofstream jsonOut(jsonPath);
    jsonOut << "{\n  \"frames\": [\n";
    for (std::size_t i = 0; i < frameNames.size(); ++i) {
        const auto& name = frameNames[i];
        const int x = static_cast<int>(i) * 16;
        jsonOut << "    {\n"
                << "      \"filename\": \"" << name << "\",\n"
                << "      \"frame\": {\"x\": " << x << ", \"y\": 0, \"w\": 16, \"h\": 16},\n"
                << "      \"spriteSourceSize\": {\"x\": 0, \"y\": 0, \"w\": 16, \"h\": 16},\n"
                << "      \"sourceSize\": {\"w\": 16, \"h\": 16},\n"
                << "      \"pivot\": {\"x\": 0.5, \"y\": 0.5},\n"
                << "      \"rotated\": false,\n"
                << "      \"trimmed\": false\n"
                << "    }";
        if (i + 1 < frameNames.size()) {
            jsonOut << ",";
        }
        jsonOut << "\n";
    }
    jsonOut << "  ],\n  \"meta\": {\"image\": \"" << pngPath.filename().string() << "\"}\n}\n";

    std::ofstream pngOut(pngPath, std::ios::binary);
    pngOut << "stub";
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

TEST_CASE("TextureManager diagnostics report atlas metrics and placeholders") {
    ConfigurationManager::loadOrDefault();
    ResetGuard guard;
    TextureManager::resetForTesting();

    TempDir dir;
    const auto jsonPath = dir.path() / "toolbaricons.json";
    const auto pngPath = dir.path() / "toolbaricons.png";

    {
        std::ofstream out(jsonPath);
        out << R"({
  "frames": [
    {
      "filename": "zoom-in.png",
      "frame": {"x": 0, "y": 0, "w": 16, "h": 16},
      "spriteSourceSize": {"x": 0, "y": 0, "w": 16, "h": 16},
      "sourceSize": {"w": 16, "h": 16},
      "pivot": {"x": 0.5, "y": 0.5},
      "rotated": false,
      "trimmed": false
    }
  ],
  "meta": {"image": "toolbaricons.png"}
})";
    }
    {
        std::ofstream out(pngPath, std::ios::binary);
        out << "stub";
    }

    ConfigurationManager::set("textures::search_paths", std::vector<std::string>{ dir.path().string() });

    writePlaceholderGenerator();

    int loadCount = 0;
    TextureManager::setLoaderForTesting([&](const std::filesystem::path& path, bool, int) -> std::optional<TextureManager::LoadedTexture> {
        (void)path;
        return makeStubTexture(700 + ++loadCount, 8, 8);
    });

    REQUIRE(TextureManager::init());

    auto atlas = TextureManager::acquireAtlas("toolbaricons.json");
    REQUIRE_FALSE(atlas.placeholder);
    REQUIRE(atlas.frames.size() == 1);

    auto snapshot = TextureManager::diagnosticsSnapshot();
    REQUIRE(snapshot.metrics.totalAtlases == 1);
    REQUIRE(snapshot.metrics.placeholderAtlases == 0);
    REQUIRE(snapshot.metrics.totalAtlasFrames == 1);
    REQUIRE(snapshot.records.size() == 1);

    const auto& record = snapshot.records.front();
    REQUIRE(record.atlasAvailable);
    REQUIRE(record.atlasFrameCount == 1);
    REQUIRE_FALSE(record.atlasPlaceholder);
    REQUIRE_FALSE(record.placeholder);
    REQUIRE(record.refCount == 1);

    REQUIRE(TextureManager::releaseAtlas(atlas.key));

    auto placeholderAtlas = TextureManager::acquireAtlas("missing-atlas.json");
    REQUIRE(placeholderAtlas.placeholder);

    auto placeholderSnapshot = TextureManager::diagnosticsSnapshot();
    REQUIRE(placeholderSnapshot.metrics.placeholderAtlases >= 1);

    bool foundPlaceholder = false;
    for (const auto& rec : placeholderSnapshot.records) {
        if (rec.key == placeholderAtlas.key) {
            foundPlaceholder = true;
            REQUIRE(rec.atlasPlaceholder);
            REQUIRE(rec.atlasFrameCount == 0);
        }
    }
    REQUIRE(foundPlaceholder);

    REQUIRE(TextureManager::releaseAtlas(placeholderAtlas.key));
    TextureManager::forceUnload(placeholderAtlas.key);
}

TEST_CASE("TextureManager reuses atlas cache on repeated acquire") {
    ConfigurationManager::loadOrDefault();
    ResetGuard guard;
    TextureManager::resetForTesting();

    TempDir dir;
    const auto jsonPath = dir.path() / "toolbaricons.json";
    const auto pngPath = dir.path() / "toolbaricons.png";
    writeAtlasFiles(jsonPath, pngPath, {"zoom-in.png"});

    ConfigurationManager::set("textures::search_paths", std::vector<std::string>{ dir.path().string() });

    writePlaceholderGenerator();

    int loadCount = 0;
    TextureManager::setLoaderForTesting([&](const std::filesystem::path&, bool, int) -> std::optional<TextureManager::LoadedTexture> {
        return makeStubTexture(800 + ++loadCount, 8, 8);
    });

    REQUIRE(TextureManager::init());

    auto first = TextureManager::acquireAtlas("toolbaricons.json");
    REQUIRE_FALSE(first.placeholder);
    REQUIRE(first.newlyLoaded);
    REQUIRE(first.frames.size() == 1);
    REQUIRE(loadCount == 1);

    auto second = TextureManager::acquireAtlas("toolbaricons.json");
    REQUIRE_FALSE(second.placeholder);
    REQUIRE_FALSE(second.newlyLoaded);
    REQUIRE(second.frames.size() == 1);
    REQUIRE(second.texture == first.texture);
    REQUIRE(loadCount == 1);

    const auto* cached = TextureManager::tryGetAtlas(first.key);
    REQUIRE(cached != nullptr);
    REQUIRE(cached->frames.size() == 1);
    REQUIRE_FALSE(cached->placeholder);

    REQUIRE(TextureManager::releaseAtlas(first.key));
    REQUIRE(TextureManager::releaseAtlas(second.key));
}

TEST_CASE("TextureManager uses placeholder when atlas texture load fails") {
    ConfigurationManager::loadOrDefault();
    ResetGuard guard;
    TextureManager::resetForTesting();

    TempDir dir;
    const auto jsonPath = dir.path() / "toolbaricons.json";
    const auto pngPath = dir.path() / "toolbaricons.png";
    writeAtlasFiles(jsonPath, pngPath, {"zoom-in.png"});

    ConfigurationManager::set("textures::search_paths", std::vector<std::string>{ dir.path().string() });

    writePlaceholderGenerator();

    TextureManager::setLoaderForTesting([&](const std::filesystem::path&, bool, int) -> std::optional<TextureManager::LoadedTexture> {
        return std::nullopt;
    });

    REQUIRE(TextureManager::init());

    auto atlas = TextureManager::acquireAtlas("toolbaricons.json");
    REQUIRE(atlas.placeholder);
    REQUIRE(atlas.newlyLoaded);
    REQUIRE(atlas.frames.size() == 0);

    auto frame = TextureManager::getAtlasFrame(atlas.key, "zoom-in.png");
    REQUIRE_FALSE(frame.has_value());

    const auto* cached = TextureManager::tryGetAtlas(atlas.key);
    REQUIRE(cached != nullptr);
    REQUIRE(cached->placeholder);
    REQUIRE(cached->frames.empty());

    REQUIRE(TextureManager::releaseAtlas(atlas.key));
}

TEST_CASE("TextureManager reloadAll refreshes atlas metadata and texture") {
    ConfigurationManager::loadOrDefault();
    ResetGuard guard;
    TextureManager::resetForTesting();

    TempDir dir;
    const auto jsonPath = dir.path() / "toolbaricons.json";
    const auto pngPath = dir.path() / "toolbaricons.png";
    writeAtlasFiles(jsonPath, pngPath, {"zoom-in.png"});

    ConfigurationManager::set("textures::search_paths", std::vector<std::string>{ dir.path().string() });

    writePlaceholderGenerator();

    bool reloadPhase = false;
    TextureManager::setLoaderForTesting([&](const std::filesystem::path&, bool, int) -> std::optional<TextureManager::LoadedTexture> {
        if (!reloadPhase) {
            return makeStubTexture(100, 8, 8);
        }
        return makeStubTexture(200, 8, 8);
    });

    REQUIRE(TextureManager::init());

    auto atlas = TextureManager::acquireAtlas("toolbaricons.json");
    REQUIRE_FALSE(atlas.placeholder);
    REQUIRE(atlas.frames.size() == 1);
    const Texture2D* textureBefore = atlas.texture;

    auto originalFrame = TextureManager::getAtlasFrame(atlas.key, "zoom-in.png");
    REQUIRE(originalFrame.has_value());

    writeAtlasFiles(jsonPath, pngPath, {"zoom-in.png", "zoom-out.png"});
    reloadPhase = true;

    auto result = TextureManager::reloadAll();
    REQUIRE(result.attempted == 1);
    REQUIRE(result.succeeded == 1);
    REQUIRE(result.placeholders == 0);

    const auto* reloaded = TextureManager::tryGetAtlas(atlas.key);
    REQUIRE(reloaded != nullptr);
    REQUIRE(reloaded->frames.size() == 2);
    REQUIRE_FALSE(reloaded->placeholder);

    auto newFrame = TextureManager::getAtlasFrame(atlas.key, "zoom-out.png");
    REQUIRE(newFrame.has_value());

    const Texture2D* textureAfter = TextureManager::tryGet(atlas.key);
    REQUIRE(textureAfter != nullptr);
    REQUIRE(textureAfter != textureBefore);

    REQUIRE(TextureManager::releaseAtlas(atlas.key));
}

TEST_CASE("TextureManager can dump atlas contents when enabled") {
    using namespace gb2d::logging;

    ConfigurationManager::loadOrDefault();
    ResetGuard guard;
    TextureManager::resetForTesting();

    if (!LogManager::isInitialized()) {
        const auto initStatus = LogManager::init();
        REQUIRE((initStatus == Status::ok || initStatus == Status::already_initialized));
    }
    Config debugCfg;
    debugCfg.level = Level::debug;
    REQUIRE(LogManager::reconfigure(debugCfg) == Status::ok);
    clear_log_buffer();
    set_log_buffer_capacity(256);

    TempDir dir;
    const auto jsonPath = dir.path() / "toolbaricons.json";
    const auto pngPath = dir.path() / "toolbaricons.png";

    {
        std::ofstream out(jsonPath);
        out << R"({
  "frames": [
    {
      "filename": "zoom-in.png",
      "frame": {"x": 0, "y": 0, "w": 16, "h": 16},
      "spriteSourceSize": {"x": 0, "y": 0, "w": 16, "h": 16},
      "sourceSize": {"w": 16, "h": 16},
      "pivot": {"x": 0.5, "y": 0.5},
      "rotated": false,
      "trimmed": false
    }
  ],
  "meta": {"image": "toolbaricons.png"}
})";
    }
    {
        std::ofstream out(pngPath, std::ios::binary);
        out << "stub";
    }

    ConfigurationManager::set("textures::search_paths", std::vector<std::string>{ dir.path().string() });
    ConfigurationManager::set("textures::log_atlas_contents", true);

    writePlaceholderGenerator();

    TextureManager::setLoaderForTesting([&](const std::filesystem::path&, bool, int) -> std::optional<TextureManager::LoadedTexture> {
        return makeStubTexture(123, 8, 8);
    });

    REQUIRE(TextureManager::init());

    auto atlas = TextureManager::acquireAtlas("toolbaricons.json");
    REQUIRE_FALSE(atlas.placeholder);
    REQUIRE(atlas.frames.size() == 1);

    bool sawHeader = false;
    bool sawFrame = false;
    for (const auto& line : read_log_lines_snapshot()) {
        if (line.level != Level::debug) {
            continue;
        }
        if (line.text.find("Texture atlas dump") != std::string::npos) {
            sawHeader = true;
        }
        if (line.text.find("zoom-in.png") != std::string::npos) {
            sawFrame = true;
        }
    }

    REQUIRE(sawHeader);
    REQUIRE(sawFrame);

    REQUIRE(TextureManager::releaseAtlas(atlas.key));

    LogManager::reconfigure(Config{});
    clear_log_buffer();
}
