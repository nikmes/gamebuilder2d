#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <functional>
#include <string>
#include <span>
#include <unordered_map>
#include <vector>
#include <memory>

#include "raylib.h"

namespace gb2d::textures {

struct AcquireResult {
    std::string key;
    const Texture2D* texture{nullptr};
    bool placeholder{false};
    bool newlyLoaded{false};
};

struct TextureMetrics {
    std::size_t totalTextures{0};
    std::size_t placeholderTextures{0};
    std::size_t totalBytes{0};
    std::size_t totalAtlases{0};
    std::size_t placeholderAtlases{0};
    std::size_t totalAtlasFrames{0};
};

struct TextureDiagnosticsRecord {
    std::string key;
    std::string originalIdentifier;
    std::string resolvedPath;
    std::size_t refCount{0};
    bool placeholder{false};
    bool ownsTexture{false};
    std::size_t byteSize{0};
    bool atlasAvailable{false};
    bool atlasPlaceholder{false};
    std::size_t atlasFrameCount{0};
    std::optional<std::string> atlasJsonPath{};
    std::vector<std::string> aliases{};
};

struct TextureDiagnosticsSnapshot {
    TextureMetrics metrics{};
    std::size_t totalAliases{0};
    std::vector<TextureDiagnosticsRecord> records{};
};

struct ReloadResult {
    std::size_t attempted{0};
    std::size_t succeeded{0};
    std::size_t placeholders{0};
};

struct AtlasFrame {
    Rectangle frame{};
    Rectangle source{};
    Vector2 pivot{};
    bool rotated{false};
    bool trimmed{false};
    std::string originalName{};
};

struct TextureAtlasHandle {
    std::string key;
    const Texture2D* texture{nullptr};
    bool placeholder{false};
    bool newlyLoaded{false};
    std::span<const AtlasFrame> frames{};
};

class TextureManager {
public:
    static bool init();
    static void shutdown();
    static bool isInitialized();

    static AcquireResult acquire(const std::string& identifier,
                                 std::optional<std::string> alias = std::nullopt);
    static const Texture2D* tryGet(const std::string& key);
    static bool release(const std::string& key);
    static bool forceUnload(const std::string& key);
    static ReloadResult reloadAll();
    static TextureMetrics metrics();
    static TextureDiagnosticsSnapshot diagnosticsSnapshot();

    static TextureAtlasHandle acquireAtlas(const std::string& jsonIdentifier,
                                           std::optional<std::string> alias = std::nullopt);
    static TextureAtlasHandle acquireAtlasFromTexture(const std::string& textureKey,
                                                      const std::string& jsonIdentifier);
    static const TextureAtlasHandle* tryGetAtlas(const std::string& key);
    static bool releaseAtlas(const std::string& key);
    static std::optional<AtlasFrame> getAtlasFrame(const std::string& atlasKey,
                                                   const std::string& frameName);

    struct LoadedTexture {
        Texture2D texture{};
        std::size_t bytes{0};
        bool ownsTexture{true};
    };
    using LoaderFn = std::function<std::optional<LoadedTexture>(const std::filesystem::path& path,
                                                                bool generateMipmaps,
                                                                int filterMode)>;
    using PlaceholderFn = std::function<std::optional<LoadedTexture>()>;

    static void setLoaderForTesting(LoaderFn loader);
    static void setPlaceholderGeneratorForTesting(PlaceholderFn generator);
    static void resetForTesting();
};

} // namespace gb2d::textures
