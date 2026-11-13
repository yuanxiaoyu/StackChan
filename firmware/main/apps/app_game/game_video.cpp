#include "game_video.h"

namespace game {
void convertFrame(uint16_t* dest, const uint8_t* source, size_t sourcePitch, const uint16_t* palette)
{
    for (size_t y = 0; y < FrameHeight; ++y) {
        const uint8_t* in = source + y * sourcePitch;
        uint16_t* out = dest + y * FrameWidth;
        // 256 -> 320 nearest-neighbor expansion is exactly four pixels to five.
        // Fuse it with palette lookup; LVGL can then blit without a transform.
        for (size_t group = 0; group < 64; ++group, in += 4, out += 5) {
            out[0] = out[1] = palette[in[0]];
            out[2] = palette[in[1]];
            out[3] = palette[in[2]];
            out[4] = palette[in[3]];
        }
    }
}
}
