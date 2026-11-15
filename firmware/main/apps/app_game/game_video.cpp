#include "game_video.h"

namespace game {
void convertFrame(uint16_t* dest, const uint8_t* source, size_t sourcePitch, const uint16_t* palette)
{
    for (size_t y = 0; y < FrameHeight; ++y) {
        const uint8_t* in = source + y * sourcePitch;
        uint16_t* out = dest + y * FrameWidth;
        for (size_t x = 0; x < FrameWidth; ++x) {
            // Scale the NES 256-pixel active area to the CoreS3's 320-pixel LCD.
            out[x] = palette[in[(x * 256) / FrameWidth]];
        }
    }
}
}
