#pragma once
#include <cstddef>
#include <cstdint>

namespace game {
inline constexpr size_t FrameWidth = 320;
inline constexpr size_t FrameHeight = 240;
inline constexpr size_t FramePixels = FrameWidth * FrameHeight;

// Source points at the first visible pixel, after the PPU's left overdraw.
void convertFrame(uint16_t* dest, const uint8_t* source, size_t sourcePitch, const uint16_t* palette);
}
