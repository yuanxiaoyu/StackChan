#include "rom_info.h"
#include <algorithm>
#include <cctype>
#include <cstring>
namespace game {
bool isNesPath(const std::string& path)
{
    if (path.size() < 4) return false;
    auto ext = path.substr(path.size() - 4);
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
    return ext == ".nes";
}
RomInfo inspectRom(const uint8_t* h, size_t n, size_t size)
{
    RomInfo r;
    if (!h || n < 16 || std::memcmp(h, "NES\x1a", 4)) {
        r.error = "Invalid iNES header";
        return r;
    }
    if (size > maxRomSize) {
        r.error = "ROM exceeds 1 MiB limit";
        return r;
    }
    if ((h[7] & 12) != 0) {
        r.error = "NES 2.0/console format unsupported";
        return r;
    }
    if (!h[4]) {
        r.error = "ROM contains no PRG banks";
        return r;
    }
    r.mapper = (h[6] >> 4) | (h[7] & 0xf0);
    // Initially qualify the widely used cartridge families; keep exotic mappers out.
    const unsigned supported[] = {0, 1, 2, 3, 4, 7, 9, 10, 11, 66, 71};
    if (std::find(std::begin(supported), std::end(supported), r.mapper) == std::end(supported)) {
        r.error = "Unsupported mapper " + std::to_string(r.mapper);
        return r;
    }
    if (h[12] || h[13] || h[14] || h[15]) {
        r.error = "Ambiguous legacy iNES header";
        return r;
    }
    r.required = 16 + ((h[6] & 4) ? 512 : 0) + size_t(h[4]) * 16384 + size_t(h[5]) * 8192;
    if (size < r.required) {
        r.error = "Truncated trainer/PRG/CHR data";
        return r;
    }
    r.valid = true;
    return r;
}
}  // namespace game
