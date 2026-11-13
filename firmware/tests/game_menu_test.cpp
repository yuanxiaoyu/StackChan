#include <apps/app_game/game_menu.h>
#include <hal/gamepad/gamepad.h>
#include <cassert>

int main()
{
    using game::pauseMenuSelection;
    // Resume / Games, Start / Select, then the full-width Exit Game row.
    assert(pauseMenuSelection(0, gamepad::Down) == 2);
    assert(pauseMenuSelection(2, gamepad::Down) == 4);
    assert(pauseMenuSelection(1, gamepad::Down) == 3);
    assert(pauseMenuSelection(3, gamepad::Up) == 1);
    assert(pauseMenuSelection(0, gamepad::Right) == 1);
    assert(pauseMenuSelection(3, gamepad::Left) == 2);
    assert(pauseMenuSelection(0, gamepad::Up) == 0);
    assert(pauseMenuSelection(1, gamepad::Right) == 1);
    assert(pauseMenuSelection(4, gamepad::Down) == 4);
    assert(pauseMenuSelection(4, gamepad::Up) == 2);
    assert(pauseMenuSelection(4, gamepad::Left) == 4);
    assert(pauseMenuSelection(2, gamepad::A) == 2);
}
