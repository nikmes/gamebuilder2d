#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "services/texture/TextureManager.h"
#include "services/configuration/ConfigurationManager.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

using gb2d::ConfigurationManager;
using gb2d::textures::TextureManager;
using gb2d::textures::TextureAtlasHandle;
using gb2d::textures::AtlasFrame;

namespace {

TextureManager::LoadedTexture makeStubTexture(int id, int width = 64, int height = 64) {
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

void installPlaceholderStub() {
    TextureManager::setPlaceholderGeneratorForTesting([]() -> std::optional<TextureManager::LoadedTexture> {
        return makeStubTexture(9001, 8, 8);
    });
}

std::filesystem::path repoRoot() {
    auto path = std::filesystem::path(__FILE__);
    return path.parent_path().parent_path().parent_path();
}

std::filesystem::path atlasDirectory() {
    return (repoRoot() / "assets" / "textures" / "atlases").lexically_normal();
}

void assertFrame(const std::optional<AtlasFrame>& frame,
                 float expectedX,
                 float expectedY,
                 float expectedW,
                 float expectedH) {
    REQUIRE(frame.has_value());
    REQUIRE(frame->frame.x == Catch::Approx(expectedX));
    REQUIRE(frame->frame.y == Catch::Approx(expectedY));
    REQUIRE(frame->frame.width == Catch::Approx(expectedW));
    REQUIRE(frame->frame.height == Catch::Approx(expectedH));
}

} // namespace

TEST_CASE("Texture atlas integration parses toolbaricons coordinates", "[integration][texture]") {
    TextureManager::resetForTesting();
    ConfigurationManager::loadOrDefault();

    const auto atlasDir = atlasDirectory();
    REQUIRE(std::filesystem::exists(atlasDir / "toolbaricons.json"));
    REQUIRE(std::filesystem::exists(atlasDir / "toolbaricons.png"));

    ConfigurationManager::set("textures::search_paths", std::vector<std::string>{ atlasDir.string() });

    installPlaceholderStub();

    std::filesystem::path requestedTexture{};
    TextureManager::setLoaderForTesting([&](const std::filesystem::path& path, bool, int) -> std::optional<TextureManager::LoadedTexture> {
        requestedTexture = path;
        return makeStubTexture(4242, 512, 512);
    });

    REQUIRE(TextureManager::init());

    TextureAtlasHandle atlas = TextureManager::acquireAtlas("toolbaricons.json");
    REQUIRE_FALSE(atlas.placeholder);
    REQUIRE(atlas.texture != nullptr);
    REQUIRE(atlas.frames.size() == 109);

    auto expectedTexturePath = std::filesystem::weakly_canonical(atlasDir / "toolbaricons.png");
    REQUIRE(std::filesystem::weakly_canonical(requestedTexture) == expectedTexturePath);

    assertFrame(TextureManager::getAtlasFrame("toolbaricons.json", "about.png"), 2.0f, 2.0f, 36.0f, 36.0f);
    assertFrame(TextureManager::getAtlasFrame("toolbaricons.json", "cam-down.png"), 40.0f, 40.0f, 36.0f, 36.0f);
    assertFrame(TextureManager::getAtlasFrame("toolbaricons.json", "gamepadconfig.png"), 2.0f, 116.0f, 36.0f, 36.0f);

    auto missing = TextureManager::getAtlasFrame("toolbaricons.json", "not-a-real-frame.png");
    REQUIRE_FALSE(missing.has_value());

    const auto* cached = TextureManager::tryGetAtlas("toolbaricons.json");
    REQUIRE(cached != nullptr);
    REQUIRE_FALSE(cached->placeholder);
    REQUIRE(cached->frames.size() == 109);

    REQUIRE(TextureManager::releaseAtlas(atlas.key));
}
