#pragma once

#ifdef RETRO_GO
#include <rg_system.h>
#define LOG_PRINTF(level, x...) rg_system_log(RG_LOG_PRINTF, NULL, x)
#define CRC32(a, b, c) rg_crc32(a, b, c)
#else
#include <stdio.h>
#define LOG_PRINTF(level, x...) printf(x)
#ifdef ESP_PLATFORM
#include <esp_attr.h>
#elif !defined(IRAM_ATTR)
#define IRAM_ATTR
#endif
static inline unsigned nofrendo_crc32(unsigned crc, const void *data, size_t size) {
    const unsigned char *p = (const unsigned char *)data;
    crc = ~crc;
    while (size--) { crc ^= *p++; for (int n=0; n<8; ++n) crc = (crc >> 1) ^ (0xedb88320u & (0u-(crc&1))); }
    return ~crc;
}
#define CRC32(a, b, c) nofrendo_crc32(a, b, c)
#endif

#define MESSAGE_ERROR(x...) LOG_PRINTF(1, "!! " x)
#define MESSAGE_WARN(x...)  LOG_PRINTF(2, " ! " x)
#define MESSAGE_INFO(x...)  LOG_PRINTF(3, x)
#ifdef NOFRENDO_DEBUG
#define MESSAGE_DEBUG(x, ...) LOG_PRINTF(4, "> %s: " x, __func__, ## __VA_ARGS__)
#else
#define MESSAGE_DEBUG(x...)
#endif
#define MESSAGE_TRACE(x, ...) LOG_PRINTF(4, "~ %s: " x, __func__, ## __VA_ARGS__)

#undef MIN
#define MIN(a, b) ({__typeof__(a) _a = (a); __typeof__(b) _b = (b);_a < _b ? _a : _b; })
#undef MAX
#define MAX(a, b) ({__typeof__(a) _a = (a); __typeof__(b) _b = (b);_a > _b ? _a : _b; })

#define ASSERT(expr) while (!(expr)) { LOG_PRINTF(1, "ASSERTION FAILED IN %s: " #expr "\n", __func__); abort(); }
#define UNUSED(x) (void)x
