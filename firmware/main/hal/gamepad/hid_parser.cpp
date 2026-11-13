#include "hid_parser.h"
#include <algorithm>
namespace gamepad {
// CodexPad official codex_pad_bleak_lib protocol: FFA0/FFA1, <IBBBB.
// Implemented from the published wire format, without vendor library code.
bool decodeCodexPad(const uint8_t* p, size_t len, uint16_t& result)
{
    if (!p || len != 8) return false;
    uint32_t bits = uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
    uint16_t out  = 0;
    static constexpr uint16_t mapping[] = {Up, Down, Left, Right, 0, 0, A, B, 0, 0, 0, 0, 0, 0, Select, Start, Menu};
    for (unsigned i = 0; i < 17; ++i)
        if (bits & (1u << i)) out |= mapping[i];
    if (p[4] < 64)
        out |= Left;
    else if (p[4] > 191)
        out |= Right;
    if (p[5] < 64)
        out |= Up;
    else if (p[5] > 191)
        out |= Down;
    result = out;
    return true;
}

bool HidParser::parseMap(const uint8_t* data, size_t len)
{
    fields_.clear();
    bits_.fill(0);
    clear();
    struct Global {
        uint32_t page = 0, size = 0, count = 0, id = 0;
        int32_t min = 0, max = 0;
    } g;
    std::vector<Global> stack;
    std::vector<uint32_t> usages;
    uint32_t first = 0, last = 0;
    bool range     = false;
    unsigned depth = 0;
    std::vector<bool> gameCollections;
    auto fail = [&]() {
        fields_.clear();
        bits_.fill(0);
        return false;
    };
    if (!data || !len || len > 4096) return fail();
    for (size_t pos = 0; pos < len;) {
        uint8_t prefix = data[pos++];
        if (prefix == 0xfe) return fail();
        unsigned n = prefix & 3;
        if (n == 3) n = 4;
        if (n > len - pos) return fail();
        uint32_t v = 0;
        for (unsigned j = 0; j < n; ++j) v |= uint32_t(data[pos++]) << (8 * j);
        int32_t sv = static_cast<int32_t>(v);
        if (n && n < 4 && (v & (1u << (8 * n - 1)))) sv = static_cast<int32_t>(v | (~0u << (8 * n)));
        unsigned type = (prefix >> 2) & 3, tag = prefix >> 4;
        if (type == 1) {
            switch (tag) {
                case 0:
                    g.page = v;
                    break;
                case 1:
                    g.min = sv;
                    break;
                case 2:
                    g.max = g.min < 0 ? sv : static_cast<int32_t>(v);
                    break;
                case 7:
                    g.size = v;
                    break;
                case 8:
                    if (!v || v > 255) return fail();
                    g.id = v;
                    break;
                case 9:
                    g.count = v;
                    break;
                case 10:
                    if (stack.size() >= 8) return fail();
                    stack.push_back(g);
                    break;
                case 11:
                    if (stack.empty()) return fail();
                    g = stack.back();
                    stack.pop_back();
                    break;
            }
        } else if (type == 2) {
            if (tag == 0) {
                if (usages.size() >= 128) return fail();
                usages.push_back(v);
            } else if (tag == 1) {
                first = v;
                range = true;
            } else if (tag == 2)
                last = v;
        } else if (type == 0) {
            if (tag == 10) {
                if (++depth > 16) return fail();
                bool game = !gameCollections.empty() && gameCollections.back();
                if (v == 1) {
                    uint32_t usage = usages.empty() ? 0 : usages.back();
                    game           = g.page == 1 && (usage == 4 || usage == 5);
                }
                gameCollections.push_back(game);
            } else if (tag == 12) {
                if (!depth) return fail();
                --depth;
                gameCollections.pop_back();
            } else if (tag == 8) {
                if (!g.size || g.size > 32 || !g.count || g.count > 1024 || g.size * g.count > 4096 ||
                    bits_[g.id] + g.size * g.count > 4096)
                    return fail();
                for (uint32_t i = 0; i < g.count; ++i) {
                    uint32_t u =
                        i < usages.size() ? usages[i] : (range && last >= first ? std::min(first + i, last) : 0);
                    uint32_t page = u > 0xffff ? u >> 16 : g.page;
                    u &= 0xffff;
                    if (!gameCollections.empty() && gameCollections.back() && !(v & 1) && (v & 2) && !(v & 4) &&
                        ((page == 9 && u <= 32) || (page == 1 && (u == 0x30 || u == 0x31 || u == 0x39)))) {
                        if (fields_.size() >= 128 || g.min >= g.max) return fail();
                        fields_.push_back({static_cast<uint16_t>(bits_[g.id] + i * g.size), static_cast<uint16_t>(page),
                                           static_cast<uint16_t>(u), static_cast<uint8_t>(g.size),
                                           static_cast<uint8_t>(g.id), g.min, g.max});
                    }
                }
                bits_[g.id] += g.size * g.count;
            }
            usages.clear();
            range = false;
            first = last = 0;
        }
    }
    if (depth || !stack.empty() || fields_.empty()) return fail();
    return true;
}
bool HidParser::input(uint8_t id, const uint8_t* data, size_t len)
{
    if (!data || !hasReport(id) || len < (bits_[id] + 7u) / 8u) return false;
    uint16_t out = 0;
    for (const auto& f : fields_) {
        if (f.id != id) continue;
        uint32_t raw = 0;
        for (unsigned i = 0; i < f.size; ++i)
            raw |= uint32_t((data[(f.offset + i) / 8] >> ((f.offset + i) % 8)) & 1) << i;
        int64_t value = raw;
        if (f.min < 0 && (raw & (1u << (f.size - 1)))) value = static_cast<int64_t>(raw) - (int64_t(1) << f.size);
        if (f.page == 9 && value) {
            switch (f.usage) {
                case 1:
                    out |= A;
                    break;
                case 2:
                    out |= B;
                    break;
                case 9:
                    out |= Select;
                    break;
                case 10:
                    out |= Start;
                    break;
                case 11:
                    out |= Menu;
                    break;
            }
        } else if (f.page == 1 && f.usage == 0x39) {
            int64_t h = value - f.min;
            if (h >= 0 && h < 8) {
                static const uint16_t hats[] = {Up,   Up | Right,  Right, Down | Right,
                                                Down, Down | Left, Left,  Up | Left};
                out |= hats[h];
            }
        } else if (f.page == 1 && value >= f.min && value <= f.max) {
            int64_t span = int64_t(f.max) - f.min;
            if (value < f.min + span / 4)
                out |= f.usage == 0x30 ? Left : Up;
            else if (value > f.max - span / 4)
                out |= f.usage == 0x30 ? Right : Down;
        }
    }
    states_[id] = out;
    return true;
}
}  // namespace gamepad
