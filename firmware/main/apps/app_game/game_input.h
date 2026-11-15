#pragma once
#include "hal/gamepad/gamepad.h"

namespace game {
enum class InputTransport { EspNow, Ble };

constexpr InputTransport chooseInputTransport(bool savedGamepad, bool bleActive)
{
    // Keep the BLE path even while a remembered controller is powered off.
    // Starting ESP-NOW first prevents BLE from obtaining its internal buffers.
    return savedGamepad || bleActive ? InputTransport::Ble : InputTransport::EspNow;
}

constexpr bool bluetoothControllerLost(InputTransport transport, bool wasConnected, bool connected)
{
    return transport == InputTransport::Ble && wasConnected && !connected;
}
}  // namespace game
