#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace gb2d::jsonio {
std::optional<nlohmann::json> readJson(const std::string& path);
bool writeJsonAtomic(const std::string& path, const nlohmann::json& j);
}
