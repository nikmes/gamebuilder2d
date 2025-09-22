#pragma once
#include <string>
#include <variant>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace gb2d::cfgvalidate {
using Value = std::variant<bool, int64_t, double, std::string, std::vector<std::string>>;
bool isValidKey(const std::string& key);
bool isSupportedType(const Value&);

// Check whether a JSON value is representable by supported types
bool isSupportedJson(const nlohmann::json& j);

// Convert JSON to Value if supported; returns false if unsupported
bool toValue(const nlohmann::json& j, Value& out);

// Convert Value to JSON
nlohmann::json toJson(const Value& v);
}
