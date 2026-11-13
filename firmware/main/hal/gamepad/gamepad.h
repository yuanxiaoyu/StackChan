#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace gamepad {
enum Button : uint16_t { A = 1, B = 2, Select = 4, Start = 8, Up = 16, Down = 32, Left = 64, Right = 128, Menu = 256 };
struct Device {
    std::string id, name;
};
struct Snapshot {
    bool connected = false, scanning = false, connecting = false;
    uint16_t buttons = 0;
    std::string name, status;
};
void initialize();
void scan();
void cancel_scan();
std::vector<Device> devices();
bool connect(const std::string& id);
void forget();
Snapshot snapshot();
}  // namespace gamepad
