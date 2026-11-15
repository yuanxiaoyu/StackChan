#include "codexpad_protocol.h"
#include <cstring>

namespace gamepad {
// Wire format published by CodexPad/codex_pad_bleak_lib (codex_pad.py).
// Independent implementation; no vendor runtime/library code is included.
bool decodeCodexPad(const uint8_t* p, size_t size, uint16_t& result)
{
    if (!p || size != 8) return false;
    const uint32_t bits = uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
                          (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
    // C10 has no Select/Home. Its extra face buttons X/Y provide those actions.
    static constexpr uint16_t mapping[] = {
        Up, Down, Left, Right, Select, Menu, A, B, 0, 0, 0, 0, 0, 0, Select, Start, Menu};
    uint16_t out = 0;
    for (unsigned i = 0; i < 17; ++i)
        if (bits & (1u << i)) out |= mapping[i];
    if (p[4] < 64) out |= Left;
    else if (p[4] > 191) out |= Right;
    // C10's positive LY points physically up, unlike generic HID screen Y.
    if (p[5] < 64) out |= Down;
    else if (p[5] > 191) out |= Up;
    result = out;
    return true;
}

bool isCodexPadAdvertisement(const uint8_t* name, size_t nameSize,
                            const uint8_t* manufacturer, size_t manufacturerSize)
{
    static constexpr char prefix[] = "CodexPad-";
    if (name && nameSize >= sizeof(prefix) - 1 &&
        std::memcmp(name, prefix, sizeof(prefix) - 1) == 0) return true;
    // NimBLE includes the two-byte little-endian company identifier.
    // Official payload: company=FFFF, "CodexPad", version[3], buttons[4], held[1].
    return manufacturer && manufacturerSize >= 18 &&
           manufacturer[0] == 0xff && manufacturer[1] == 0xff &&
           std::memcmp(manufacturer + 2, "CodexPad", 8) == 0;
}
}  // namespace gamepad
