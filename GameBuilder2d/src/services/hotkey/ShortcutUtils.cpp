#include "services/hotkey/ShortcutUtils.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <unordered_map>
#include <vector>

namespace gb2d::hotkeys {

namespace {

std::string trimCopy(std::string_view text) {
    auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) {
        return {};
    }
    auto end = text.find_last_not_of(" \t\r\n");
    return std::string{text.substr(begin, end - begin + 1)};
}

std::string toLower(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return out;
}

bool isAlpha(char ch) {
    return ch >= 'a' && ch <= 'z';
}

bool isDigit(char ch) {
    return ch >= '0' && ch <= '9';
}

struct KeyMappingEntry {
    std::string lowercaseToken;
    int keyCode;
    std::string canonicalToken;
};

const std::vector<KeyMappingEntry>& staticMappings() {
    static const std::vector<KeyMappingEntry> kMappings = {
        {"space", KEY_SPACE, "Space"},
        {"enter", KEY_ENTER, "Enter"},
        {"return", KEY_ENTER, "Enter"},
        {"tab", KEY_TAB, "Tab"},
        {"backspace", KEY_BACKSPACE, "Backspace"},
        {"escape", KEY_ESCAPE, "Esc"},
        {"esc", KEY_ESCAPE, "Esc"},
        {"delete", KEY_DELETE, "Delete"},
        {"insert", KEY_INSERT, "Insert"},
        {"home", KEY_HOME, "Home"},
        {"end", KEY_END, "End"},
        {"pageup", KEY_PAGE_UP, "PageUp"},
        {"pagedown", KEY_PAGE_DOWN, "PageDown"},
        {"up", KEY_UP, "Up"},
        {"down", KEY_DOWN, "Down"},
        {"left", KEY_LEFT, "Left"},
        {"right", KEY_RIGHT, "Right"},
        {"minus", KEY_MINUS, "-"},
        {"hyphen", KEY_MINUS, "-"},
    {"-", KEY_MINUS, "-"},
        {"equals", KEY_EQUAL, "="},
        {"equal", KEY_EQUAL, "="},
    {"=", KEY_EQUAL, "="},
        {"comma", KEY_COMMA, ","},
    {",", KEY_COMMA, ","},
        {"period", KEY_PERIOD, "."},
    {".", KEY_PERIOD, "."},
        {"slash", KEY_SLASH, "/"},
    {"/", KEY_SLASH, "/"},
        {"backslash", KEY_BACKSLASH, "\\"},
    {"\\", KEY_BACKSLASH, "\\"},
        {"semicolon", KEY_SEMICOLON, ";"},
    {";", KEY_SEMICOLON, ";"},
        {"apostrophe", KEY_APOSTROPHE, "'"},
        {"quote", KEY_APOSTROPHE, "'"},
    {"'", KEY_APOSTROPHE, "'"},
        {"grave", KEY_GRAVE, "`"},
        {"tilde", KEY_GRAVE, "`"},
    {"`", KEY_GRAVE, "`"},
        {"capslock", KEY_CAPS_LOCK, "CapsLock"},
        {"scrolllock", KEY_SCROLL_LOCK, "ScrollLock"},
        {"numlock", KEY_NUM_LOCK, "NumLock"},
        {"printscreen", KEY_PRINT_SCREEN, "PrintScreen"},
        {"pause", KEY_PAUSE, "Pause"},
    };
    return kMappings;
}

bool parseFunctionKey(const std::string& token, int& outKeyCode, std::string& canonical) {
    if (token.size() < 2 || token[0] != 'f') {
        return false;
    }
    int value = 0;
    auto [ptr, ec] = std::from_chars(token.data() + 1, token.data() + token.size(), value);
    if (ec != std::errc() || value < 1 || value > 24) {
        return false;
    }
    outKeyCode = KEY_F1 + (value - 1);
    canonical = "F" + std::to_string(value);
    return true;
}

bool parseNumpadKey(const std::string& token, int& outKeyCode, std::string& canonical) {
    if (token.rfind("numpad", 0) != 0 || token.size() <= 6) {
        return false;
    }
    const std::string suffix = token.substr(6);
    if (suffix == "enter") {
        outKeyCode = KEY_KP_ENTER;
        canonical = "NumEnter";
        return true;
    }
    if (suffix == "plus" || suffix == "+") {
        outKeyCode = KEY_KP_ADD;
        canonical = "Num+";
        return true;
    }
    if (suffix == "minus" || suffix == "-") {
        outKeyCode = KEY_KP_SUBTRACT;
        canonical = "Num-";
        return true;
    }
    if (suffix == "multiply" || suffix == "*") {
        outKeyCode = KEY_KP_MULTIPLY;
        canonical = "Num*";
        return true;
    }
    if (suffix == "divide" || suffix == "/") {
        outKeyCode = KEY_KP_DIVIDE;
        canonical = "Num/";
        return true;
    }
    if (suffix == "decimal" || suffix == ".") {
        outKeyCode = KEY_KP_DECIMAL;
        canonical = "Num.";
        return true;
    }
    if (suffix.size() == 1 && isDigit(suffix[0])) {
        outKeyCode = KEY_KP_0 + (suffix[0] - '0');
        canonical = std::string{"Num"} + suffix;
        return true;
    }
    return false;
}

bool parseArrowWord(const std::string& token, int& outKeyCode, std::string& canonical) {
    if (token == "arrowup") {
        outKeyCode = KEY_UP;
        canonical = "Up";
        return true;
    }
    if (token == "arrowdown") {
        outKeyCode = KEY_DOWN;
        canonical = "Down";
        return true;
    }
    if (token == "arrowleft") {
        outKeyCode = KEY_LEFT;
        canonical = "Left";
        return true;
    }
    if (token == "arrowright") {
        outKeyCode = KEY_RIGHT;
        canonical = "Right";
        return true;
    }
    return false;
}

bool parseBaseKey(const std::string& token, int& outKeyCode, std::string& canonical) {
    std::string lower = toLower(token);

    if (lower.size() == 1 && isAlpha(lower[0])) {
        outKeyCode = KEY_A + (lower[0] - 'a');
        canonical.assign(1, static_cast<char>(std::toupper(lower[0])));
        return true;
    }
    if (lower.size() == 1 && isDigit(lower[0])) {
        outKeyCode = KEY_ZERO + (lower[0] - '0');
        canonical = std::string{lower};
        return true;
    }
    if (parseFunctionKey(lower, outKeyCode, canonical)) {
        return true;
    }
    if (parseNumpadKey(lower, outKeyCode, canonical)) {
        return true;
    }
    if (parseArrowWord(lower, outKeyCode, canonical)) {
        return true;
    }

    for (const auto& entry : staticMappings()) {
        if (lower == entry.lowercaseToken) {
            outKeyCode = entry.keyCode;
            canonical = entry.canonicalToken;
            return true;
        }
    }

    return false;
}

std::string canonicalKeyToken(int keyCode, std::string keyToken) {
    if (!keyToken.empty()) {
        return keyToken;
    }

    if (keyCode >= KEY_A && keyCode <= KEY_Z) {
        char ch = static_cast<char>('A' + (keyCode - KEY_A));
        return std::string{1, ch};
    }
    if (keyCode >= KEY_ZERO && keyCode <= KEY_NINE) {
        char ch = static_cast<char>('0' + (keyCode - KEY_ZERO));
        return std::string{1, ch};
    }
    if (keyCode >= KEY_F1 && keyCode <= KEY_F12) {
        return "F" + std::to_string(1 + keyCode - KEY_F1);
    }

    for (const auto& entry : staticMappings()) {
        if (entry.keyCode == keyCode) {
            return entry.canonicalToken;
        }
    }

    switch (keyCode) {
        case KEY_KP_ENTER:     return "NumEnter";
        case KEY_KP_ADD:       return "Num+";
        case KEY_KP_SUBTRACT:  return "Num-";
        case KEY_KP_MULTIPLY:  return "Num*";
        case KEY_KP_DIVIDE:    return "Num/";
        case KEY_KP_DECIMAL:   return "Num.";
        default:               return {};
    }
}

std::string formatShortcut(std::uint32_t modifiers, const std::string& keyToken) {
    std::vector<std::string> parts;
    parts.reserve(4);
    if (modifiers & kModifierCtrl)  parts.emplace_back("Ctrl");
    if (modifiers & kModifierShift) parts.emplace_back("Shift");
    if (modifiers & kModifierAlt)   parts.emplace_back("Alt");
    if (modifiers & kModifierSuper) parts.emplace_back("Super");
    if (!keyToken.empty())          parts.emplace_back(keyToken);

    std::string result;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += '+';
        result += parts[i];
    }
    return result;
}

} // namespace

ShortcutBinding parseShortcut(std::string_view text) {
    ShortcutBinding binding{};
    std::string trimmed = trimCopy(text);
    if (trimmed.empty()) {
        return binding;
    }

    std::uint32_t modifiers = 0;
    int keyCode = 0;
    std::string keyToken;
    bool keySet = false;

    std::size_t start = 0;
    while (start <= trimmed.size()) {
        std::size_t plusPos = trimmed.find('+', start);
        std::string token = trimCopy(trimmed.substr(start, plusPos - start));
        if (!token.empty()) {
            std::string lower = toLower(token);
            if (lower == "ctrl" || lower == "control") {
                modifiers |= kModifierCtrl;
            } else if (lower == "shift") {
                modifiers |= kModifierShift;
            } else if (lower == "alt" || lower == "option") {
                modifiers |= kModifierAlt;
            } else if (lower == "cmd" || lower == "command" || lower == "super" || lower == "meta" || lower == "win") {
                modifiers |= kModifierSuper;
            } else {
                if (keySet) {
                    return ShortcutBinding{};
                }
                if (!parseBaseKey(token, keyCode, keyToken)) {
                    return ShortcutBinding{};
                }
                keySet = true;
            }
        }

        if (plusPos == std::string::npos) break;
        start = plusPos + 1;
    }

    if (!keySet) {
        return ShortcutBinding{};
    }

    binding.keyCode = static_cast<std::uint32_t>(keyCode);
    binding.modifiers = modifiers;
    binding.keyToken = canonicalKeyToken(keyCode, keyToken);
    binding.humanReadable = formatShortcut(modifiers, binding.keyToken);
    binding.valid = !binding.humanReadable.empty();
    return binding;
}

ShortcutBinding buildShortcut(std::uint32_t keyCode, std::uint32_t modifiers, std::string keyToken) {
    ShortcutBinding binding{};
    if (keyCode == 0) {
        return binding;
    }
    binding.keyCode = keyCode;
    binding.modifiers = modifiers;
    binding.keyToken = canonicalKeyToken(static_cast<int>(keyCode), std::move(keyToken));
    binding.humanReadable = formatShortcut(modifiers, binding.keyToken);
    binding.valid = !binding.humanReadable.empty();
    return binding;
}

std::string toString(const ShortcutBinding& binding) {
    if (!binding.valid) {
        return {};
    }
    return binding.humanReadable;
}

std::size_t hashShortcut(const ShortcutBinding& binding) {
    if (!binding.valid) {
        return 0u;
    }
    std::uint64_t combined = (static_cast<std::uint64_t>(binding.keyCode) << 32) |
                              static_cast<std::uint64_t>(binding.modifiers);
    return std::hash<std::uint64_t>{}(combined);
}

bool equalsShortcut(const ShortcutBinding& lhs, const ShortcutBinding& rhs) {
    if (!lhs.valid || !rhs.valid) {
        return false;
    }
    return lhs.keyCode == rhs.keyCode && lhs.modifiers == rhs.modifiers;
}

} // namespace gb2d::hotkeys
