# StackChan-RemoteControl-ESPNow

This repository is designed to remotely control StackChan servo movements via the `ESPNow` protocol.

## Applicable Devices

* StickC-Plus + Hat Mini JoyC

## Compilation Environment

* IDF：ESP-IDF v5.4.2
* Device Type：esp32

## Compilation and Flashing

1. Before compilation, globally search the project for `__has_include(<driver/i2c_master.h>)` in `M5GFX` and replace all occurrences with `0`.
2. When flashing, specify the baud rate as `1500000`.

## Package Firmware

```
esptool.py --chip esp32 merge_bin -o StackChan-RemoteControl-ESPNow-jyy-20251231_0x0.bin 0x1000 build\bootloader\bootloader.bin 0x8000 build\partition_table\partition-table.bin 0x10000 build\StackChan-RemoteControl-ESPNow.bin
```

## FC game controller mode

The original servo/setup/IMU modes remain available. In setup, keep the stick
released and hold **BtnB for 1 second** to enter GAME. The neutral stick position
is calibrated on entry (300 raw units of deadzone). Match the host's ESP-NOW
channel and receiver ID in setup first; channel remains 1 by default, and ID 0
is broadcast (host default receiver ID is 1).

* Hold horizontally with the Mini JoyC on the **left**; GAME rotates the
  portrait stick axes 90 degrees counterclockwise. Stick: NES D-pad
  (diagonals supported). Setup and servo modes keep their original axes.
* BtnA / BtnB: held NES A / B.
* Mini JoyC center press: Start immediately while pressed; release to release
  Start. Holding it continues Start. Hold **A alone for 700 ms** to open the
  host menu (or tap the host touchscreen). A short press still fires.
* Select: choose Select in the host pause menu.
* Hold BtnA + BtnB for 2 seconds to leave GAME; a released packet is sent first.

GAME's main loop targets a full state every 20 ms, including unchanged/released
states. **Current hardware diagnostic build:** each register write and read is
followed by a 20 ms idle interval to prevent the HAT from locking SCL. Three
registers therefore take about 120 ms per sample; this build does not achieve
a 20 ms heartbeat and still has intermittent read timeouts. The user has
verified horizontal direction mapping, center Start, in-game pause/resume, and
the long-A host menu. A/B shooting and long-term wireless stability remain to
be verified.
Start is transmitted as a held button, like A/B. The payload is exactly seven bytes:
`47 50 01 target buttons-low buttons-high sequence`. Button bits are A=1,
B=2, Select=4, Start=8, Up=16, Down=32, Left=64, Right=128, Menu=256.
This distinct length prevents the original eight-byte motion parser from
interpreting game input as servo targets.

Center input is I2C address `0x54`, register `0x30`, active low. The official
[M5Unit-MiniJoyC firmware](https://github.com/m5stack/M5Unit-MiniJoyC-Internal-FW/blob/main/HAT_MiniJoyC/Core/Src/main.c)
returns the pull-up button GPIO directly for this register. I2C accesses from
setup, motion, and game readers share a mutex.
Each raw axis must be read separately: two bytes from `0x00` for X, then two
from `0x02` for Y. The HAT firmware supplies only two bytes per axis request;
a four-byte burst from `0x00` does not read both axes.
Register selection is a separate write transaction ending with STOP, followed
by a read transaction. The HAT's STM32 firmware prepares the response from its
listen-complete callback; a combined write/repeated-START/read can reach the
read before that response is ready. Setup, motion and GAME share this helper.
Each transaction and GAME mutex wait is bounded to 10 ms, stopping on the first
failure. This keeps missing/unresponsive HAT reads from blocking A/B heartbeat
packets beyond the host's 300 ms disconnect threshold. `GAME-REMOTE` logs report
failed register reads and SDA/SCL levels at most once every five seconds and
log button state changes. GAME reads the center register first; a successful
center reading is still used if a later axis read fails.

### Build validation

The game variant is compiled with ESP-IDF **v5.5.4**, ESP32 target, and the
locked M5Unified 0.2.17 / M5GFX 0.2.23 / esp-now 2.5.3 dependencies. The existing
M5GFX legacy-I2C workaround above is still required; do it in the downloaded
managed component only, before rebuilding. This avoids mixing M5GFX's new I2C
driver with the original remote's legacy `i2c_bus` driver. Do not commit the
managed component, sdkconfig migration, or build directory.
