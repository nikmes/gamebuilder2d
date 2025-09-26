#include "ui/WindowRegistry.h"

namespace gb2d {

void WindowRegistry::registerType(WindowTypeDesc desc) {
    // ensure unique typeId; replace if duplicate to allow re-registration in dev
    for (auto& t : types_) {
        if (t.typeId == desc.typeId) { t = std::move(desc); return; }
    }
    types_.push_back(std::move(desc));
}

std::unique_ptr<IWindow> WindowRegistry::create(const std::string& typeId, WindowContext& ctx) const {
    for (const auto& t : types_) {
        if (t.typeId == typeId && t.factory) return t.factory(ctx);
    }
    return nullptr;
}

} // namespace gb2d
