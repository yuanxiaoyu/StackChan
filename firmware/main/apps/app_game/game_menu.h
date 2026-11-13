#pragma once
#include <cstddef>
#include <cstdint>
#include <hal/gamepad/gamepad.h>

namespace game {
inline size_t pauseMenuSelection(size_t selected, uint16_t pressed)
{
    // Follow the on-screen rows and columns; an edge keeps its selection.
    // Exit Game spans both columns and moves up to Start.
    static constexpr size_t adjacent[5][4] = {
        {0, 2, 0, 1}, {1, 3, 0, 1}, {0, 4, 2, 3},
        {1, 4, 2, 3}, {2, 4, 4, 4},
    };
    if (selected >= 5) return 0;
    if (pressed & gamepad::Up) return adjacent[selected][0];
    if (pressed & gamepad::Down) return adjacent[selected][1];
    if (pressed & gamepad::Left) return adjacent[selected][2];
    if (pressed & gamepad::Right) return adjacent[selected][3];
    return selected;
}
}
