#pragma once
#include "gamepad.h"
#include <array>
#include <cstddef>
namespace gamepad {
bool decodeCodexPad(const uint8_t*, size_t, uint16_t&);
// Bounded parser for variable absolute gamepad fields. HOGP reports omit ID bytes:
// the Report Reference descriptor supplies the ID to input().
class HidParser {
    struct Field {
        uint16_t offset, page, usage;
        uint8_t size, id;
        int32_t min, max;
    };
    std::vector<Field> fields_;
    std::array<uint16_t, 256> bits_{};
    std::array<uint16_t, 256> states_{};

public:
    bool parseMap(const uint8_t*, size_t);
    bool input(uint8_t, const uint8_t*, size_t);
    uint16_t buttons() const
    {
        uint16_t b = 0;
        for (auto s : states_) b |= s;
        return b;
    }
    void clear()
    {
        states_.fill(0);
    }
    bool hasReport(uint8_t id) const
    {
        for (auto f : fields_)
            if (f.id == id) return true;
        return false;
    }
};
}  // namespace gamepad
