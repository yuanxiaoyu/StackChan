#pragma once
#include "gamepad.h"
#include <cstddef>

namespace gamepad {
// C10 uses FFA0 / FFA1, not a HID service or a prefixed HID report.
bool decodeCodexPad(const uint8_t* data, size_t size, uint16_t& buttons);
bool isCodexPadAdvertisement(const uint8_t* name, size_t nameSize,
                            const uint8_t* manufacturer, size_t manufacturerSize);
}  // namespace gamepad
