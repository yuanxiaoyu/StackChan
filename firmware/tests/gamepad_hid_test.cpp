#include "hal/gamepad/hid_parser.h"
#include <cassert>
#include <vector>
using namespace gamepad;
int main()
{
    uint16_t codex      = 0;
    const uint8_t c10[] = {0x41, 0xc0, 1, 0, 128, 128, 128, 128};
    assert(decodeCodexPad(c10, sizeof c10, codex));
    assert(codex == (A | Up | Select | Start | Menu));
    const uint8_t stick[] = {0, 0, 0, 0, 0, 255, 128, 128};
    assert(decodeCodexPad(stick, sizeof stick, codex));
    assert(codex == (Left | Down));
    assert(!decodeCodexPad(stick, 7, codex));
    assert(codex == (Left | Down));
    const uint8_t center[] = {0, 0, 0, 0, 128, 128, 128, 128};
    assert(decodeCodexPad(center, sizeof center, codex));
    assert(codex == 0);

    const uint8_t map[] = {0x05, 1,    0x09, 5,    0xa1, 1,    0x85, 1,    0x05, 9,    0x19, 1,    0x29, 10,
                           0x15, 0,    0x25, 1,    0x75, 1,    0x95, 10,   0x81, 2,    0x75, 6,    0x95, 1,
                           0x81, 3,    0x85, 2,    0x05, 1,    0x09, 0x30, 0x09, 0x31, 0x15, 0,    0x26, 0xff,
                           0,    0x75, 8,    0x95, 2,    0x81, 2,    0x09, 0x39, 0x15, 1,    0x25, 8,    0x75,
                           4,    0x95, 1,    0x81, 0x42, 0x75, 4,    0x81, 3,    0xc0};
    HidParser p;
    assert(p.parseMap(map, sizeof map));
    uint8_t buttons[] = {1, 0};
    assert(p.input(1, buttons, 2));
    assert(p.buttons() == A);
    uint8_t axis[] = {0, 128, 9};
    assert(p.input(2, axis, 3));
    assert(p.buttons() == (A | Left));
    uint8_t hat[] = {128, 128, 2};
    assert(p.input(2, hat, 3));
    assert(p.buttons() == (A | Up | Right));
    assert(!p.input(2, hat, 1));
    assert(p.buttons() == (A | Up | Right));
    uint8_t released[] = {0, 0};
    assert(p.input(1, released, 2));
    assert(p.buttons() == (Up | Right));
    assert(!p.input(99, hat, 3));
    p.clear();
    assert(!p.buttons());
    const uint8_t mouse[] = {0x05, 1, 0x09, 2, 0xa1, 1, 0x05, 9, 0x19, 1, 0x29, 2,
                             0x15, 0, 0x25, 1, 0x75, 1, 0x95, 2, 0x81, 2, 0xc0};
    assert(!p.parseMap(mouse, sizeof mouse));
    const uint8_t signedAxes[] = {0x05, 1,    0x09, 4,    0xa1, 1,    0x09, 0x30, 0x09, 0x31, 0x15,
                                  0x81, 0x25, 0x7f, 0x75, 8,    0x95, 2,    0x81, 2,    0xc0};
    assert(p.parseMap(signedAxes, sizeof signedAxes));
    const uint8_t extreme[] = {0x81, 0x7f};
    assert(p.input(0, extreme, sizeof extreme));
    assert(p.buttons() == (Left | Down));
    const uint8_t neutral[] = {0, 0};
    assert(p.input(0, neutral, sizeof neutral));
    assert(p.buttons() == 0);
    uint8_t bad[] = {0x75};
    assert(!p.parseMap(bad, sizeof bad));
    uint8_t overflow[] = {0x75, 32, 0x96, 0xff, 0xff, 0x81, 2};
    assert(!p.parseMap(overflow, sizeof overflow));
    uint8_t unbalanced[] = {0xa1, 1};
    assert(!p.parseMap(unbalanced, sizeof unbalanced));
}
