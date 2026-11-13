#include "apps/app_game/rom_info.h"
#include <array>
#include <cassert>
int main()
{
    std::array<uint8_t, 16> h{'N', 'E', 'S', 0x1a, 1, 1};
    assert(game::isNesPath("/room/a.NES"));
    assert(!game::isNesPath("/room/a.nes.zip"));
    assert(game::inspectRom(h.data(), 16, 24592).valid);
    assert(!game::inspectRom(h.data(), 15, 24592).valid);
    assert(!game::inspectRom(h.data(), 16, 24591).valid);
    h[6] = 4;
    assert(!game::inspectRom(h.data(), 16, 24592).valid);
    assert(game::inspectRom(h.data(), 16, 25104).valid);
    h[7] = 8;
    assert(!game::inspectRom(h.data(), 16, 25104).valid);
    h[7] = 0;
    h[6] = 0xf0;
    assert(!game::inspectRom(h.data(), 16, 25104).valid);
    h[6] = 0;
    assert(!game::inspectRom(h.data(), 16, game::maxRomSize + 1).valid);
    h[4] = 0;
    assert(!game::inspectRom(h.data(), 16, 24592).valid);
    h[0] = 0;
    assert(!game::inspectRom(h.data(), 16, 24592).valid);

    // Jackal on the user's SD card: Mapper 2 with CHR RAM (zero CHR-ROM banks).
    std::array<uint8_t, 16> jackal{'N', 'E', 'S', 0x1a, 8, 0, 0x21, 0};
    assert(game::isNesPath("/sdcard/rom/000_赤色要塞 - Jackal (U).nes"));
    const auto jackal_info = game::inspectRom(jackal.data(), jackal.size(), 131088);
    assert(jackal_info.valid && jackal_info.mapper == 2 && jackal_info.required == 131088);
}
