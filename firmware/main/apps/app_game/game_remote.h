#pragma once
#include <cstddef>
#include <cstdint>

namespace game {
class ConnectionPause {
public:
    bool update(bool connected, uint32_t now)
    {
        if (connected) { seen_ = true; losing_ = false; fired_ = false; return false; }
        if (!seen_) return false;
        if (!losing_) { losing_ = true; lost_at_ = now; }
        if (!fired_ && now - lost_at_ >= 1500) { fired_ = true; return true; }
        return false;
    }
private:
    bool seen_ = false, losing_ = false, fired_ = false;
    uint32_t lost_at_ = 0;
};
struct RemoteSnapshot {
    bool connected = false;
    bool legacy = false;
    uint16_t buttons = 0;
};

// Pure decoder: the caller serializes access from Wi-Fi and UI tasks.
class RemoteInput {
public:
    bool receive(const uint8_t* data, size_t size, uint32_t now)
    {
        if (!data) return false;
        const bool connected = read(now).connected;
        if (size == 7 && data[0] == 'G' && data[1] == 'P' && data[2] == 1) {
            if (data[3] != 0 && data[3] != 1) return false;
            const auto delta = static_cast<uint8_t>(data[6] - sequence_);
            if (connected && !legacy_ && (delta == 0 || delta >= 128)) return false;
            sequence_ = data[6];
            buttons_ = (data[4] | (uint16_t(data[5]) << 8)) & 511;
            legacy_ = false;
        } else if (size == 8 && (data[0] == 0 || data[0] == 1)) {
            // Original factory remote: pose values and a toggled laser button.
            const auto yaw = static_cast<int16_t>(data[1] | (data[2] << 8));
            const auto pitch = static_cast<int16_t>(data[3] | (data[4] << 8));
            if (yaw < -1280 || yaw > 1280 || pitch < 0 || pitch > 900 || data[7] > 1) return false;
            buttons_ = yaw > 250 ? 64 : yaw < -250 ? 128 : 0;
            buttons_ |= pitch < 300 ? 16 : pitch > 600 ? 32 : 0;
            if (data[7] != toggle_ && (seen_ || data[7])) pulse_at_ = now;
            toggle_ = data[7];
            legacy_ = true;
        } else {
            return false;
        }
        seen_ = true;
        received_at_ = now;
        return true;
    }

    RemoteSnapshot read(uint32_t now) const
    {
        // The caller may have sampled its clock just before a Wi-Fi receive
        // updated the snapshot. A slightly newer packet is fresh, not expired.
        const auto age = static_cast<int32_t>(now - received_at_);
        const bool live = seen_ && age < (legacy_ ? 800 : 300);
        uint16_t result = live ? buttons_ : 0;
        if (live && legacy_ && pulse_at_ && static_cast<int32_t>(now - pulse_at_) < 80) result |= 1;
        return {live, legacy_, result};
    }

private:
    bool seen_ = false, legacy_ = false;
    uint8_t sequence_ = 0, toggle_ = 0;
    uint16_t buttons_ = 0;
    uint32_t received_at_ = 0, pulse_at_ = 0;
};
}
