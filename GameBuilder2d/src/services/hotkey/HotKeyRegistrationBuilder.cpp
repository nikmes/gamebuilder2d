#include "services/hotkey/HotKeyRegistrationBuilder.h"

#include <string_view>

namespace gb2d::hotkeys {

namespace {
std::string trimShortcut(std::string_view text) {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) {
        return {};
    }
    const auto end = text.find_last_not_of(" \t\r\n");
    return std::string{text.substr(begin, end - begin + 1)};
}

ShortcutBinding normalizeBinding(ShortcutBinding binding, const std::string& fallbackLabel) {
    if (!binding.valid) {
        ShortcutBinding result{};
        result.humanReadable = fallbackLabel;
        result.keyToken = fallbackLabel;
        return result;
    }
    if (binding.humanReadable.empty()) {
        binding.humanReadable = toString(binding);
    }
    return binding;
}

HotKeyAction makeActionInternal(std::string id,
                                std::string label,
                                std::string category,
                                std::string context,
                                ShortcutBinding binding,
                                std::string description) {
    return HotKeyAction{
        std::move(id),
        std::move(label),
        std::move(category),
        std::move(context),
        std::move(binding),
        std::move(description)
    };
}

} // namespace

HotKeyAction makeActionFromString(std::string id,
                                  std::string label,
                                  std::string category,
                                  std::string context,
                                  std::string defaultShortcut,
                                  std::string description) {
    std::string trimmed = trimShortcut(defaultShortcut);
    ShortcutBinding binding = parseShortcut(trimmed);
    binding = normalizeBinding(std::move(binding), trimmed);
    return makeActionInternal(std::move(id),
                              std::move(label),
                              std::move(category),
                              std::move(context),
                              std::move(binding),
                              std::move(description));
}

HotKeyAction makeActionFromBinding(std::string id,
                                   std::string label,
                                   std::string category,
                                   std::string context,
                                   ShortcutBinding binding,
                                   std::string description) {
    binding = normalizeBinding(std::move(binding), {});
    return makeActionInternal(std::move(id),
                              std::move(label),
                              std::move(category),
                              std::move(context),
                              std::move(binding),
                              std::move(description));
}

HotKeyRegistrationBuilder& HotKeyRegistrationBuilder::reserve(std::size_t count) {
    registration_.actions.reserve(count);
    return *this;
}

HotKeyRegistrationBuilder& HotKeyRegistrationBuilder::add(HotKeyAction action) {
    registration_.actions.emplace_back(std::move(action));
    return *this;
}

HotKeyRegistrationBuilder& HotKeyRegistrationBuilder::add(const HotKeyActionDesc& desc) {
    return add(desc.id,
               desc.label,
               desc.category,
               desc.context,
               desc.defaultShortcut,
               desc.description);
}

HotKeyRegistrationBuilder& HotKeyRegistrationBuilder::add(std::string id,
                                                          std::string label,
                                                          std::string category,
                                                          std::string context,
                                                          std::string defaultShortcut,
                                                          std::string description) {
    registration_.actions.emplace_back(makeActionFromString(std::move(id),
                                                            std::move(label),
                                                            std::move(category),
                                                            std::move(context),
                                                            std::move(defaultShortcut),
                                                            std::move(description)));
    return *this;
}

HotKeyRegistrationBuilder& HotKeyRegistrationBuilder::add(std::string id,
                                                          std::string label,
                                                          std::string category,
                                                          std::string context,
                                                          ShortcutBinding binding,
                                                          std::string description) {
    registration_.actions.emplace_back(makeActionFromBinding(std::move(id),
                                                             std::move(label),
                                                             std::move(category),
                                                             std::move(context),
                                                             std::move(binding),
                                                             std::move(description)));
    return *this;
}

HotKeyRegistrationBuilder& HotKeyRegistrationBuilder::withDefaults(std::string category, std::string context) {
    defaultCategory_ = std::move(category);
    defaultContext_ = std::move(context);
    return *this;
}

HotKeyRegistrationBuilder& HotKeyRegistrationBuilder::addWithDefaults(std::string id,
                                                                      std::string label,
                                                                      std::string defaultShortcut,
                                                                      std::string description) {
    return add(std::move(id),
               std::move(label),
               defaultCategory_,
               defaultContext_,
               std::move(defaultShortcut),
               std::move(description));
}

HotKeyRegistrationBuilder& HotKeyRegistrationBuilder::addWithDefaults(std::string id,
                                                                      std::string label,
                                                                      ShortcutBinding binding,
                                                                      std::string description) {
    return add(std::move(id),
               std::move(label),
               defaultCategory_,
               defaultContext_,
               std::move(binding),
               std::move(description));
}

HotKeyRegistration HotKeyRegistrationBuilder::build() && {
    return std::move(registration_);
}

} // namespace gb2d::hotkeys
