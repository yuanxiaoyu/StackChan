#include "hal/gamepad/codexpad_protocol.h"
#include <array>
#include <cassert>
#include <cstdio>

using namespace gamepad;

int main()
{
    uint16_t buttons = 0;
    // Real C10 controls are X, Y, A, B and Start; X/Y supply Select/Menu.
    const uint8_t x[] = {0x10, 0, 0, 0, 128, 128, 128, 128};
    assert(decodeCodexPad(x, sizeof x, buttons));
    assert(buttons == Select);
    const uint8_t y[] = {0x20, 0, 0, 0, 128, 128, 128, 128};
    assert(decodeCodexPad(y, sizeof y, buttons));
    assert(buttons == Menu);
    const std::array<uint16_t, 17> expected = {
        Up, Down, Left, Right, Select, Menu, A, B, 0, 0, 0, 0, 0, 0, Select, Start, Menu};
    for (unsigned bit = 0; bit < expected.size(); ++bit) {
        const uint32_t mask = 1u << bit;
        const uint8_t report[] = {uint8_t(mask), uint8_t(mask >> 8), uint8_t(mask >> 16), uint8_t(mask >> 24),
                                  128, 128, 128, 128};
        assert(decodeCodexPad(report, sizeof report, buttons));
        assert(buttons == expected[bit]);
    }
    // C10 physical up increases LY; generic HID's screen-style Y is different.
    const uint8_t up[] = {0, 0, 0, 0, 128, 255, 128, 128};
    assert(decodeCodexPad(up, sizeof up, buttons));
    assert(buttons == Up);
    const uint8_t down[] = {0, 0, 0, 0, 128, 0, 128, 128};
    assert(decodeCodexPad(down, sizeof down, buttons));
    assert(buttons == Down);
    const uint8_t diagonal[] = {0xc0, 0x80, 0, 0, 0, 255, 128, 128};
    assert(decodeCodexPad(diagonal, sizeof diagonal, buttons));
    assert(buttons == (A | B | Start | Left | Up));
    for (uint8_t axis : {uint8_t(64), uint8_t(128), uint8_t(191)}) {
        const uint8_t center[] = {0, 0, 0, 0, axis, axis, 128, 128};
        assert(decodeCodexPad(center, sizeof center, buttons));
        assert(buttons == 0);
    }
    const uint8_t edge[] = {0, 0, 0, 0, 192, 63, 0, 255};
    assert(decodeCodexPad(edge, sizeof edge, buttons));
    assert(buttons == (Right | Down)); // unused right stick must not affect C10
    const uint8_t prefixed[] = {1, 0xc0, 0, 0, 0, 128, 128, 128, 128};
    assert(!decodeCodexPad(prefixed, sizeof prefixed, buttons)); // private protocol has no HID report ID
    assert(!decodeCodexPad(nullptr, 8, buttons));
    assert(!decodeCodexPad(edge, 7, buttons));
    assert(!decodeCodexPad(edge, 0, buttons));
    const uint8_t name[] = "CodexPad-C10";
    assert(isCodexPadAdvertisement(name, sizeof(name) - 1, nullptr, 0));
    assert(!isCodexPadAdvertisement(name, 7, nullptr, 0));
    uint8_t manufacturer[] = {0xff, 0xff, 'C','o','d','e','x','P','a','d', 2, 0, 0, 0, 0, 0, 0, 0};
    assert(isCodexPadAdvertisement(nullptr, 0, manufacturer, sizeof manufacturer));
    for (size_t size = 0; size < sizeof manufacturer; ++size)
        assert(!isCodexPadAdvertisement(nullptr, 0, manufacturer, size));
    manufacturer[0] = 0;
    assert(!isCodexPadAdvertisement(nullptr, 0, manufacturer, sizeof manufacturer));
    assert(!isCodexPadAdvertisement(nullptr, 0, nullptr, 0));
    std::puts("CodexPad C10 controls and packet boundaries passed");
}
