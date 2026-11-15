#include <apps/app_game/game_menu.h>
#include <hal/gamepad/gamepad.h>
#include <cassert>

int main()
{
    using game::pauseMenuSelection;
    uint32_t lastMove = 0;
    assert(game::pauseMenuNavigation(gamepad::Down, gamepad::Down, 20, lastMove) == gamepad::Down);
    assert(lastMove == 20);
    assert(game::pauseMenuNavigation(0, gamepad::Down, 199, lastMove) == 0);
    assert(game::pauseMenuNavigation(0, gamepad::Down, 200, lastMove) == gamepad::Down);
    assert(lastMove == 200);
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

    game::JackalContinueInputState jackalInput;
    assert(game::jackalContinueInput(gamepad::Left, true, 0, 0, 10, jackalInput) == gamepad::Left);
    assert(game::jackalContinueInput(gamepad::Left, false, 8, 0, 20, jackalInput) == gamepad::Left);
    assert(game::jackalContinueInput(gamepad::Left, true, 8, 0, 30, jackalInput) == (gamepad::Left | gamepad::Select));
    assert(game::jackalContinueInput(gamepad::Left, true, 8, 0, 40, jackalInput) == gamepad::Left);
    assert(game::jackalContinueInput(gamepad::Left, true, 8, 1, 220, jackalInput) == gamepad::Left);
    assert(game::jackalContinueInput(0, true, 8, 0, 230, jackalInput) == 0);
    assert(game::jackalContinueInput(gamepad::Right, true, 8, 0, 240, jackalInput) == (gamepad::Right | gamepad::Select));
    assert(game::jackalContinueInput(gamepad::Right, true, 8, 0, 420, jackalInput) == (gamepad::Right | gamepad::Select));
    assert(game::jackalContinueInput(gamepad::Right, true, 8, 0, 430, jackalInput) == gamepad::Right);

    game::JackalContinueInputState zeroTimeJackalInput;
    assert(game::jackalContinueInput(gamepad::Left, true, 8, 0, 0, zeroTimeJackalInput) == (gamepad::Left | gamepad::Select));
    assert(game::jackalContinueInput(gamepad::Left, true, 8, 0, 180, zeroTimeJackalInput) == (gamepad::Left | gamepad::Select));
}
