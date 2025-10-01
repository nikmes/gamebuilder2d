#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <functional>
#include <string>

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
};

struct ReloadResult {
    std::size_t attempted{0};
    std::size_t succeeded{0};
    std::size_t placeholders{0};
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
