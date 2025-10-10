#include "AudioManager.h"

#include "services/configuration/ConfigurationManager.h"
#include "services/logger/LogManager.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gb2d::audio {
namespace {

using gb2d::logging::LogManager;

struct Settings {
    bool enabled{true};
    float masterVolume{1.0f};
    float musicVolume{1.0f};
    float sfxVolume{1.0f};
    std::size_t maxConcurrentSounds{16};
    std::vector<std::filesystem::path> searchPaths{};
    std::vector<std::string> preloadSounds{};
    std::vector<std::string> preloadMusic{};
};

struct SoundRecord {
    Sound sound{};
    std::size_t refCount{0};
    bool placeholder{true};
    std::string originalIdentifier{};
    std::string resolvedPath{};
};

struct MusicRecord {
    Music music{};
    std::size_t refCount{0};
    bool placeholder{true};
    std::string originalIdentifier{};
    std::string resolvedPath{};
    bool playing{false};
    bool paused{false};
    float volume{1.0f};
};

struct ManagerState {
    std::mutex mutex;
    bool initialized{false};
    bool deviceReady{false};
    bool silentMode{false};
    Settings settings{};
    AudioConfig publishedConfig{};
    AudioManager::Backend* overrideBackend{nullptr};
    std::unordered_map<std::string, SoundRecord> sounds{};
    std::unordered_map<std::string, MusicRecord> music{};
    std::uint32_t generationCounter{1};
    std::size_t activeSoundInstances{0};
    const AudioManager::RaylibHooks* overrideHooks{nullptr};
    // Event subscription infrastructure
    std::vector<AudioEventSubscription> eventSubscriptions{};
    std::uint32_t nextSubscriptionId{1};
    struct SoundSlot {
        Sound alias{};
        std::string key{};
        bool active{false};
        bool placeholder{true};
        std::uint32_t generation{0};
        float volume{1.0f};
        float pitch{1.0f};
        float pan{0.5f};
    };
    std::vector<SoundSlot> soundSlots{};
};

ManagerState& state() {
    static ManagerState s;
    return s;
}

void publishAudioEvent(AudioEventType type, const std::string& key = "", const std::string& details = "") {
    auto& st = state();
    // Note: This function assumes the caller already holds st.mutex
    
    AudioEvent event{
        type,
        key,
        static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()),
        details
    };
    
    LogManager::info("AudioManager publishing event: type={}, key='{}', subscriptions={}", 
                     static_cast<int>(type), key, st.eventSubscriptions.size());
    
    for (const auto& subscription : st.eventSubscriptions) {
        if (subscription.active && subscription.sink) {
            LogManager::info("AudioManager calling onAudioEvent for subscription ID {}", subscription.id);
            subscription.sink->onAudioEvent(event);
        } else {
            LogManager::warn("AudioManager skipping inactive subscription ID {}", subscription.id);
        }
    }
}

class RaylibBackend final : public AudioManager::Backend {
public:
    void initDevice() override { InitAudioDevice(); }
    void closeDevice() override { CloseAudioDevice(); }
    bool isDeviceReady() override { return IsAudioDeviceReady(); }
    void setMasterVolume(float volume) override { SetMasterVolume(volume); }
};

AudioManager::Backend* defaultBackend() {
    static RaylibBackend backend;
    return &backend;
}

AudioManager::Backend* backend() {
    auto& st = state();
    return st.overrideBackend ? st.overrideBackend : defaultBackend();
}

const AudioManager::RaylibHooks& defaultHooks() {
    static const AudioManager::RaylibHooks hooks{
        &LoadSound,
        &UnloadSound,
        &LoadSoundAlias,
        &UnloadSoundAlias,
        &rlPlaySound,
        &StopSound,
        &IsSoundPlaying,
        &SetSoundVolume,
        &SetSoundPitch,
        &SetSoundPan,
        &LoadMusicStream,
        &UnloadMusicStream,
        &PlayMusicStream,
        &PauseMusicStream,
        &ResumeMusicStream,
        &StopMusicStream,
        &UpdateMusicStream,
        &IsMusicStreamPlaying,
        &SetMusicVolume,
        &SeekMusicStream,
        &GetMusicTimeLength,
        &GetMusicTimePlayed
    };
    return hooks;
}

[[nodiscard]] const AudioManager::RaylibHooks& hooks(const ManagerState& st) {
    return st.overrideHooks ? *st.overrideHooks : defaultHooks();
}

void stopAllSoundsLocked(ManagerState& st, const AudioManager::RaylibHooks& api);

float clamp01(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

float clampPitch(float value) {
    constexpr float kMinPitch = 0.125f;
    constexpr float kMaxPitch = 4.0f;
    if (value < kMinPitch) {
        return kMinPitch;
    }
    if (value > kMaxPitch) {
        return kMaxPitch;
    }
    return value;
}

float clampPan(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

void ensureSoundSlotCapacity(ManagerState& st) {
    if (st.soundSlots.size() == st.settings.maxConcurrentSounds) {
        return;
    }
    const AudioManager::RaylibHooks& api = hooks(st);
    if (!st.soundSlots.empty()) {
        stopAllSoundsLocked(st, api);
    }
    st.soundSlots.clear();
    st.soundSlots.resize(st.settings.maxConcurrentSounds);
    st.activeSoundInstances = 0;
}

std::optional<std::size_t> findFreeSoundSlotIndex(ManagerState& st) {
    for (std::size_t i = 0; i < st.soundSlots.size(); ++i) {
        if (!st.soundSlots[i].active) {
            return i;
        }
    }
    return std::nullopt;
}

std::string canonicalizeKey(const std::string& raw) {
    std::string key = raw;
    std::replace(key.begin(), key.end(), '\\', '/');
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return key;
}

std::string canonicalizePath(const std::filesystem::path& path) {
    auto normalized = path.lexically_normal();
    return canonicalizeKey(normalized.generic_string());
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

    for (const auto& root : settings.searchPaths) {
        std::filesystem::path base = root;
        if (!base.is_absolute()) {
            std::error_code rootEc;
            auto cwd = std::filesystem::current_path(rootEc);
            if (!rootEc) {
                base = cwd / base;
            }
        }
        auto candidate = base / input;
        if (auto found = checkCandidate(candidate)) {
            return found;
        }
    }

    return std::nullopt;
}

bool isSoundValid(const Sound& sound) {
    return sound.frameCount > 0 && sound.stream.buffer != nullptr;
}

bool isMusicValid(const Music& music) {
    return music.stream.buffer != nullptr;
}

void unloadSoundRecord(SoundRecord& record, const AudioManager::RaylibHooks& api) {
    if (!record.placeholder && isSoundValid(record.sound)) {
        api.unloadSound(record.sound);
    }
    record.sound = Sound{};
    record.placeholder = true;
    record.resolvedPath.clear();
}

void unloadMusicRecord(MusicRecord& record, const AudioManager::RaylibHooks& api) {
    if (!record.placeholder && isMusicValid(record.music)) {
        api.stopMusicStream(record.music);
        api.unloadMusicStream(record.music);
    }
    record.music = Music{};
    record.placeholder = true;
    record.resolvedPath.clear();
    record.playing = false;
    record.paused = false;
    record.volume = 1.0f;
}

void releaseSoundSlot(ManagerState::SoundSlot& slot, const AudioManager::RaylibHooks& api) {
    if (slot.active) {
        if (isSoundValid(slot.alias)) {
            api.stopSound(slot.alias);
            api.unloadSoundAlias(slot.alias);
        }
    }
    slot.alias = Sound{};
    slot.key.clear();
    slot.active = false;
    slot.placeholder = true;
    slot.generation = 0;
    slot.volume = 1.0f;
    slot.pitch = 1.0f;
    slot.pan = 0.5f;
}

void refreshSoundSlotsLocked(ManagerState& st, const AudioManager::RaylibHooks& api) {
    std::size_t active = 0;
    for (auto& slot : st.soundSlots) {
        if (!slot.active) {
            continue;
        }
        if (!api.isSoundPlaying(slot.alias)) {
            // Sound finished playing - publish event before releasing
            std::string stoppedKey = slot.key;
            releaseSoundSlot(slot, api);
            
            // Publish event outside the slot cleanup
            if (!stoppedKey.empty()) {
                publishAudioEvent(AudioEventType::SoundPlaybackStopped, stoppedKey);
            }
            continue;
        }
        active++;
    }
    st.activeSoundInstances = active;
}

void stopAllSoundsLocked(ManagerState& st, const AudioManager::RaylibHooks& api) {
    for (auto& slot : st.soundSlots) {
        if (!slot.active) {
            continue;
        }
        releaseSoundSlot(slot, api);
    }
    st.activeSoundInstances = 0;
}

void stopMusicRecord(const AudioManager::RaylibHooks& api, MusicRecord& record) {
    if (!record.placeholder && isMusicValid(record.music)) {
        api.stopMusicStream(record.music);
    }
    record.playing = false;
    record.paused = false;
}

Settings loadSettings() {
    Settings s;
    s.enabled = ConfigurationManager::getBool("audio::enabled", true);
    s.masterVolume = std::clamp(ConfigurationManager::getDouble("audio::master_volume", 1.0), 0.0, 1.0);
    s.musicVolume = std::clamp(ConfigurationManager::getDouble("audio::music_volume", 1.0), 0.0, 1.0);
    s.sfxVolume = std::clamp(ConfigurationManager::getDouble("audio::sfx_volume", 1.0), 0.0, 1.0);
    auto maxSlots = ConfigurationManager::getInt("audio::max_concurrent_sounds", 16);
    if (maxSlots < 0) maxSlots = 0;
    s.maxConcurrentSounds = static_cast<std::size_t>(maxSlots);
    auto paths = ConfigurationManager::getStringList("audio::search_paths", {"assets/audio"});
    s.searchPaths.reserve(paths.size());
    for (const auto& p : paths) {
        s.searchPaths.emplace_back(p);
    }
    s.preloadSounds = ConfigurationManager::getStringList("audio::preload_sounds", {});
    s.preloadMusic = ConfigurationManager::getStringList("audio::preload_music", {});
    return s;
}

AudioConfig toConfig(const Settings& s, bool initialized, bool deviceReady, bool silentMode) {
    AudioConfig cfg;
    cfg.enabled = s.enabled;
    cfg.masterVolume = s.masterVolume;
    cfg.musicVolume = s.musicVolume;
    cfg.sfxVolume = s.sfxVolume;
    cfg.maxConcurrentSounds = s.maxConcurrentSounds;
    cfg.searchPaths.reserve(s.searchPaths.size());
    for (const auto& p : s.searchPaths) {
        cfg.searchPaths.emplace_back(p.generic_string());
    }
    cfg.preloadSounds = s.preloadSounds;
    cfg.preloadMusic = s.preloadMusic;
    (void)initialized;
    (void)deviceReady;
    (void)silentMode;
    return cfg;
}

} // namespace

bool AudioManager::init() {
    auto& st = state();
    std::unique_lock<std::mutex> lock(st.mutex);
    if (st.initialized) {
        return st.deviceReady && !st.silentMode;
    }

    st.settings = loadSettings();
    ensureSoundSlotCapacity(st);
    st.publishedConfig = toConfig(st.settings, true, false, false);

    std::vector<std::string> preloadSounds = st.settings.preloadSounds;
    std::vector<std::string> preloadMusic = st.settings.preloadMusic;

    if (!st.settings.enabled) {
        LogManager::info("AudioManager initialized in silent mode (audio disabled by configuration)");
        st.initialized = true;
        st.deviceReady = false;
        st.silentMode = true;
        return false;
    }

    backend()->initDevice();
    st.deviceReady = backend()->isDeviceReady();
    st.silentMode = !st.deviceReady;

    if (st.deviceReady) {
        backend()->setMasterVolume(st.settings.masterVolume);
        LogManager::info("AudioManager initialized (master={}, music={}, sfx={}, maxSlots={})",
                         st.settings.masterVolume,
                         st.settings.musicVolume,
                         st.settings.sfxVolume,
                         st.settings.maxConcurrentSounds);
    } else {
        LogManager::warn("AudioManager failed to initialize audio device; entering silent mode");
    }

    st.initialized = true;
    st.publishedConfig = toConfig(st.settings, st.initialized, st.deviceReady, st.silentMode);
    bool canPreload = st.deviceReady && !st.silentMode && (!preloadSounds.empty() || !preloadMusic.empty());
    lock.unlock();
    if (canPreload) {
        for (const auto& soundId : preloadSounds) {
            auto result = acquireSound(soundId);
            if (result.placeholder) {
                LogManager::warn("Failed to preload sound '{}'", soundId);
            }
        }
        for (const auto& musicId : preloadMusic) {
            auto result = acquireMusic(musicId);
            if (result.placeholder) {
                LogManager::warn("Failed to preload music '{}'", musicId);
            }
        }
    }
    return st.deviceReady;
}

void AudioManager::shutdown() {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return;
    }

    const auto& api = hooks(st);
    for (auto& [key, rec] : st.sounds) {
        (void)key;
        unloadSoundRecord(rec, api);
    }
    for (auto& [key, rec] : st.music) {
        (void)key;
        unloadMusicRecord(rec, api);
    }
    st.sounds.clear();
    st.music.clear();
    st.activeSoundInstances = 0;

    stopAllSoundsLocked(st, api);
    for (auto& slot : st.soundSlots) {
        slot = ManagerState::SoundSlot{};
    }

    if (st.deviceReady) {
        backend()->closeDevice();
    }

    st.initialized = false;
    st.deviceReady = false;
    st.silentMode = false;
}

bool AudioManager::isInitialized() {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    return st.initialized;
}

bool AudioManager::isDeviceReady() {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    return st.deviceReady;
}

void AudioManager::tick(float /*deltaSeconds*/) {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized || st.silentMode) {
        if (st.initialized && st.activeSoundInstances > 0) {
            const auto& api = hooks(st);
            stopAllSoundsLocked(st, api);
        }
        return;
    }
    if (!st.deviceReady) {
        const auto& api = hooks(st);
        stopAllSoundsLocked(st, api);
        return;
    }

    const auto& api = hooks(st);
    refreshSoundSlotsLocked(st, api);

    for (auto& [key, rec] : st.music) {
        (void)key;
        if (rec.placeholder || !rec.playing || rec.paused) {
            continue;
        }
        api.updateMusicStream(rec.music);
        if (!api.isMusicStreamPlaying(rec.music)) {
            rec.playing = false;
            rec.paused = false;
            
            // Publish music stopped event
            publishAudioEvent(AudioEventType::MusicPlaybackStopped, key);
        }
    }
}

AudioConfig AudioManager::config() {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    return st.publishedConfig;
}

AcquireSoundResult AudioManager::acquireSound(const std::string& identifier,
                                              std::optional<std::string> alias) {
    if (!isInitialized()) {
        init();
    }

    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return {};
    }
    const auto& api = hooks(st);
    std::string key = alias && !alias->empty() ? canonicalizeKey(*alias) : canonicalizeKey(identifier);
    std::optional<std::filesystem::path> resolved = resolvePath(identifier, st.settings);
    if ((!alias || alias->empty()) && resolved) {
        key = canonicalizePath(*resolved);
    }
    auto it = st.sounds.find(key);
    if (it != st.sounds.end()) {
        it->second.refCount++;
        return AcquireSoundResult{key, &it->second.sound, it->second.placeholder, false};
    }

    SoundRecord record;
    record.refCount = 1;
    record.originalIdentifier = identifier;
    record.placeholder = true;
    if (resolved) {
        record.resolvedPath = resolved->string();
    }

    if (!st.silentMode && st.deviceReady && resolved) {
        Sound soundHandle = api.loadSound(resolved->string().c_str());
        if (isSoundValid(soundHandle)) {
            record.sound = soundHandle;
            record.placeholder = false;
            LogManager::info("AudioManager loaded sound '{}' as '{}'", resolved->string(), key);
            publishAudioEvent(AudioEventType::SoundLoaded, key);
        } else {
            LogManager::error("AudioManager failed to load sound '{}' (key '{}'), using placeholder", resolved->string(), key);
        }
    } else if (!resolved) {
        LogManager::warn("AudioManager could not resolve sound identifier '{}'", identifier);
    } else if (st.silentMode || !st.deviceReady) {
        LogManager::info("AudioManager in silent mode; sound '{}' will be placeholder", identifier);
    }

    auto [insertedIt, inserted] = st.sounds.emplace(key, std::move(record));
    (void)inserted;
    return AcquireSoundResult{key, &insertedIt->second.sound, insertedIt->second.placeholder, true};
}

const Sound* AudioManager::tryGetSound(const std::string& key) {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    auto canonical = canonicalizeKey(key);
    auto it = st.sounds.find(canonical);
    if (it == st.sounds.end()) {
        return nullptr;
    }
    return &it->second.sound;
}

bool AudioManager::releaseSound(const std::string& key) {
    auto canonical = canonicalizeKey(key);
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    const auto& api = hooks(st);
    auto it = st.sounds.find(canonical);
    if (it == st.sounds.end()) {
        LogManager::warn("AudioManager::releaseSound unknown key '{}'", key);
        return false;
    }
    auto& rec = it->second;
    if (rec.refCount == 0) {
        LogManager::warn("AudioManager::releaseSound over-release for key '{}'", key);
        return false;
    }
    rec.refCount--;
    if (rec.refCount == 0) {
        unloadSoundRecord(rec, api);
        st.sounds.erase(it);
        publishAudioEvent(AudioEventType::SoundUnloaded, canonical);
    }
    return true;
}

AcquireMusicResult AudioManager::acquireMusic(const std::string& identifier,
                                              std::optional<std::string> alias) {
    if (!isInitialized()) {
        init();
    }

    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return {};
    }
    const auto& api = hooks(st);
    std::string key = alias && !alias->empty() ? canonicalizeKey(*alias) : canonicalizeKey(identifier);
    std::optional<std::filesystem::path> resolved = resolvePath(identifier, st.settings);
    if (!alias || alias->empty()) {
        if (resolved) {
            key = canonicalizePath(*resolved);
        }
    }
    auto it = st.music.find(key);
    if (it != st.music.end()) {
        it->second.refCount++;
        return AcquireMusicResult{key, &it->second.music, it->second.placeholder, false};
    }

    MusicRecord record;
    record.refCount = 1;
    record.originalIdentifier = identifier;
    record.placeholder = true;
    if (resolved) {
        record.resolvedPath = resolved->string();
    }

    if (!st.silentMode && st.deviceReady && resolved) {
        Music musicHandle = api.loadMusicStream(resolved->string().c_str());
        if (isMusicValid(musicHandle)) {
            record.music = musicHandle;
            record.placeholder = false;
            LogManager::info("AudioManager loaded music '{}' as '{}'", resolved->string(), key);
            publishAudioEvent(AudioEventType::MusicLoaded, key);
        } else {
            LogManager::error("AudioManager failed to load music '{}' (key '{}'), using placeholder", resolved->string(), key);
        }
    } else if (!resolved) {
        LogManager::warn("AudioManager could not resolve music identifier '{}'", identifier);
    } else if (st.silentMode || !st.deviceReady) {
        LogManager::info("AudioManager in silent mode; music '{}' will be placeholder", identifier);
    }

    auto [insertedIt, inserted] = st.music.emplace(key, std::move(record));
    (void)inserted;
    return AcquireMusicResult{key, &insertedIt->second.music, insertedIt->second.placeholder, true};
}

const Music* AudioManager::tryGetMusic(const std::string& key) {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    auto canonical = canonicalizeKey(key);
    auto it = st.music.find(canonical);
    if (it == st.music.end()) {
        return nullptr;
    }
    return &it->second.music;
}

bool AudioManager::releaseMusic(const std::string& key) {
    auto canonical = canonicalizeKey(key);
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    const auto& api = hooks(st);
    auto it = st.music.find(canonical);
    if (it == st.music.end()) {
        LogManager::warn("AudioManager::releaseMusic unknown key '{}'", key);
        return false;
    }
    auto& rec = it->second;
    if (rec.refCount == 0) {
        LogManager::warn("AudioManager::releaseMusic over-release for key '{}'", key);
        return false;
    }
    rec.refCount--;
    if (rec.refCount == 0) {
        unloadMusicRecord(rec, api);
        st.music.erase(it);
        publishAudioEvent(AudioEventType::MusicUnloaded, canonical);
    }
    return true;
}

PlaybackHandle AudioManager::playSound(const std::string& key, const PlaybackParams& params) {
    auto canonical = canonicalizeKey(key);
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        LogManager::warn("AudioManager::playSound called before initialization (key='{}')", canonical);
        return {};
    }

    auto it = st.sounds.find(canonical);
    if (it == st.sounds.end()) {
        LogManager::warn("AudioManager::playSound unknown key '{}'", canonical);
        return {};
    }

    auto& record = it->second;

    if (record.placeholder || !isSoundValid(record.sound)) {
        LogManager::warn("AudioManager::playSound using placeholder sound for key '{}'", canonical);
        return {};
    }

    if (st.silentMode || !st.deviceReady) {
        LogManager::debug("AudioManager::playSound silent mode; suppressing playback for '{}'", canonical);
        return {};
    }

    if (st.soundSlots.empty()) {
        LogManager::warn("AudioManager::playSound dropped '{}' (maxConcurrentSounds=0)", canonical);
        return {};
    }

    const auto& api = hooks(st);
    refreshSoundSlotsLocked(st, api);

    auto freeIndex = findFreeSoundSlotIndex(st);
    if (!freeIndex) {
        LogManager::warn("AudioManager::playSound throttled '{}': active={} max={}",
                         canonical,
                         st.activeSoundInstances,
                         st.soundSlots.size());
        return {};
    }

    Sound alias = api.loadSoundAlias(record.sound);
    if (!isSoundValid(alias)) {
        LogManager::error("AudioManager::playSound failed to create alias for '{}'", canonical);
        return {};
    }

    float volume = clamp01(params.volume);
    float finalVolume = clamp01(volume * st.settings.sfxVolume);
    float pitch = clampPitch(params.pitch);
    float pan = clampPan(params.pan);
    api.setSoundVolume(alias, finalVolume);
    api.setSoundPitch(alias, pitch);
    api.setSoundPan(alias, pan);
    api.playSound(alias);

    auto& slot = st.soundSlots[*freeIndex];
    slot.alias = alias;
    slot.key = canonical;
    slot.active = true;
    slot.placeholder = false;
    slot.generation = st.generationCounter++;
    slot.volume = volume;
    slot.pitch = pitch;
    slot.pan = pan;
    st.activeSoundInstances = std::min<std::size_t>(st.activeSoundInstances + 1, st.soundSlots.size());

    return PlaybackHandle{static_cast<int>(*freeIndex), slot.generation};
}

bool AudioManager::stopSound(PlaybackHandle handle) {
    if (!handle.valid()) {
        return false;
    }

    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return false;
    }

    const auto index = static_cast<std::size_t>(handle.slot);
    if (index >= st.soundSlots.size()) {
        return false;
    }

    auto& slot = st.soundSlots[index];
    if (!slot.active || slot.generation != handle.generation) {
        return false;
    }

    const auto& api = hooks(st);
    releaseSoundSlot(slot, api);
    refreshSoundSlotsLocked(st, api);
    return true;
}

bool AudioManager::stopAllSounds() {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return false;
    }
    const auto& api = hooks(st);
    stopAllSoundsLocked(st, api);
    return true;
}

bool AudioManager::isHandleActive(PlaybackHandle handle) {
    if (!handle.valid()) {
        return false;
    }

    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return false;
    }

    const auto index = static_cast<std::size_t>(handle.slot);
    if (index >= st.soundSlots.size()) {
        return false;
    }

    const auto& slot = st.soundSlots[index];
    return slot.active && slot.generation == handle.generation;
}

bool AudioManager::updateSoundPlayback(PlaybackHandle handle, const PlaybackParams& params) {
    if (!handle.valid()) {
        return false;
    }

    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return false;
    }

    const auto index = static_cast<std::size_t>(handle.slot);
    if (index >= st.soundSlots.size()) {
        return false;
    }

    auto& slot = st.soundSlots[index];
    if (!slot.active || slot.generation != handle.generation) {
        return false;
    }

    const auto& api = hooks(st);
    if (!isSoundValid(slot.alias)) {
        return false;
    }

    slot.volume = clamp01(params.volume);
    slot.pitch = clampPitch(params.pitch);
    slot.pan = clampPan(params.pan);

    api.setSoundVolume(slot.alias, clamp01(slot.volume * st.settings.sfxVolume));
    api.setSoundPitch(slot.alias, slot.pitch);
    api.setSoundPan(slot.alias, slot.pan);
    return true;
}

bool AudioManager::playMusic(const std::string& key) {
    auto canonical = canonicalizeKey(key);
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        LogManager::warn("AudioManager::playMusic called before initialization (key='{}')", canonical);
        return false;
    }

    auto it = st.music.find(canonical);
    if (it == st.music.end()) {
        LogManager::warn("AudioManager::playMusic unknown key '{}'", canonical);
        return false;
    }

    auto& record = it->second;
    record.playing = true;
    record.paused = false;

    if (st.silentMode || !st.deviceReady) {
        LogManager::debug("AudioManager::playMusic silent mode; suppressing playback for '{}'", canonical);
        return true;
    }

    if (record.placeholder || !isMusicValid(record.music)) {
        LogManager::warn("AudioManager::playMusic using placeholder music for '{}'", canonical);
        record.playing = false;
        return false;
    }

    const auto& api = hooks(st);
    api.stopMusicStream(record.music);
    api.playMusicStream(record.music);
    float finalVolume = clamp01(record.volume * st.settings.musicVolume);
    api.setMusicVolume(record.music, finalVolume);
    return true;
}

bool AudioManager::pauseMusic(const std::string& key) {
    auto canonical = canonicalizeKey(key);
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return false;
    }

    auto it = st.music.find(canonical);
    if (it == st.music.end()) {
        return false;
    }

    auto& record = it->second;
    if (!record.playing || record.paused) {
        return false;
    }

    record.paused = true;
    if (st.silentMode || !st.deviceReady || record.placeholder || !isMusicValid(record.music)) {
        return true;
    }

    const auto& api = hooks(st);
    api.pauseMusicStream(record.music);
    return true;
}

bool AudioManager::resumeMusic(const std::string& key) {
    auto canonical = canonicalizeKey(key);
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return false;
    }

    auto it = st.music.find(canonical);
    if (it == st.music.end()) {
        return false;
    }

    auto& record = it->second;
    if (!record.playing || !record.paused) {
        return false;
    }

    record.paused = false;

    if (st.silentMode || !st.deviceReady || record.placeholder || !isMusicValid(record.music)) {
        return true;
    }

    const auto& api = hooks(st);
    api.resumeMusicStream(record.music);
    float finalVolume = clamp01(record.volume * st.settings.musicVolume);
    api.setMusicVolume(record.music, finalVolume);
    return true;
}

bool AudioManager::stopMusic(const std::string& key) {
    auto canonical = canonicalizeKey(key);
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return false;
    }

    auto it = st.music.find(canonical);
    if (it == st.music.end()) {
        return false;
    }

    auto& record = it->second;
    if (!record.playing && !record.paused) {
        return false;
    }

    if (st.silentMode || !st.deviceReady || record.placeholder || !isMusicValid(record.music)) {
        record.playing = false;
        record.paused = false;
        return true;
    }

    const auto& api = hooks(st);
    stopMusicRecord(api, record);
    return true;
}

bool AudioManager::setMusicVolume(const std::string& key, float volume) {
    auto canonical = canonicalizeKey(key);
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return false;
    }

    auto it = st.music.find(canonical);
    if (it == st.music.end()) {
        return false;
    }

    auto& record = it->second;
    record.volume = clamp01(volume);

    if (st.silentMode || !st.deviceReady || record.placeholder || !isMusicValid(record.music)) {
        return true;
    }

    const auto& api = hooks(st);
    float finalVolume = clamp01(record.volume * st.settings.musicVolume);
    api.setMusicVolume(record.music, finalVolume);
    return true;
}

bool AudioManager::seekMusic(const std::string& key, float positionSeconds) {
    auto canonical = canonicalizeKey(key);
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return false;
    }

    auto it = st.music.find(canonical);
    if (it == st.music.end()) {
        return false;
    }

    auto& record = it->second;
    if (st.silentMode || !st.deviceReady || record.placeholder || !isMusicValid(record.music)) {
        return true;
    }

    const auto& api = hooks(st);
    api.seekMusicStream(record.music, std::max(positionSeconds, 0.0f));
    return true;
}

MusicPlaybackStatus AudioManager::musicPlaybackStatus(const std::string& key) {
    MusicPlaybackStatus status{};
    auto canonical = canonicalizeKey(key);
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (!st.initialized) {
        return status;
    }

    auto it = st.music.find(canonical);
    if (it == st.music.end()) {
        return status;
    }

    const auto& record = it->second;
    status.valid = true;
    status.playing = record.playing;
    status.paused = record.paused;

    if (record.placeholder) {
        return status;
    }

    if (st.silentMode || !st.deviceReady || !isMusicValid(record.music)) {
        return status;
    }

    const auto& api = hooks(st);
    if (api.getMusicTimeLength) {
        float length = api.getMusicTimeLength(record.music);
        if (!std::isfinite(length) || length < 0.0f) {
            length = 0.0f;
        }
        status.durationSeconds = length;
    }

    if (api.getMusicTimePlayed) {
        float played = api.getMusicTimePlayed(record.music);
        if (!std::isfinite(played) || played < 0.0f) {
            played = 0.0f;
        }
        status.positionSeconds = played;
        if (status.durationSeconds > 0.0f && status.positionSeconds > status.durationSeconds) {
            status.positionSeconds = status.durationSeconds;
        }
    }

    return status;
}

bool AudioManager::reloadAll() {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    bool allSucceeded = true;

    if (!st.initialized) {
        return false;
    }

    if (st.silentMode || !st.deviceReady) {
        LogManager::warn("AudioManager::reloadAll skipped (silent mode)");
        return false;
    }

    const auto& api = hooks(st);
    stopAllSoundsLocked(st, api);
    for (auto& slot : st.soundSlots) {
        slot = ManagerState::SoundSlot{};
    }
    st.activeSoundInstances = 0;

    for (auto& [key, rec] : st.music) {
        (void)key;
        stopMusicRecord(api, rec);
    }

    for (auto& [key, rec] : st.sounds) {
        std::optional<std::filesystem::path> path;
        if (!rec.resolvedPath.empty()) {
            path = std::filesystem::path(rec.resolvedPath);
        } else {
            path = resolvePath(rec.originalIdentifier, st.settings);
        }

        if (!path) {
            LogManager::warn("AudioManager failed to resolve path for sound '{}' during reload", key);
            unloadSoundRecord(rec, api);
            allSucceeded = false;
            continue;
        }

        Sound handle = api.loadSound(path->string().c_str());
        if (isSoundValid(handle)) {
            unloadSoundRecord(rec, api);
            rec.sound = handle;
            rec.placeholder = false;
            rec.resolvedPath = path->string();
            LogManager::info("AudioManager reloaded sound '{}' from '{}'", key, path->string());
        } else {
            LogManager::error("AudioManager failed to reload sound '{}' from '{}'", key, path->string());
            unloadSoundRecord(rec, api);
            rec.resolvedPath = path->string();
            allSucceeded = false;
        }
    }

    for (auto& [key, rec] : st.music) {
        std::optional<std::filesystem::path> path;
        if (!rec.resolvedPath.empty()) {
            path = std::filesystem::path(rec.resolvedPath);
        } else {
            path = resolvePath(rec.originalIdentifier, st.settings);
        }

        if (!path) {
            LogManager::warn("AudioManager failed to resolve path for music '{}' during reload", key);
            unloadMusicRecord(rec, api);
            allSucceeded = false;
            continue;
        }

        Music handle = api.loadMusicStream(path->string().c_str());
        if (isMusicValid(handle)) {
            unloadMusicRecord(rec, api);
            rec.music = handle;
            rec.placeholder = false;
            rec.resolvedPath = path->string();
            LogManager::info("AudioManager reloaded music '{}' from '{}'", key, path->string());
        } else {
            LogManager::error("AudioManager failed to reload music '{}' from '{}'", key, path->string());
            unloadMusicRecord(rec, api);
            rec.resolvedPath = path->string();
            allSucceeded = false;
        }
    }

    return allSucceeded;
}

AudioMetrics AudioManager::metrics() {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    AudioMetrics m;
    m.initialized = st.initialized;
    m.deviceReady = st.deviceReady;
    m.silentMode = st.silentMode;
    m.loadedSounds = st.sounds.size();
    m.loadedMusic = st.music.size();
    m.activeSoundInstances = st.activeSoundInstances;
    m.maxSoundSlots = st.settings.maxConcurrentSounds;
    return m;
}

std::vector<SoundInventoryRecord> AudioManager::captureSoundInventorySnapshot() {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    
    std::vector<SoundInventoryRecord> snapshot;
    snapshot.reserve(st.sounds.size());
    
    for (const auto& [key, record] : st.sounds) {
        SoundInventoryRecord rec;
        rec.key = key;
        rec.path = record.resolvedPath;
        rec.refCount = record.refCount;
        rec.placeholder = record.placeholder;
        
        if (!record.placeholder && record.sound.stream.buffer) {
            // Extract audio properties from raylib Sound
            rec.sampleRate = record.sound.stream.sampleRate;
            rec.channels = record.sound.stream.channels;
            rec.durationSeconds = static_cast<float>(record.sound.frameCount) / 
                                static_cast<float>(record.sound.stream.sampleRate);
        }
        
        snapshot.push_back(std::move(rec));
    }
    
    return snapshot;
}

std::vector<MusicInventoryRecord> AudioManager::captureMusicInventorySnapshot() {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    
    std::vector<MusicInventoryRecord> snapshot;
    snapshot.reserve(st.music.size());
    
    for (const auto& [key, record] : st.music) {
        MusicInventoryRecord rec;
        rec.key = key;
        rec.path = record.resolvedPath;
        rec.refCount = record.refCount;
        rec.placeholder = record.placeholder;
        
        if (!record.placeholder && record.music.stream.buffer) {
            // Extract audio properties from raylib Music
            rec.sampleRate = record.music.stream.sampleRate;
            rec.channels = record.music.stream.channels;
            rec.durationSeconds = static_cast<float>(record.music.frameCount) / 
                                static_cast<float>(record.music.stream.sampleRate);
        }
        
        snapshot.push_back(std::move(rec));
    }
    
    return snapshot;
}

AudioEventSubscription AudioManager::subscribeToAudioEvents(AudioEventSink* sink) {
    if (!sink) {
        return AudioEventSubscription{};
    }
    
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    
    AudioEventSubscription subscription;
    subscription.id = st.nextSubscriptionId++;
    subscription.sink = sink;
    subscription.active = true;
    
    st.eventSubscriptions.push_back(subscription);
    
    return subscription;
}

bool AudioManager::unsubscribeFromAudioEvents(AudioEventSubscription& subscription) {
    if (!subscription.active || subscription.id == 0) {
        return false;
    }
    
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    
    auto it = std::find_if(st.eventSubscriptions.begin(), st.eventSubscriptions.end(),
                          [&](const AudioEventSubscription& sub) {
                              return sub.id == subscription.id;
                          });
    
    if (it != st.eventSubscriptions.end()) {
        it->active = false;
        subscription.active = false;
        return true;
    }
    
    return false;
}

void AudioManager::setBackendForTesting(Backend* backendInstance) {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    st.overrideBackend = backendInstance;
}

void AudioManager::setRaylibHooksForTesting(const RaylibHooks* hookTable) {
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    st.overrideHooks = hookTable;
}

void AudioManager::resetForTesting() {
    shutdown();
    auto& st = state();
    std::scoped_lock lock(st.mutex);
    if (st.deviceReady) {
        backend()->closeDevice();
    }
    st.initialized = false;
    st.deviceReady = false;
    st.silentMode = false;
    st.settings = Settings{};
    st.publishedConfig = AudioConfig{};
    st.overrideBackend = nullptr;
    st.overrideHooks = nullptr;
    st.sounds.clear();
    st.music.clear();
    st.generationCounter = 1;
    st.activeSoundInstances = 0;
    st.soundSlots.clear();
    st.eventSubscriptions.clear();
    st.nextSubscriptionId = 1;
}

} // namespace gb2d::audio
