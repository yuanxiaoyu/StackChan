#include <apps/app_game/game_remote.h>
#include <cassert>
#include <cstdint>
#include <joystick/game_controls.h>

int main()
{
    // The HAT is on the left when held horizontally: portrait right becomes
    // physical up, and portrait up becomes physical left.
    assert(game_dpad_horizontal_left(1000, 0, 300) == 16);
    assert(game_dpad_horizontal_left(-1000, 0, 300) == 32);
    assert(game_dpad_horizontal_left(0, 1000, 300) == 64);
    assert(game_dpad_horizontal_left(0, -1000, 300) == 128);
    assert(game_dpad_horizontal_left(1000, 1000, 300) == (16 | 64));
    assert(game_dpad_horizontal_left(300, -300, 300) == 0);

    // Center press reaches the host immediately, stays Start through a long
    // hold, and releases without generating Menu or a second Start pulse.
    game::RemoteInput center;
    uint8_t center_packet[] = {'G', 'P', 1, 0, 0, 0, 0};
    for (uint32_t now = 100; now <= 900; now += 100) {
        const auto buttons = game_buttons_horizontal_left(0, 0, 300, true);
        center_packet[4] = buttons;
        center_packet[5] = buttons >> 8;
        ++center_packet[6];
        assert(center.receive(center_packet, sizeof(center_packet), now));
        assert(center.read(now).buttons == 8);
    }
    center_packet[4] = game_buttons_horizontal_left(0, 0, 300, false);
    ++center_packet[6];
    assert(center.receive(center_packet, sizeof(center_packet), 1000));
    assert(center.read(1000).buttons == 0);
    assert(game_buttons_horizontal_left(1000, -1000, 300, true) == (16 | 128 | 8));
    game_menu_hold_t menu{};
    assert(!game_menu_hold_update(&menu, true, false, 100));
    assert(!game_menu_hold_update(&menu, true, false, 799));
    assert(game_menu_hold_update(&menu, true, false, 800));
    assert(!game_menu_hold_update(&menu, true, false, 1600));
    assert(!game_menu_hold_update(&menu, false, false, 1700));
    assert(!game_menu_hold_update(&menu, true, true, 1800));
    assert(!game_menu_hold_update(&menu, true, true, 3000));
    assert(!game_menu_hold_update(&menu, true, false, 3100));
    assert(game_menu_hold_update(&menu, true, false, 3800));

    // A Wi-Fi receive can happen after the UI samples its time but before it
    // locks the snapshot. A newer packet must not appear disconnected.
    game::RemoteInput concurrent;
    uint8_t newer[] = {'G', 'P', 1, 0, 8, 0, 1};
    assert(concurrent.receive(newer, sizeof(newer), 150));
    assert(concurrent.read(140).connected);
    assert(concurrent.read(140).buttons == 8);
    assert(!concurrent.read(450).connected);
    game::RemoteInput wrapped;
    assert(wrapped.receive(newer, sizeof(newer), UINT32_MAX - 20));
    assert(wrapped.read(10).buttons == 8);
    assert(!wrapped.read(300).connected);
    game::ConnectionPause connection;
    assert(!connection.update(false, 10)); // no controller yet
    assert(!connection.update(true, 20));
    assert(!connection.update(false, 400)); // release keys, no menu for jitter
    assert(!connection.update(false, 1000));
    assert(!connection.update(true, 1100)); // recovery cancels pending pause
    assert(!connection.update(false, 1300));
    assert(connection.update(false, 2800)); // sustained loss still pauses
    assert(!connection.update(false, 2900)); // only once per loss
    assert(!connection.update(true, 3000));
    assert(!connection.update(false, 3100));
    assert(connection.update(false, 4600));
    game::RemoteInput input;
    uint8_t held[] = {'G', 'P', 1, 1, 0x81, 0, 255};
    assert(input.receive(held, sizeof(held), 100));
    assert(input.read(110).buttons == 0x81);
    uint8_t released[] = {'G', 'P', 1, 1, 0, 0, 0};
    assert(input.receive(released, sizeof(released), 120)); // sequence wrap
    assert(input.read(125).buttons == 0);
    assert(!input.receive(held, sizeof(held), 130)); // stale report cannot restore held key
    released[6] = 1; released[4] = 2;
    assert(input.receive(released, sizeof(released), 140));
    assert(!input.read(500).connected && input.read(500).buttons == 0);
    held[3] = 2;
    assert(!input.receive(held, sizeof(held), 510)); // another target
    held[3] = 1;
    assert(input.receive(held, sizeof(held), 520)); // new session after link expiry
    assert(!input.receive(held, 6, 530));
    assert(input.read(535).buttons == 0x81);

    game::RemoteInput legacy;
    uint8_t pose[] = {1, 0, 5, 194, 1, 88, 2, 0}; // yaw +1280 => left, pitch450
    assert(legacy.receive(pose, sizeof(pose), 100));
    assert(legacy.read(101).legacy && legacy.read(101).buttons == 64);
    pose[7] = 1;
    assert(legacy.receive(pose, sizeof(pose), 120));
    assert(legacy.read(130).buttons == 65);
    assert(legacy.read(220).buttons == 64); // toggle is a pulse, never a stuck A
    assert(legacy.read(1000).buttons == 0);
}
