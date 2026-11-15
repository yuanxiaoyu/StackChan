#include "apps/app_game/game_input.h"
#include "apps/app_game/game_remote.h"
#include <cassert>

int main()
{
    using game::InputTransport;
    assert(game::chooseInputTransport(true, false) == InputTransport::Ble);
    assert(game::chooseInputTransport(true, true) == InputTransport::Ble);
    assert(game::chooseInputTransport(false, true) == InputTransport::Ble);
    assert(game::chooseInputTransport(false, false) == InputTransport::EspNow);
    assert(game::bluetoothControllerLost(InputTransport::Ble, true, false));
    assert(!game::bluetoothControllerLost(InputTransport::Ble, false, false));
    assert(!game::bluetoothControllerLost(InputTransport::Ble, true, true));
    assert(!game::bluetoothControllerLost(InputTransport::EspNow, true, false));
    game::ConnectionPause radio;
    assert(!radio.update(true, 100));
    assert(!radio.update(false, 200)); // original remote still tolerates a short dropout
    assert(radio.update(false, 1700));
}
