#pragma once
#include <stdint.h>
#include <stdbool.h>

static inline uint16_t game_dpad_horizontal_left(int32_t dx, int32_t dy, int32_t deadzone)
{
    uint16_t buttons = 0;
    // Rotate the portrait directions 90 degrees counterclockwise: the HAT
    // sits on the user's left when the StickC is held as a game controller.
    if (dx > deadzone) buttons |= 16;    // up
    if (dx < -deadzone) buttons |= 32;   // down
    if (dy > deadzone) buttons |= 64;    // left
    if (dy < -deadzone) buttons |= 128;  // right
    return buttons;
}

static inline uint16_t game_buttons_horizontal_left(int32_t dx, int32_t dy, int32_t deadzone, bool center_pressed)
{
    return game_dpad_horizontal_left(dx, dy, deadzone) | (center_pressed ? 8 : 0);
}

typedef struct { uint32_t since; bool tracking, fired; } game_menu_hold_t;
static inline bool game_menu_hold_update(game_menu_hold_t *hold, bool a, bool b, uint32_t now)
{
    if (!a || b) { hold->tracking = hold->fired = false; return false; }
    if (!hold->tracking) { hold->since = now; hold->tracking = true; }
    if (!hold->fired && now - hold->since >= 700) { hold->fired = true; return true; }
    return false;
}
