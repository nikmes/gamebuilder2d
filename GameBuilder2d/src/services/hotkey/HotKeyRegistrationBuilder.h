#pragma once

#include <string>
#include <string_view>
#include <utility>

#include "services/hotkey/HotKeyManager.h"
#include "services/hotkey/ShortcutUtils.h"

namespace gb2d::hotkeys {

struct HotKeyActionDesc {
    std::string id;
    std::string label;
    std::string category;
    std::string context;
    std::string defaultShortcut;
    std::string description;
};

HotKeyAction makeActionFromString(std::string id,
                                  std::string label,
                                  std::string category,
                                  std::string context,
                                  std::string defaultShortcut,
                                  std::string description = {});

HotKeyAction makeActionFromBinding(std::string id,
                                   std::string label,
                                   std::string category,
                                   std::string context,
                                   ShortcutBinding binding,
                                   std::string description = {});

class HotKeyRegistrationBuilder {
public:
    HotKeyRegistrationBuilder& reserve(std::size_t count);

    HotKeyRegistrationBuilder& add(HotKeyAction action);
    HotKeyRegistrationBuilder& add(const HotKeyActionDesc& desc);
    HotKeyRegistrationBuilder& add(std::string id,
                                   std::string label,
                                   std::string category,
                                   std::string context,
                                   std::string defaultShortcut,
                                   std::string description = {});
    HotKeyRegistrationBuilder& add(std::string id,
                                   std::string label,
                                   std::string category,
                                   std::string context,
                                   ShortcutBinding binding,
                                   std::string description = {});

    HotKeyRegistrationBuilder& withDefaults(std::string category, std::string context);

    HotKeyRegistrationBuilder& addWithDefaults(std::string id,
                                              std::string label,
                                              std::string defaultShortcut,
                                              std::string description = {});
    HotKeyRegistrationBuilder& addWithDefaults(std::string id,
                                              std::string label,
                                              ShortcutBinding binding,
                                              std::string description = {});

    HotKeyRegistration build() &&;

private:
    HotKeyRegistration registration_{};
    std::string defaultCategory_{};
    std::string defaultContext_{};
};

} // namespace gb2d::hotkeys
