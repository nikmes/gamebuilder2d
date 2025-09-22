#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace gb2d::jsonio {

// Maximum allowed size for a config file read (bytes). Files larger than this
// are treated as unreadable.
inline constexpr std::uintmax_t kMaxConfigBytes = 1u * 1024u * 1024u; // 1 MiB
std::optional<nlohmann::json> readJson(const std::string& path);
bool writeJsonAtomic(const std::string& path, const nlohmann::json& j);
}
