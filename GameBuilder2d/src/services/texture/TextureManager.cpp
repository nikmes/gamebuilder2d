#include "TextureManager.h"

#include "services/configuration/ConfigurationManager.h"
#include "services/logger/LogManager.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gb2d::textures {
namespace {

using gb2d::logging::LogManager;

struct Settings {
    std::vector<std::filesystem::path> searchPaths{};
    bool generateMipmaps{false};
    int filterMode{TEXTURE_FILTER_BILINEAR};
    std::size_t maxBytes{0};
    std::optional<std::filesystem::path> placeholderPath{};
};

struct TextureRecord {
    std::unique_ptr<Texture2D> texture{};
    std::size_t refCount{0};
    std::string originalIdentifier{};
    std::string resolvedPath{};
    bool placeholder{false};
    bool ownsTexture{false};
    std::size_t byteSize{0};
};

struct ManagerState {
    std::mutex mutex;
    bool initialized{false};
    Settings settings{};
    Texture2D placeholder{};
    bool placeholderReady{false};
    bool placeholderOwns{false};
    std::unordered_map<std::string, TextureRecord> records{};
    std::size_t totalBytes{0};
    bool overBudgetNotified{false};
    TextureManager::LoaderFn testLoader{};
    TextureManager::PlaceholderFn testPlaceholder{};
};

ManagerState& state() {
    static ManagerState s;
    return s;
}

std::string canonicalizeKey(const std::string& raw) {
    std::string key = raw;
    std::replace(key.begin(), key.end(), '\\', '/');
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return key;
}

std::string canonicalizePath(const std::filesystem::path& p) {
    auto normalized = p.lexically_normal();
    return canonicalizeKey(normalized.generic_string());
}

std::size_t estimateTextureBytes(const Texture2D& texture) {
    int base = GetPixelDataSize(texture.width, texture.height, texture.format);
    if (base < 0) base = 0;
    std::size_t bytes = static_cast<std::size_t>(base);
    if (texture.mipmaps > 1) {
        bytes *= static_cast<std::size_t>(texture.mipmaps);
    }
    return bytes;
}

std::optional<std::filesystem::path> checkCandidate(const std::filesystem::path& candidate) {
    std::error_code ec;
    if (!std::filesystem::exists(candidate, ec)) {
        return std::nullopt;
    }
    auto canonical = std::filesystem::weakly_canonical(candidate, ec);
    if (ec) {
        return std::nullopt;
    }
    return canonical;
}

std::optional<std::filesystem::path> resolvePath(const std::string& identifier, const Settings& settings) {
    if (identifier.empty()) {
        return std::nullopt;
    }

    std::filesystem::path input(identifier);

    if (input.is_absolute()) {
        if (auto absolute = checkCandidate(input)) {
            return absolute;
        }
        return std::nullopt;
    }

    std::error_code ec;
    auto currentDir = std::filesystem::current_path(ec);
    if (!ec) {
        auto direct = currentDir / input;
        if (auto found = checkCandidate(direct)) {
            return found;
        }
    }

    for (const auto& search : settings.searchPaths) {
        std::filesystem::path root = search;
        if (!root.is_absolute()) {
            std::error_code ec2;
            auto base = std::filesystem::current_path(ec2);
            if (!ec2) {
                root = base / root;
            }
        }
        auto candidate = root / input;
        if (auto found = checkCandidate(candidate)) {
            return found;
        }
    }

    return std::nullopt;
}

int parseFilter(const std::string& value) {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    if (lower == "nearest" || lower == "point" || lower == "pixel") return TEXTURE_FILTER_POINT;
    if (lower == "bilinear" || lower == "linear") return TEXTURE_FILTER_BILINEAR;
    if (lower == "trilinear") return TEXTURE_FILTER_TRILINEAR;
    if (lower == "anisotropic" || lower == "aniso") return TEXTURE_FILTER_ANISOTROPIC_4X;
    return TEXTURE_FILTER_BILINEAR;
}

Settings loadSettings() {
    Settings s;
    auto paths = ConfigurationManager::getStringList("textures::search_paths", { "assets/textures" });
    s.searchPaths.reserve(paths.size());
    for (const auto& p : paths) {
        s.searchPaths.emplace_back(p);
    }
    s.generateMipmaps = ConfigurationManager::getBool("textures::generate_mipmaps", false);
    s.filterMode = parseFilter(ConfigurationManager::getString("textures::default_filter", "bilinear"));
    auto maxBytes = ConfigurationManager::getInt("textures::max_bytes", 0);
    if (maxBytes > 0) {
        s.maxBytes = static_cast<std::size_t>(maxBytes);
    }
    std::string placeholderPath = ConfigurationManager::getString("textures::placeholder_path", "");
    if (!placeholderPath.empty()) {
        s.placeholderPath = std::filesystem::path(placeholderPath);
    }
    return s;
}

std::optional<TextureManager::LoadedTexture> loadTextureFromDisk(ManagerState& st, const std::filesystem::path& path) {
    if (st.testLoader) {
        return st.testLoader(path, st.settings.generateMipmaps, st.settings.filterMode);
    }
    Texture2D handle = LoadTexture(path.string().c_str());
    if (handle.id == 0) {
        return std::nullopt;
    }
    if (st.settings.generateMipmaps) {
        GenTextureMipmaps(&handle);
    }
    SetTextureFilter(handle, st.settings.filterMode);
    TextureManager::LoadedTexture loaded;
    loaded.texture = handle;
    loaded.ownsTexture = true;
    loaded.bytes = estimateTextureBytes(handle);
    return loaded;
}

std::optional<TextureManager::LoadedTexture> generatePlaceholderTexture(ManagerState& st) {
    if (st.testPlaceholder) {
        return st.testPlaceholder();
    }
    if (st.settings.placeholderPath) {
        if (auto fromConfig = loadTextureFromDisk(st, *st.settings.placeholderPath)) {
            return fromConfig;
        }
        LogManager::warn("Failed to load placeholder texture from '{}'; falling back to generated checkerboard",
                         st.settings.placeholderPath->string());
    }

    Image image = GenImageChecked(64, 64, 8, 8, Color{255, 0, 255, 255}, Color{0, 0, 0, 255});
    Texture2D handle = LoadTextureFromImage(image);
    UnloadImage(image);
    if (handle.id == 0) {
        return std::nullopt;
    }
    if (st.settings.generateMipmaps) {
        GenTextureMipmaps(&handle);
    }
    TextureManager::LoadedTexture loaded;
    loaded.texture = handle;
    loaded.ownsTexture = true;
    loaded.bytes = estimateTextureBytes(handle);
    return loaded;
}

const Texture2D* texturePtr(const TextureRecord& rec, const ManagerState& st) {
    if (rec.placeholder) {
        return st.placeholderReady ? &st.placeholder : nullptr;
    }
    return rec.texture ? rec.texture.get() : nullptr;
}

void subtractBytes(ManagerState& st, std::size_t bytes) {
    if (bytes > st.totalBytes) {
        st.totalBytes = 0;
    } else {
        st.totalBytes -= bytes;
    }
    if (st.settings.maxBytes == 0 || st.totalBytes <= st.settings.maxBytes) {
        st.overBudgetNotified = false;
    }
}

void applyLoadedTexture(TextureRecord& rec, ManagerState& st, const TextureManager::LoadedTexture& loaded, const std::filesystem::path& path) {
    if (rec.texture && rec.ownsTexture) {
        UnloadTexture(*rec.texture);
    }
    subtractBytes(st, rec.byteSize);
    rec.texture = std::make_unique<Texture2D>(loaded.texture);
    rec.ownsTexture = loaded.ownsTexture;
    rec.byteSize = loaded.bytes;
    rec.placeholder = false;
    rec.resolvedPath = path.string();
    st.totalBytes += rec.byteSize;
    if (st.settings.maxBytes > 0 && st.totalBytes > st.settings.maxBytes && !st.overBudgetNotified) {
        LogManager::warn("Texture budget exceeded: {} bytes > configured cap {} bytes", st.totalBytes, st.settings.maxBytes);
        st.overBudgetNotified = true;
    }
}

} // namespace

bool TextureManager::init() {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (st.initialized) {
        return true;
    }

    st.settings = loadSettings();
    if (!st.placeholderReady) {
        if (auto placeholder = generatePlaceholderTexture(st)) {
            st.placeholder = placeholder->texture;
            st.placeholderOwns = placeholder->ownsTexture;
            st.placeholderReady = true;
        } else {
            LogManager::error("TextureManager failed to generate placeholder texture");
            st.placeholder = Texture2D{};
            st.placeholderOwns = false;
            st.placeholderReady = false;
        }
    }

    st.initialized = true;
    LogManager::info("TextureManager initialized (search paths={}, mipmaps={}, filter={})",
                     st.settings.searchPaths.size(),
                     st.settings.generateMipmaps ? "on" : "off",
                     st.settings.filterMode);
    return st.placeholderReady;
}

void TextureManager::shutdown() {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return;
    }

    for (auto& entry : st.records) {
        auto& rec = entry.second;
        if (rec.texture && rec.ownsTexture) {
            UnloadTexture(*rec.texture);
        }
    }
    st.records.clear();
    st.totalBytes = 0;
    st.overBudgetNotified = false;

    if (st.placeholderReady && st.placeholderOwns && st.placeholder.id != 0) {
        UnloadTexture(st.placeholder);
    }
    st.placeholder = Texture2D{};
    st.placeholderReady = false;
    st.placeholderOwns = false;
    st.initialized = false;
}

bool TextureManager::isInitialized() {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    return st.initialized;
}

AcquireResult TextureManager::acquire(const std::string& identifier, std::optional<std::string> alias) {
    if (!isInitialized()) {
        init();
    }

    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return {};
    }

    auto resolved = resolvePath(identifier, st.settings);
    std::string canonicalKey;
    if (alias && !alias->empty()) {
        canonicalKey = canonicalizeKey(*alias);
    } else if (resolved) {
        canonicalKey = canonicalizePath(*resolved);
    } else {
        canonicalKey = canonicalizeKey(identifier);
    }

    auto found = st.records.find(canonicalKey);
    if (found != st.records.end()) {
        auto& rec = found->second;
        rec.refCount++;
        return AcquireResult{canonicalKey, texturePtr(rec, st), rec.placeholder, false};
    }

    TextureRecord rec;
    rec.refCount = 1;
    rec.originalIdentifier = identifier;
    rec.resolvedPath = resolved ? resolved->string() : std::string{};

    bool loadedSuccessfully = false;
    if (resolved) {
        if (auto loaded = loadTextureFromDisk(st, *resolved)) {
            applyLoadedTexture(rec, st, *loaded, *resolved);
            loadedSuccessfully = true;
            LogManager::info("Loaded texture '{}' as '{}'", resolved->string(), canonicalKey);
        } else {
            LogManager::error("Failed to load texture '{}' (key '{}'), using placeholder", resolved->string(), canonicalKey);
            rec.placeholder = true;
        }
    } else {
        LogManager::warn("Texture '{}' not found in configured search paths; using placeholder", identifier);
        rec.placeholder = true;
    }

    auto [it, inserted] = st.records.emplace(canonicalKey, std::move(rec));
    (void)inserted;
    const Texture2D* ptr = texturePtr(it->second, st);
    return AcquireResult{canonicalKey, ptr, it->second.placeholder, true};
}

const Texture2D* TextureManager::tryGet(const std::string& key) {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return nullptr;
    }
    auto canonical = canonicalizeKey(key);
    auto it = st.records.find(canonical);
    if (it == st.records.end()) {
        return nullptr;
    }
    return texturePtr(it->second, st);
}

bool TextureManager::release(const std::string& key) {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return false;
    }
    auto canonical = canonicalizeKey(key);
    auto it = st.records.find(canonical);
    if (it == st.records.end()) {
        LogManager::warn("TextureManager::release called for unknown key '{}'", key);
        return false;
    }
    auto& rec = it->second;
    if (rec.refCount == 0) {
        LogManager::warn("TextureManager::release over-release detected for key '{}'", key);
        return false;
    }
    rec.refCount--;
    if (rec.refCount == 0 && !rec.placeholder) {
        if (rec.texture && rec.ownsTexture) 
        {
            LogManager::info("Unloaded texture '{}' (key '{}')", rec.resolvedPath, canonical);
            UnloadTexture(*rec.texture);
        }
        subtractBytes(st, rec.byteSize);
        st.records.erase(it);
    }
    return true;
}

bool TextureManager::forceUnload(const std::string& key) {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return false;
    }
    auto canonical = canonicalizeKey(key);
    auto it = st.records.find(canonical);
    if (it == st.records.end()) {
        return false;
    }
    auto& rec = it->second;
    if (rec.texture && rec.ownsTexture) {
        UnloadTexture(*rec.texture);
    }
    subtractBytes(st, rec.byteSize);
    st.records.erase(it);
    LogManager::info("Force-unloaded texture '{}'; future acquire will reload", canonical);
    return true;
}

ReloadResult TextureManager::reloadAll() {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    ReloadResult result;
    if (!st.initialized) {
        return result;
    }

    for (auto& [key, rec] : st.records) {
        result.attempted++;
        std::optional<std::filesystem::path> path;
        if (!rec.resolvedPath.empty()) {
            path = std::filesystem::path(rec.resolvedPath);
        } else {
            path = resolvePath(rec.originalIdentifier, st.settings);
        }

        if (!path) {
            result.placeholders++;
            rec.placeholder = true;
            rec.texture.reset();
            rec.ownsTexture = false;
            rec.byteSize = 0;
            LogManager::warn("Reload skipped for '{}' — no resolved path", key);
            continue;
        }

        if (auto loaded = loadTextureFromDisk(st, *path)) {
            applyLoadedTexture(rec, st, *loaded, *path);
            result.succeeded++;
            LogManager::info("Reloaded texture '{}' from '{}'", key, path->string());
        } else {
            result.placeholders++;
            rec.placeholder = true;
            if (rec.texture && rec.ownsTexture) {
                UnloadTexture(*rec.texture);
            }
            subtractBytes(st, rec.byteSize);
            rec.texture.reset();
            rec.ownsTexture = false;
            rec.byteSize = 0;
            rec.resolvedPath = path->string();
            LogManager::error("Reload failed for '{}' ({}); placeholder in use", key, path->string());
        }
    }

    return result;
}

TextureMetrics TextureManager::metrics() {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    TextureMetrics metrics;
    if (!st.initialized) {
        return metrics;
    }
    metrics.totalBytes = st.totalBytes;
    for (const auto& [key, rec] : st.records) {
        (void)key;
        if (rec.placeholder) {
            metrics.placeholderTextures++;
        } else {
            metrics.totalTextures++;
        }
    }
    return metrics;
}

void TextureManager::setLoaderForTesting(LoaderFn loader) {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    st.testLoader = std::move(loader);
}

void TextureManager::setPlaceholderGeneratorForTesting(PlaceholderFn generator) {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    st.testPlaceholder = std::move(generator);
    st.placeholderReady = false;
}

void TextureManager::resetForTesting() {
    shutdown();
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    st.settings = Settings{};
    st.testLoader = nullptr;
    st.testPlaceholder = nullptr;
}

} // namespace gb2d::textures
