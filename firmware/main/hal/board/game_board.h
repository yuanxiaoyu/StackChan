#pragma once

namespace game_board {
// Acquires the LVGL lock, drains LCD DMA and releases shared GPIO35 for SD MISO.
// Call from a worker, never while holding the UI lock. Pair every true with End.
bool storageBegin();
void storageEnd();
}
