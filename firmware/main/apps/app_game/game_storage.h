#pragma once
#include "game_runtime.h"
namespace game {
// Caller owns returned heap_caps allocation. Only the built-in ROM and accepted library paths may load.
uint8_t* loadRom(const std::string& path, size_t& size, std::string& error);
}  // namespace game
