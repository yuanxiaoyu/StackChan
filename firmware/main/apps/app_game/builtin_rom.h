#pragma once
#include <cstddef>
#include <cstdint>

namespace game {
inline constexpr char builtinRomPath[] = "builtin:gevara";
struct BuiltinRom {
    const uint8_t* data;
    size_t size;
};
BuiltinRom builtinRom();
}  // namespace game
