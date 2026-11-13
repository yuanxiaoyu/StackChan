#include "apps/app_game/game_video.h"
#include <array>
#include <cassert>
#include <vector>

int main()
{
    // Fill visible pixels with a pattern that changes along both axes. Padding
    // has a separate color, so sampling the PPU overdraw also fails this test.
    constexpr size_t width = 320, height = 240, pitch = 272, left = 8;
    std::vector<uint8_t> source(pitch * height, 255);
    std::array<uint16_t, 256> palette{};
    for (size_t i = 0; i < palette.size(); ++i) palette[i] = uint16_t(0x4000 + i);
    for (size_t y = 0; y < height; ++y)
        for (size_t x = 0; x < 256; ++x)
            source[y * pitch + left + x] = uint8_t((x + 17 * y) % 251);
    std::vector<uint16_t> output(width * height + 2, 0xDEAD);
    game::convertFrame(output.data() + 1, source.data() + left, pitch, palette.data());
    assert(game::FrameWidth == width && game::FrameHeight == height);
    for (size_t y = 0; y < height; ++y)
        for (size_t x = 0; x < width; ++x) {
            const size_t sourceX = x * 256 / width;
            assert(output[1 + y * width + x] == palette[(sourceX + 17 * y) % 251]);
        }
    assert(output.front() == 0xDEAD && output.back() == 0xDEAD);
}
