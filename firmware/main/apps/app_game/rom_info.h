#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
namespace game {
constexpr size_t maxRomSize = 1024 * 1024;
struct RomInfo {
    bool valid      = false;
    unsigned mapper = 0;
    size_t required = 0;
    std::string error;
};
bool isNesPath(const std::string& path);
RomInfo inspectRom(const uint8_t* header, size_t headerSize, size_t fileSize);
}  // namespace game
