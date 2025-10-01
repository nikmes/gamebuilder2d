#pragma once

#include <cstdint>
#include <type_traits>

namespace gb2d::ui {

template <typename ImTexId>
constexpr ImTexId makeImTextureId(unsigned int textureId) {
    if constexpr (std::is_pointer_v<ImTexId>) {
        return reinterpret_cast<ImTexId>(static_cast<intptr_t>(textureId));
    } else {
        return static_cast<ImTexId>(textureId);
    }
}

} // namespace gb2d::ui
