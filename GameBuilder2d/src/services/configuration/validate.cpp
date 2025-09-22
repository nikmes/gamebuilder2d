#include "validate.h"
using nlohmann::json;

namespace gb2d::cfgvalidate {
bool isValidKey(const std::string& key) {
	if (key.empty()) return false;
	// Keys use dotted segments with [a-z0-9_]+ per segment, no empty segments
	if (key.front() == '.' || key.back() == '.') return false;
	bool prevDot = false;
	for (size_t i = 0; i < key.size(); ++i) {
		char c = key[i];
		if (c == '.') {
			if (prevDot) return false; // no consecutive dots
			prevDot = true;
			continue;
		}
		prevDot = false;
		if (!(c == '_' || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) return false;
	}
	return true;
}

bool isSupportedType(const Value&) { return true; }

bool isSupportedJson(const json& j) {
	if (j.is_boolean() || j.is_string() || j.is_number()) return true;
	if (j.is_array()) {
		for (const auto& e : j) if (!e.is_string()) return false;
		return true;
	}
	return false;
}

bool toValue(const json& j, Value& out) {
	if (j.is_boolean()) { out = j.get<bool>(); return true; }
	if (j.is_number_integer() || j.is_number_unsigned()) { out = j.get<int64_t>(); return true; }
	if (j.is_number_float()) { out = j.get<double>(); return true; }
	if (j.is_string()) { out = j.get<std::string>(); return true; }
	if (j.is_array()) {
		std::vector<std::string> v;
		v.reserve(j.size());
		for (const auto& e : j) {
			if (!e.is_string()) return false;
			v.push_back(e.get<std::string>());
		}
		out = std::move(v);
		return true;
	}
	return false;
}

json toJson(const Value& v) {
	return std::visit([](auto&& val) -> json {
		using T = std::decay_t<decltype(val)>;
		if constexpr (std::is_same_v<T, std::vector<std::string>>) {
			json arr = json::array();
			for (const auto& s : val) arr.push_back(s);
			return arr;
		} else {
			return json(val);
		}
	}, v);
}
}
