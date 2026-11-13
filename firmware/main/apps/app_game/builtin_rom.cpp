#include "builtin_rom.h"

#ifdef STACKCHAN_HAS_BUILTIN_ROM
extern const uint8_t jackal_start[] asm("_binary_stackchan_jackal_start");
extern const uint8_t jackal_end[] asm("_binary_stackchan_jackal_end");
#endif

namespace game {
BuiltinRom builtinRom()
{
#ifdef STACKCHAN_HAS_BUILTIN_ROM
    return {jackal_start, size_t(jackal_end - jackal_start)};
#else
    return {nullptr, 0};
#endif
}
}  // namespace game
