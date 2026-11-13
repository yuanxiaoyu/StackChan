// Self-generated minimal mapper-0 program; no commercial ROM or ROM file.
#undef calloc
#include "nofrendo.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
// The host target redirects calloc to exercise the APU's allocation failure.
#undef calloc
static bool fail_next_allocation;
void *nofrendo_test_calloc(size_t count, size_t size) {
    if (fail_next_allocation) { fail_next_allocation = false; return NULL; }
    return calloc(count, size);
}
int main(void) {
    fail_next_allocation = true;
    assert(apu_init(24000, false) == NULL);
    apu_shutdown();
    for (int cycle = 0; cycle < 3; ++cycle) {
        const size_t trainer_size = cycle == 1 ? 512 : 0;
        const size_t offset = 16 + trainer_size;
        const size_t size = offset + 16384 + 8192;
        uint8_t *data = calloc(1, size);
        uint8_t *video = calloc(1, NES_SCREEN_PITCH * NES_SCREEN_HEIGHT);
        assert(data && video);
        memcpy(data, "NES\x1a", 4);
        data[4] = data[5] = 1;
        if (trainer_size) {
            data[6] = 4;
            for (size_t i = 0; i < trainer_size; ++i) data[16+i] = (uint8_t)(i ^ 0xa5);
        }
        // Poll controller 1 through the actual 6502 $4016 bus into RAM.
        // This exercises the path a game uses, including strobe and bit order.
        const uint8_t poll_controller[] = {
            0xa9, 1, 0x8d, 0x16, 0x40, 0xa9, 0, 0x8d, 0x16, 0x40,
            0xa2, 0, 0xad, 0x16, 0x40, 0x29, 1, 0x9d, 0, 2,
            0xe8, 0xe0, 8, 0xd0, 0xf3, 0x4c, 0, 0x80,
        };
        memcpy(data + offset, poll_controller, sizeof(poll_controller));
        data[offset + 16384 - 3] = 0x80;
        assert(nes_init(SYS_NES_NTSC, 24000, false, NULL));
        assert(nes_insertcart(rom_loadmem(data, size)) == 0);
        if (trainer_size) {
            assert(memcmp(nes_getptr()->cart->prg_ram + 0x1000, data+16, 512) == 0);
            memset(nes_getptr()->cart->prg_ram + 0x1000, 0x42, 512);
            nes_reset(false);
            assert(nes_getptr()->cart->prg_ram[0x1000] == 0x42);
            nes_reset(true);
            assert(memcmp(nes_getptr()->cart->prg_ram + 0x1000, data+16, 512) == 0);
        }
        nes_setvidbuf(video);
        input_connect(0, NES_JOYPAD);
        assert(nes_getptr()->apu->buffer);
        assert(nes_getptr()->apu->samples_per_frame == 400);
        for (int frame = 0; frame < 120; ++frame) {
            const uint8_t mask = frame % 9 == 8 ? 0 : 1 << (frame % 9);
            input_update(0, mask);
            nes_emulate(frame % 2 == 0);
            for (int bit = 0; bit < 8; ++bit)
                assert(nes_getptr()->mem->ram[0x200 + bit] == ((mask >> bit) & 1));
            // Read all output samples, as the production audio consumer does.
            volatile int16_t sample;
            for (int i = 0; i < 400; ++i) sample = nes_getptr()->apu->buffer[i];
            (void)sample;
        }
        nes_shutdown();
        free(data);
        free(video);
    }
    return 0;
}
