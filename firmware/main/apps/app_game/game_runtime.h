#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
namespace game {
struct RomEntry {
    std::string name, path;
    size_t size;
};
struct Library {
    std::vector<RomEntry> games;
    std::string status;
};
Library scanRoms();
struct RuntimeStatus {
    bool running = false, paused = false, finished = false;
    std::string error;
    uint32_t frames = 0;
};
bool start(const std::string& path);
void stop();
void pause(bool paused);
void setButtons(uint16_t buttons);
// Configure only while stopped. The worker samples the reader each NES frame;
// setButtons supplies additional synthetic UI pulses when a reader is set.
void setInputReader(std::function<uint16_t()> reader);
// Exchange the caller's full-size RGB565 buffer for the latest completed frame.
// Call only between LVGL renders; the returned buffer belongs to the caller
// until the next exchange. The worker may immediately reuse the previous one.
bool takeFrame(uint16_t*& rgb565);
RuntimeStatus runtimeStatus();
}  // namespace game
