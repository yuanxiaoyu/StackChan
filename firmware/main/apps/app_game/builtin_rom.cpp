#include "builtin_rom.h"

#ifdef STACKCHAN_HAS_BUILTIN_ROM
extern const uint8_t gevara_start[] asm("_binary_stackchan_gevara_start");
extern const uint8_t gevara_end[] asm("_binary_stackchan_gevara_end");
#endif

namespace game {
BuiltinRom builtinRom()
{
#ifdef STACKCHAN_HAS_BUILTIN_ROM
    return {gevara_start, size_t(gevara_end - gevara_start)};
#else
    return {nullptr, 0};
#endif
}
}  // namespace game
