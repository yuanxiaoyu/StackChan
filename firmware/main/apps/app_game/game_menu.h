#pragma once
#include <cstddef>
#include <cstdint>
#include <hal/gamepad/gamepad.h>

namespace game {
inline uint16_t pauseMenuNavigation(uint16_t pressed, uint16_t held, uint32_t now, uint32_t& lastMove)
{
    constexpr uint16_t directions = gamepad::Up | gamepad::Down | gamepad::Left | gamepad::Right;
    uint16_t navigation = pressed & directions;
    if (!navigation && now - lastMove >= 180) navigation = held & directions;
    if (navigation) lastMove = now;
    return navigation;
}

struct JackalContinueInputState {
    uint16_t previousDirections = 0;
    uint32_t lastSelect = 0;
    bool hasSelected = false;
};

inline uint16_t jackalContinueInput(uint16_t buttons, bool isJackal, uint8_t controlState, uint8_t subState,
                                    uint32_t now, JackalContinueInputState& state)
{
    constexpr uint16_t horizontal = gamepad::Left | gamepad::Right;
    if (!isJackal || controlState != 8 || subState != 0) {
        state.previousDirections = 0;
        state.lastSelect = 0;
        state.hasSelected = false;
        return buttons;
    }

    const uint16_t directions = buttons & horizontal;
    const bool pressed = directions && directions != state.previousDirections;
    const bool repeat = directions && state.hasSelected && now - state.lastSelect >= 180;
    state.previousDirections = directions;
    if (pressed || repeat) {
        state.lastSelect = now;
        state.hasSelected = true;
        buttons |= gamepad::Select;
    }
    return buttons;
}

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
