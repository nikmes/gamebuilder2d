#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "raylib.h"
#include "services/hotkey/HotKeyManager.h"

namespace gb2d::hotkeys {

constexpr std::uint32_t kModifierCtrl  = 1u << 0;
constexpr std::uint32_t kModifierShift = 1u << 1;
constexpr std::uint32_t kModifierAlt   = 1u << 2;
constexpr std::uint32_t kModifierSuper = 1u << 3; // Cmd (macOS) / Win / Super

ShortcutBinding parseShortcut(std::string_view text);
ShortcutBinding buildShortcut(std::uint32_t keyCode, std::uint32_t modifiers, std::string keyToken = {});
std::string toString(const ShortcutBinding& binding);
std::size_t hashShortcut(const ShortcutBinding& binding);
bool equalsShortcut(const ShortcutBinding& lhs, const ShortcutBinding& rhs);

struct ShortcutBindingHash {
    std::size_t operator()(const ShortcutBinding& binding) const {
        return hashShortcut(binding);
    }
};

struct ShortcutBindingEqual {
    bool operator()(const ShortcutBinding& lhs, const ShortcutBinding& rhs) const {
        return equalsShortcut(lhs, rhs);
    }
};

} // namespace gb2d::hotkeys
