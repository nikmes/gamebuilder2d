#include "TextureManager.h"

#include "services/configuration/ConfigurationManager.h"
#include "services/logger/LogManager.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <span>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace gb2d::textures {
namespace {

using gb2d::logging::LogManager;

struct Settings {
    std::vector<std::filesystem::path> searchPaths{};
    bool generateMipmaps{false};
    int filterMode{TEXTURE_FILTER_BILINEAR};
    std::size_t maxBytes{0};
    std::optional<std::filesystem::path> placeholderPath{};
    bool logAtlasContents{false};
};

struct TextureRecord {
    std::unique_ptr<Texture2D> texture{};
    std::size_t refCount{0};
    std::string originalIdentifier{};
    std::string resolvedPath{};
    bool placeholder{false};
    bool ownsTexture{false};
    std::size_t byteSize{0};
    std::optional<std::filesystem::path> atlasJsonPath{};
    std::unique_ptr<std::vector<AtlasFrame>> atlasFrames{};
    std::unordered_map<std::string, std::size_t> atlasLookup{};
    bool atlasPlaceholder{false};
    mutable std::optional<TextureAtlasHandle> cachedAtlasHandle{};
};

struct AtlasDefinition {
    std::filesystem::path imagePath{};
    std::vector<AtlasFrame> frames{};
    std::unordered_map<std::string, std::size_t> lookup{};
};

struct ManagerState {
    std::mutex mutex;
    bool initialized{false};
    Settings settings{};
    Texture2D placeholder{};
    bool placeholderReady{false};
    bool placeholderOwns{false};
    std::unordered_map<std::string, TextureRecord> records{};
    std::unordered_map<std::string, std::string> aliasToKey{};
    std::size_t totalBytes{0};
    bool overBudgetNotified{false};
    TextureManager::LoaderFn testLoader{};
    TextureManager::PlaceholderFn testPlaceholder{};
};

ManagerState& state() {
    static ManagerState s;
    return s;
}

TextureMetrics computeMetricsSnapshot(const ManagerState& st) {
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
        if (rec.atlasPlaceholder) {
            metrics.placeholderAtlases++;
        } else if (rec.atlasFrames && !rec.atlasFrames->empty()) {
            metrics.totalAtlases++;
            metrics.totalAtlasFrames += rec.atlasFrames->size();
        }
    }
    return metrics;
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

std::string canonicalizeFrameName(const std::string& raw) {
    std::string key = raw;
    std::replace(key.begin(), key.end(), '\\', '/');
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return key;
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
    s.logAtlasContents = ConfigurationManager::getBool("textures::log_atlas_contents", false);
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
    rec.cachedAtlasHandle.reset();
}

float jsonNumberToFloat(const nlohmann::json& node, const char* key, float fallback = 0.0f) {
    const auto it = node.find(key);
    if (it == node.end()) {
        return fallback;
    }
    if (it->is_number_float()) {
        return static_cast<float>(it->get<double>());
    }
    if (it->is_number_integer()) {
        return static_cast<float>(it->get<int>());
    }
    return fallback;
}

std::optional<int> jsonInt(const nlohmann::json& node, const char* key) {
    const auto it = node.find(key);
    if (it == node.end()) {
        return std::nullopt;
    }
    if (it->is_number_integer()) {
        return it->get<int>();
    }
    if (it->is_number_float()) {
        return static_cast<int>(it->get<double>());
    }
    return std::nullopt;
}

AtlasDefinition parseAtlasFrames(const nlohmann::json& framesNode, const std::filesystem::path& jsonPath) {
    AtlasDefinition definition;
    definition.frames.reserve(framesNode.size());

    for (const auto& frameNode : framesNode) {
        if (!frameNode.is_object()) {
            continue;
        }
        const auto filenameIt = frameNode.find("filename");
        if (filenameIt == frameNode.end() || !filenameIt->is_string()) {
            LogManager::warn("Texture atlas '{}': frame entry missing filename", jsonPath.string());
            continue;
        }
        const std::string originalName = filenameIt->get<std::string>();
        if (originalName.empty()) {
            LogManager::warn("Texture atlas '{}': frame entry has empty filename", jsonPath.string());
            continue;
        }

        const auto canonicalName = canonicalizeFrameName(originalName);
        if (definition.lookup.contains(canonicalName)) {
            LogManager::warn("Texture atlas '{}': duplicate frame '{}' ignored", jsonPath.string(), originalName);
            continue;
        }

        const auto frameRectIt = frameNode.find("frame");
        if (frameRectIt == frameNode.end() || !frameRectIt->is_object()) {
            LogManager::warn("Texture atlas '{}': frame '{}' missing 'frame' rectangle", jsonPath.string(), originalName);
            continue;
        }
        Rectangle rect{};
        const auto xOpt = jsonInt(*frameRectIt, "x");
        const auto yOpt = jsonInt(*frameRectIt, "y");
        const auto wOpt = jsonInt(*frameRectIt, "w");
        const auto hOpt = jsonInt(*frameRectIt, "h");
        if (!xOpt || !yOpt || !wOpt || !hOpt) {
            LogManager::warn("Texture atlas '{}': frame '{}' has invalid rectangle values", jsonPath.string(), originalName);
            continue;
        }
        rect.x = static_cast<float>(*xOpt);
        rect.y = static_cast<float>(*yOpt);
        rect.width = static_cast<float>(*wOpt);
        rect.height = static_cast<float>(*hOpt);

        Rectangle sourceRect{0.0f, 0.0f, rect.width, rect.height};
        const auto spriteSourceIt = frameNode.find("spriteSourceSize");
        if (spriteSourceIt != frameNode.end() && spriteSourceIt->is_object()) {
            const auto sx = jsonInt(*spriteSourceIt, "x");
            const auto sy = jsonInt(*spriteSourceIt, "y");
            const auto sw = jsonInt(*spriteSourceIt, "w");
            const auto sh = jsonInt(*spriteSourceIt, "h");
            if (sx && sy && sw && sh) {
                sourceRect.x = static_cast<float>(*sx);
                sourceRect.y = static_cast<float>(*sy);
                sourceRect.width = static_cast<float>(*sw);
                sourceRect.height = static_cast<float>(*sh);
            }
        }

        const auto sourceSizeIt = frameNode.find("sourceSize");
        if (sourceSizeIt != frameNode.end() && sourceSizeIt->is_object()) {
            const auto sw = jsonInt(*sourceSizeIt, "w");
            const auto sh = jsonInt(*sourceSizeIt, "h");
            if (sw && sh) {
                sourceRect.width = static_cast<float>(*sw);
                sourceRect.height = static_cast<float>(*sh);
            }
        }

        Vector2 pivot{0.0f, 0.0f};
        const auto pivotIt = frameNode.find("pivot");
        if (pivotIt != frameNode.end() && pivotIt->is_object()) {
            pivot.x = jsonNumberToFloat(*pivotIt, "x", 0.0f);
            pivot.y = jsonNumberToFloat(*pivotIt, "y", 0.0f);
        }

        const bool rotated = frameNode.value("rotated", false);
        const bool trimmed = frameNode.value("trimmed", false);
        if (rotated || trimmed) {
            LogManager::warn("Texture atlas '{}': frame '{}' is {}{} – rotation/trim not yet supported",
                             jsonPath.string(),
                             originalName,
                             rotated ? "rotated" : "",
                             (rotated && trimmed) ? " & trimmed" : (trimmed ? "trimmed" : ""));
        }

        AtlasFrame frame{};
        frame.frame = rect;
        frame.source = sourceRect;
        frame.pivot = pivot;
        frame.rotated = rotated;
        frame.trimmed = trimmed;
        frame.originalName = originalName;

        definition.lookup.emplace(canonicalName, definition.frames.size());
        definition.frames.push_back(frame);
    }

    return definition;
}

std::optional<AtlasDefinition> loadAtlasDefinition(const std::filesystem::path& jsonPath) {
    std::ifstream file(jsonPath);
    if (!file) {
        LogManager::error("Texture atlas JSON '{}' could not be opened", jsonPath.string());
        return std::nullopt;
    }

    nlohmann::json document;
    try {
        file >> document;
    } catch (const std::exception& ex) {
        LogManager::error("Texture atlas JSON '{}' failed to parse: {}", jsonPath.string(), ex.what());
        return std::nullopt;
    }

    if (!document.is_object()) {
        LogManager::error("Texture atlas JSON '{}' root must be an object", jsonPath.string());
        return std::nullopt;
    }

    const auto framesIt = document.find("frames");
    if (framesIt == document.end() || !framesIt->is_array()) {
        LogManager::error("Texture atlas JSON '{}' missing 'frames' array", jsonPath.string());
        return std::nullopt;
    }

    AtlasDefinition definition = parseAtlasFrames(*framesIt, jsonPath);
    if (definition.frames.empty()) {
        LogManager::error("Texture atlas JSON '{}' did not yield any frames", jsonPath.string());
        return std::nullopt;
    }

    const auto metaIt = document.find("meta");
    if (metaIt != document.end() && metaIt->is_object()) {
        const auto imageIt = metaIt->find("image");
        if (imageIt != metaIt->end() && imageIt->is_string()) {
            std::filesystem::path imageRelative = imageIt->get<std::string>();
            definition.imagePath = (jsonPath.parent_path() / imageRelative).lexically_normal();
        }
    }

    if (definition.imagePath.empty()) {
        auto fallback = jsonPath;
        fallback.replace_extension(".png");
        definition.imagePath = fallback.lexically_normal();
        LogManager::warn("Texture atlas JSON '{}' missing meta.image; assuming '{}'", jsonPath.string(), definition.imagePath.string());
    }

    return definition;
}

void setAtlasPlaceholder(TextureRecord& rec) {
    rec.atlasPlaceholder = true;
    if (!rec.atlasFrames) {
        rec.atlasFrames = std::make_unique<std::vector<AtlasFrame>>();
    } else {
        rec.atlasFrames->clear();
    }
    rec.atlasLookup.clear();
    rec.cachedAtlasHandle.reset();
}

void assignAtlasFrames(TextureRecord& rec, AtlasDefinition&& definition) {
    rec.atlasPlaceholder = false;
    rec.atlasFrames = std::make_unique<std::vector<AtlasFrame>>(std::move(definition.frames));
    rec.atlasLookup = std::move(definition.lookup);
    rec.cachedAtlasHandle.reset();
}

void maybeDumpAtlasContents(const ManagerState& st,
                           const std::string& atlasKey,
                           const std::string& identifier,
                           const std::optional<std::filesystem::path>& jsonPath,
                           const AtlasDefinition& definition) {
    if (!st.settings.logAtlasContents) {
        return;
    }

    std::string sourcePath;
    if (jsonPath) {
        sourcePath = jsonPath->string();
    } else if (!identifier.empty()) {
        sourcePath = identifier;
    }

    LogManager::debug("Texture atlas dump: key='{}', identifier='{}', json='{}', frames={}",
                      atlasKey,
                      identifier,
                      sourcePath.empty() ? "<unknown>" : sourcePath,
                      definition.frames.size());

    for (const auto& frame : definition.frames) {
        LogManager::debug("    frame '{}' rect=[{}, {}, {}, {}] source=[{}, {}, {}, {}] pivot=[{}, {}] rotated={} trimmed={}",
                          frame.originalName,
                          frame.frame.x,
                          frame.frame.y,
                          frame.frame.width,
                          frame.frame.height,
                          frame.source.x,
                          frame.source.y,
                          frame.source.width,
                          frame.source.height,
                          frame.pivot.x,
                          frame.pivot.y,
                          frame.rotated ? "true" : "false",
                          frame.trimmed ? "true" : "false");
    }
}

TextureAtlasHandle makeAtlasHandle(const std::string& key,
                                  const TextureRecord& rec,
                                  const ManagerState& st,
                                  bool newlyLoaded) {
    TextureAtlasHandle handle;
    handle.key = key;
    handle.texture = texturePtr(rec, st);
    handle.placeholder = rec.placeholder || rec.atlasPlaceholder;
    handle.newlyLoaded = newlyLoaded;
    if (rec.atlasFrames) {
        handle.frames = std::span<const AtlasFrame>(*rec.atlasFrames);
    }
    return handle;
}

const TextureAtlasHandle* ensureAtlasHandle(const std::string& key,
                                            TextureRecord& rec,
                                            const ManagerState& st) {
    if (!rec.cachedAtlasHandle) {
        rec.cachedAtlasHandle.emplace();
    }
    TextureAtlasHandle& handle = *rec.cachedAtlasHandle;
    handle.key = key;
    handle.texture = texturePtr(rec, st);
    handle.placeholder = rec.placeholder || rec.atlasPlaceholder;
    handle.newlyLoaded = false;
    if (rec.atlasFrames) {
        handle.frames = std::span<const AtlasFrame>(*rec.atlasFrames);
    } else {
        handle.frames = {};
    }
    return &handle;
}

std::string resolveRecordKey(const ManagerState& st, const std::string& suppliedKey) {
    auto canonical = canonicalizeKey(suppliedKey);
    auto aliasIt = st.aliasToKey.find(canonical);
    if (aliasIt != st.aliasToKey.end()) {
        return aliasIt->second;
    }
    return canonical;
}

void bindAlias(ManagerState& st, const std::string& alias, const std::string& key) {
    if (alias.empty()) {
        return;
    }
    st.aliasToKey[alias] = key;
}

void unbindAlias(ManagerState& st, const std::string& alias) {
    if (alias.empty()) {
        return;
    }
    st.aliasToKey.erase(alias);
}

void unbindAliasesForKey(ManagerState& st, const std::string& key) {
    for (auto it = st.aliasToKey.begin(); it != st.aliasToKey.end();) {
        if (it->second == key) {
            it = st.aliasToKey.erase(it);
        } else {
            ++it;
        }
    }
}

TextureRecord* rekeyRecord(ManagerState& st, const std::string& oldKey, const std::string& newKey) {
    if (oldKey == newKey) {
        auto it = st.records.find(newKey);
        return it != st.records.end() ? &it->second : nullptr;
    }

    auto oldIt = st.records.find(oldKey);
    if (oldIt == st.records.end()) {
        auto it = st.records.find(newKey);
        return it != st.records.end() ? &it->second : nullptr;
    }

    auto newIt = st.records.find(newKey);
    if (newIt != st.records.end()) {
        auto& dst = newIt->second;
        auto& src = oldIt->second;
        dst.refCount += src.refCount;
        if (!dst.texture && src.texture) {
            dst.texture = std::move(src.texture);
            dst.ownsTexture = src.ownsTexture;
            dst.byteSize = src.byteSize;
            dst.placeholder = src.placeholder;
            dst.resolvedPath = src.resolvedPath;
        }
        if (!src.resolvedPath.empty() && dst.resolvedPath.empty()) {
            dst.resolvedPath = src.resolvedPath;
        }
        if (!src.originalIdentifier.empty() && dst.originalIdentifier.empty()) {
            dst.originalIdentifier = src.originalIdentifier;
        }
        if (src.atlasFrames) {
            if (!dst.atlasFrames || dst.atlasFrames->empty()) {
                dst.atlasFrames = std::move(src.atlasFrames);
                dst.atlasLookup = std::move(src.atlasLookup);
                dst.atlasPlaceholder = src.atlasPlaceholder;
            }
        }
        if (src.cachedAtlasHandle) {
            dst.cachedAtlasHandle = std::move(src.cachedAtlasHandle);
        }
        if (src.atlasJsonPath) {
            dst.atlasJsonPath = src.atlasJsonPath;
        }
        st.records.erase(oldIt);
        for (auto& [aliasKey, mapped] : st.aliasToKey) {
            if (mapped == oldKey) {
                mapped = newKey;
            }
        }
        return &dst;
    }

    auto node = st.records.extract(oldKey);
    if (node.empty()) {
        return nullptr;
    }
    node.key() = newKey;
    auto insertResult = st.records.insert(std::move(node));
    for (auto& [aliasKey, mapped] : st.aliasToKey) {
        if (mapped == oldKey) {
            mapped = newKey;
        }
    }
    return &insertResult.position->second;
}

bool reloadAtlasMetadata(const std::string& key, TextureRecord& rec, ManagerState& st) {
    if (!rec.atlasJsonPath) {
        return false;
    }

    auto jsonPath = *rec.atlasJsonPath;
    if (auto canonical = checkCandidate(jsonPath)) {
        jsonPath = *canonical;
        rec.atlasJsonPath = jsonPath;
    }

    std::error_code existsEc;
    if (!std::filesystem::exists(jsonPath, existsEc) || existsEc) {
        LogManager::warn("Texture atlas JSON '{}' missing during reload for '{}'", jsonPath.string(), key);
        setAtlasPlaceholder(rec);
        return false;
    }

    auto definition = loadAtlasDefinition(jsonPath);
    if (!definition) {
        LogManager::error("Texture atlas '{}' failed to reload for '{}'", jsonPath.string(), key);
        setAtlasPlaceholder(rec);
        return false;
    }

    std::size_t frameCount = definition->frames.size();
    maybeDumpAtlasContents(st, key, rec.originalIdentifier, rec.atlasJsonPath, *definition);
    assignAtlasFrames(rec, std::move(*definition));
    bindAlias(st, canonicalizePath(jsonPath), key);
    LogManager::info("Texture atlas '{}' reloaded (frames={})", jsonPath.string(), frameCount);
    return true;
}

void purgeAtlasMetadata(TextureRecord& rec) {
    if (rec.atlasFrames) {
        rec.atlasFrames->clear();
        rec.atlasFrames.reset();
    }
    rec.atlasLookup.clear();
    rec.atlasPlaceholder = false;
    rec.atlasJsonPath.reset();
    rec.cachedAtlasHandle.reset();
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
    LogManager::info("TextureManager initialized (search paths={}, mipmaps={}, filter={}, atlasDumpLogging={})",
                     st.settings.searchPaths.size(),
                     st.settings.generateMipmaps ? "on" : "off",
                     st.settings.filterMode,
                     st.settings.logAtlasContents ? "on" : "off");
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
        purgeAtlasMetadata(rec);
    }
    st.records.clear();
    st.aliasToKey.clear();
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
    std::string aliasKey;
    if (alias && !alias->empty()) {
        aliasKey = canonicalizeKey(*alias);
    }

    std::string canonicalKey;
    bool canonicalFromAlias = false;
    bool canonicalFromIdentifierAlias = false;

    if (!aliasKey.empty()) {
        auto aliasIt = st.aliasToKey.find(aliasKey);
        if (aliasIt != st.aliasToKey.end()) {
            canonicalKey = aliasIt->second;
            canonicalFromAlias = true;
        }
    }

    if (canonicalKey.empty()) {
        std::string identifierKey = resolveRecordKey(st, identifier);
        if (identifierKey != canonicalizeKey(identifier)) {
            canonicalKey = identifierKey;
            canonicalFromIdentifierAlias = true;
        }
    }

    if (canonicalKey.empty()) {
        if (resolved) {
            canonicalKey = canonicalizePath(*resolved);
        } else {
            canonicalKey = canonicalizeKey(identifier);
        }
    }

    auto found = st.records.find(canonicalKey);
    if (found == st.records.end() && canonicalFromAlias) {
        unbindAlias(st, aliasKey);
        canonicalKey.clear();
        if (resolved) {
            canonicalKey = canonicalizePath(*resolved);
        } else {
            canonicalKey = canonicalizeKey(identifier);
        }
        found = st.records.find(canonicalKey);
    }
        auto identifierKeyCanon = canonicalizeKey(identifier);
        if (identifierKeyCanon != canonicalKey) {
            bindAlias(st, identifierKeyCanon, canonicalKey);
        }
    if (found == st.records.end() && canonicalFromIdentifierAlias) {
        unbindAlias(st, canonicalizeKey(identifier));
        canonicalKey.clear();
        if (resolved) {
            canonicalKey = canonicalizePath(*resolved);
        } else {
            canonicalKey = canonicalizeKey(identifier);
        }
        found = st.records.find(canonicalKey);
    }

    if (!aliasKey.empty()) {
        bindAlias(st, aliasKey, canonicalKey);
    }
    if (found != st.records.end()) {
        auto& rec = found->second;
        rec.refCount++;
        return AcquireResult{canonicalKey, texturePtr(rec, st), rec.placeholder, false};
    }

    TextureRecord rec;
    rec.refCount = 1;
    rec.originalIdentifier = identifier;
    rec.resolvedPath = resolved ? resolved->string() : std::string{};

    if (resolved) {
        if (auto loaded = loadTextureFromDisk(st, *resolved)) {
            applyLoadedTexture(rec, st, *loaded, *resolved);
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

TextureAtlasHandle TextureManager::acquireAtlas(const std::string& jsonIdentifier,
                                               std::optional<std::string> alias) {
    if (!isInitialized()) {
        init();
    }

    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return {};
    }

    auto resolvedJson = resolvePath(jsonIdentifier, st.settings);
    std::optional<AtlasDefinition> definition;
    if (resolvedJson) {
        definition = loadAtlasDefinition(*resolvedJson);
        if (!definition) {
            LogManager::error("Texture atlas '{}' failed to load; placeholder will be used", resolvedJson->string());
        }
    } else {
        LogManager::error("Texture atlas JSON '{}' not found; placeholder will be used", jsonIdentifier);
    }

    std::filesystem::path jsonPath = resolvedJson ? *resolvedJson : std::filesystem::path{};
    std::filesystem::path texturePath;
    std::string textureKey;
    if (definition) {
        auto candidate = checkCandidate(definition->imagePath);
        texturePath = candidate ? *candidate : definition->imagePath;
        textureKey = canonicalizePath(texturePath);
    }

    std::string aliasKey = (alias && !alias->empty()) ? canonicalizeKey(*alias) : std::string{};
    std::string jsonKey = resolvedJson ? canonicalizePath(*resolvedJson) : canonicalizeKey(jsonIdentifier);

    std::vector<std::string> candidateKeys;
    if (!textureKey.empty()) {
        candidateKeys.push_back(textureKey);
    }
    if (!aliasKey.empty()) {
        candidateKeys.push_back(resolveRecordKey(st, aliasKey));
    }
    candidateKeys.push_back(resolveRecordKey(st, jsonKey));

    TextureRecord* record = nullptr;
    std::string existingKey;
    for (const auto& candidate : candidateKeys) {
        auto it = st.records.find(candidate);
        if (it != st.records.end()) {
            record = &it->second;
            existingKey = candidate;
            break;
        }
    }

    std::string canonicalKey;
    if (!textureKey.empty()) {
        canonicalKey = textureKey;
    } else if (!existingKey.empty()) {
        canonicalKey = existingKey;
    } else if (!candidateKeys.empty()) {
        canonicalKey = candidateKeys.front();
    } else {
        canonicalKey = jsonKey;
    }

    if (record && !textureKey.empty() && existingKey != textureKey) {
        record = rekeyRecord(st, existingKey, textureKey);
        canonicalKey = textureKey;
    }

    bool recordWasNew = false;
    if (!record) {
        TextureRecord rec;
        rec.refCount = 0;
        if (!texturePath.empty()) {
            rec.originalIdentifier = texturePath.string();
            rec.resolvedPath = texturePath.string();
        } else {
            rec.originalIdentifier = jsonIdentifier;
        }
        if (!definition) {
            rec.placeholder = true;
            setAtlasPlaceholder(rec);
        }
        rec.atlasJsonPath = resolvedJson;
        auto [itRecord, inserted] = st.records.emplace(canonicalKey, std::move(rec));
        recordWasNew = inserted;
        record = &itRecord->second;
    }

    if (!aliasKey.empty()) {
        bindAlias(st, aliasKey, canonicalKey);
    }
    if (jsonKey != canonicalKey) {
        bindAlias(st, jsonKey, canonicalKey);
    }
    std::string identifierKey = canonicalizeKey(jsonIdentifier);
    if (identifierKey != canonicalKey) {
        bindAlias(st, identifierKey, canonicalKey);
    }

    bool textureLoadedNow = false;
    bool metadataNewlyLoaded = false;

    if (definition) {
        record->atlasJsonPath = resolvedJson;
        metadataNewlyLoaded = record->atlasPlaceholder || !record->atlasFrames || record->atlasFrames->empty();
    AtlasDefinition loadedDef = std::move(*definition);
    maybeDumpAtlasContents(st, canonicalKey, jsonIdentifier, resolvedJson, loadedDef);
    std::size_t frameCount = loadedDef.frames.size();
        assignAtlasFrames(*record, std::move(loadedDef));
        LogManager::info("Texture atlas '{}' loaded (frames={})", jsonIdentifier, frameCount);
        if (!texturePath.empty()) {
            std::string existingPathKey = record->resolvedPath.empty()
                                              ? std::string{}
                                              : canonicalizePath(std::filesystem::path(record->resolvedPath));
            bool needsLoad = record->placeholder || !record->texture || existingPathKey != canonicalKey;
            if (needsLoad) {
                if (auto loaded = loadTextureFromDisk(st, texturePath)) {
                    applyLoadedTexture(*record, st, *loaded, texturePath);
                    textureLoadedNow = true;
                    LogManager::info("Texture atlas '{}' bound to texture '{}'", jsonIdentifier, texturePath.string());
                } else {
                    LogManager::error("Failed to load atlas texture '{}' referenced by '{}'", texturePath.string(), jsonIdentifier);
                    if (record->texture && record->ownsTexture) {
                        UnloadTexture(*record->texture);
                    }
                    subtractBytes(st, record->byteSize);
                    record->texture.reset();
                    record->ownsTexture = false;
                    record->byteSize = 0;
                    record->placeholder = true;
                    record->resolvedPath = texturePath.string();
                    setAtlasPlaceholder(*record);
                    LogManager::warn("Texture atlas '{}' falling back to placeholder texture", jsonIdentifier);
                }
            }
        }
    } else {
        setAtlasPlaceholder(*record);
        record->atlasJsonPath = resolvedJson;
        LogManager::warn("Texture atlas '{}' using placeholder metadata", jsonIdentifier);
    }

    record->refCount++;
    bool newlyLoaded = recordWasNew || textureLoadedNow || metadataNewlyLoaded;
    return makeAtlasHandle(canonicalKey, *record, st, newlyLoaded);
}

TextureAtlasHandle TextureManager::acquireAtlasFromTexture(const std::string& textureKey,
                                                           const std::string& jsonIdentifier) {
    if (!isInitialized()) {
        init();
    }

    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return {};
    }

    std::string canonicalKey = resolveRecordKey(st, textureKey);
    auto it = st.records.find(canonicalKey);
    if (it == st.records.end()) {
        LogManager::warn("TextureManager::acquireAtlasFromTexture missing base texture '{}'", textureKey);
        TextureAtlasHandle handle;
        handle.key = canonicalKey;
        handle.texture = st.placeholderReady ? &st.placeholder : nullptr;
        handle.placeholder = true;
        handle.newlyLoaded = false;
        return handle;
    }

    TextureRecord& record = it->second;

    auto resolvedJson = resolvePath(jsonIdentifier, st.settings);
    if (!resolvedJson) {
        LogManager::error("Texture atlas JSON '{}' not found; placeholder will be used", jsonIdentifier);
        setAtlasPlaceholder(record);
        record.atlasJsonPath.reset();
        record.refCount++;
        return makeAtlasHandle(canonicalKey, record, st, false);
    }

    auto definition = loadAtlasDefinition(*resolvedJson);
    if (!definition) {
        LogManager::error("Texture atlas '{}' failed to load; placeholder will be used", resolvedJson->string());
        setAtlasPlaceholder(record);
        record.atlasJsonPath = resolvedJson;
        record.refCount++;
        bindAlias(st, canonicalizePath(*resolvedJson), canonicalKey);
        return makeAtlasHandle(canonicalKey, record, st, false);
    }

    bool metadataNewlyLoaded = record.atlasPlaceholder || !record.atlasFrames || record.atlasFrames->empty();
    AtlasDefinition loadedDef = std::move(*definition);
    maybeDumpAtlasContents(st, canonicalKey, jsonIdentifier, resolvedJson, loadedDef);
    std::size_t frameCount = loadedDef.frames.size();
    assignAtlasFrames(record, std::move(loadedDef));
    LogManager::info("Texture atlas '{}' loaded (frames={})", jsonIdentifier, frameCount);
    record.atlasJsonPath = resolvedJson;
    record.refCount++;
    std::string jsonKey = canonicalizePath(*resolvedJson);
    if (jsonKey != canonicalKey) {
        bindAlias(st, jsonKey, canonicalKey);
    }
    std::string identifierKey = canonicalizeKey(jsonIdentifier);
    if (identifierKey != canonicalKey) {
        bindAlias(st, identifierKey, canonicalKey);
    }
    return makeAtlasHandle(canonicalKey, record, st, metadataNewlyLoaded);
}

const TextureAtlasHandle* TextureManager::tryGetAtlas(const std::string& key) {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return nullptr;
    }
    auto canonical = resolveRecordKey(st, key);
    auto it = st.records.find(canonical);
    if (it == st.records.end()) {
        return nullptr;
    }
    return ensureAtlasHandle(canonical, it->second, st);
}

bool TextureManager::releaseAtlas(const std::string& key) {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return false;
    }
    auto canonical = resolveRecordKey(st, key);
    auto it = st.records.find(canonical);
    if (it == st.records.end()) {
        LogManager::warn("TextureManager::releaseAtlas called for unknown key '{}'", key);
        return false;
    }
    TextureRecord& rec = it->second;
    if (rec.refCount == 0) {
        LogManager::warn("TextureManager::releaseAtlas over-release detected for key '{}'", key);
        return false;
    }
    rec.refCount--;
    if (rec.refCount == 0 && !rec.placeholder) {
        if (rec.texture && rec.ownsTexture) {
            UnloadTexture(*rec.texture);
        }
        subtractBytes(st, rec.byteSize);
        st.records.erase(it);
        unbindAliasesForKey(st, canonical);
    }
    return true;
}

std::optional<AtlasFrame> TextureManager::getAtlasFrame(const std::string& atlasKey,
                                                        const std::string& frameName) {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return std::nullopt;
    }

    auto canonical = resolveRecordKey(st, atlasKey);
    auto it = st.records.find(canonical);
    if (it == st.records.end()) {
        return std::nullopt;
    }
    const TextureRecord& rec = it->second;
    if (!rec.atlasFrames || rec.atlasFrames->empty()) {
        return std::nullopt;
    }
    auto nameKey = canonicalizeFrameName(frameName);
    auto frameIt = rec.atlasLookup.find(nameKey);
    if (frameIt == rec.atlasLookup.end()) {
        return std::nullopt;
    }
    std::size_t index = frameIt->second;
    if (index >= rec.atlasFrames->size()) {
        return std::nullopt;
    }
    return (*rec.atlasFrames)[index];
}

const Texture2D* TextureManager::tryGet(const std::string& key) {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return nullptr;
    }
    auto canonical = resolveRecordKey(st, key);
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
    auto canonical = resolveRecordKey(st, key);
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
        unbindAliasesForKey(st, canonical);
    }
    return true;
}

bool TextureManager::forceUnload(const std::string& key) {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return false;
    }
    auto canonical = resolveRecordKey(st, key);
    auto it = st.records.find(canonical);
    if (it == st.records.end()) {
        return false;
    }
    auto& rec = it->second;
    if (rec.texture && rec.ownsTexture) {
        UnloadTexture(*rec.texture);
    }
    purgeAtlasMetadata(rec);
    subtractBytes(st, rec.byteSize);
    st.records.erase(it);
    unbindAliasesForKey(st, canonical);
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

        bool textureLoaded = false;
        if (!path) {
            result.placeholders++;
            rec.placeholder = true;
            rec.texture.reset();
            rec.ownsTexture = false;
            rec.byteSize = 0;
            LogManager::warn("Reload skipped for '{}' — no resolved path", key);
            setAtlasPlaceholder(rec);
            continue;
        }

        if (auto loaded = loadTextureFromDisk(st, *path)) {
            applyLoadedTexture(rec, st, *loaded, *path);
            result.succeeded++;
            LogManager::info("Reloaded texture '{}' from '{}'", key, path->string());
            textureLoaded = true;
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
            setAtlasPlaceholder(rec);
        }

        if (textureLoaded) {
            if (rec.atlasJsonPath) {
                reloadAtlasMetadata(key, rec, st);
            } else if (rec.atlasFrames && rec.atlasFrames->size() > 0) {
                setAtlasPlaceholder(rec);
            }
        }
    }

    return result;
}

TextureMetrics TextureManager::metrics() {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    return computeMetricsSnapshot(st);
}

TextureDiagnosticsSnapshot TextureManager::diagnosticsSnapshot() {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    TextureDiagnosticsSnapshot snapshot;
    snapshot.metrics = computeMetricsSnapshot(st);
    if (!st.initialized) {
        return snapshot;
    }

    snapshot.totalAliases = st.aliasToKey.size();

    std::unordered_map<std::string, std::vector<std::string>> reverseAliases;
    reverseAliases.reserve(st.aliasToKey.size());
    for (const auto& [alias, target] : st.aliasToKey) {
        reverseAliases[target].push_back(alias);
    }

    snapshot.records.reserve(st.records.size());
    for (const auto& [key, rec] : st.records) {
        TextureDiagnosticsRecord record;
        record.key = key;
        record.originalIdentifier = rec.originalIdentifier;
        record.resolvedPath = rec.resolvedPath;
        record.refCount = rec.refCount;
        record.placeholder = rec.placeholder;
        record.ownsTexture = rec.ownsTexture;
        record.byteSize = rec.byteSize;
        record.atlasPlaceholder = rec.atlasPlaceholder;
        record.atlasFrameCount = rec.atlasFrames ? rec.atlasFrames->size() : 0;
        record.atlasAvailable = record.atlasFrameCount > 0;
        if (rec.atlasJsonPath) {
            record.atlasJsonPath = rec.atlasJsonPath->string();
        }
        if (auto aliasIt = reverseAliases.find(key); aliasIt != reverseAliases.end()) {
            record.aliases = aliasIt->second;
            std::sort(record.aliases.begin(), record.aliases.end());
        }
        snapshot.records.push_back(std::move(record));
    }

    std::sort(snapshot.records.begin(), snapshot.records.end(), [](const TextureDiagnosticsRecord& a, const TextureDiagnosticsRecord& b) {
        return a.key < b.key;
    });

    return snapshot;
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
    st.aliasToKey.clear();
}

} // namespace gb2d::textures
