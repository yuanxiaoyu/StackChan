#include "game_runtime.h"
#include "game_storage.h"
#include "game_video.h"
#include <atomic>
#include <mutex>
#include <cstring>
#include <utility>
#include <algorithm>
#include <cmath>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/idf_additions.h>
#include <board.h>
#include <audio/audio_codec.h>
extern "C" {
#include <nofrendo.h>
}
namespace game {
namespace {
std::mutex stateMutex;
RuntimeStatus state;
std::atomic<bool> active{false}, quit{false}, paused{false};
std::atomic<uint16_t> buttons{0};
std::function<uint16_t()> inputReader;
std::string selectedPath;
uint16_t* frames[2] = {};
int front           = 0;
bool fresh          = false;
void fail(const std::string& error)
{
    std::lock_guard<std::mutex> lock(stateMutex);
    state.error = error;
}
void worker(void*)
{
    size_t size = 0;
    std::string error;
    uint8_t* data     = loadRom(selectedPath, size, error);
    uint8_t* video    = nullptr;
    uint16_t* palette = nullptr;
    bool initialized  = false;
    AudioCodec* codec = nullptr;
    bool wasOutput    = false;
    if (!data) {
        fail(error);
        goto cleanup;
    }
    if (quit.load()) goto cleanup;
    frames[0] = static_cast<uint16_t*>(heap_caps_malloc(FramePixels * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    frames[1] = static_cast<uint16_t*>(heap_caps_malloc(FramePixels * 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    video     = static_cast<uint8_t*>(heap_caps_malloc(NES_SCREEN_PITCH * 240, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    palette   = static_cast<uint16_t*>(nofrendo_buildpalette(NES_PALETTE_NOFRENDO, 16));
    if (!frames[0] || !frames[1] || !video || !palette) {
        fail("Not enough memory for NES video");
        goto cleanup;
    }
    if (!nes_init(SYS_NES_NTSC, 24000, false, nullptr)) {
        fail("NES initialization failed");
        goto cleanup;
    }
    initialized = true;
    if (nes_insertcart(rom_loadmem(data, size)) < 0) {
        // nes_insertcart unwinds all core allocations on failure.
        initialized = false;
        fail("Cannot initialize cartridge mapper");
        goto cleanup;
    }
    nes_setvidbuf(video);
    input_connect(0, NES_JOYPAD);
    ESP_LOGI("NES", "Started mapper %d, ROM %u bytes", nes_getptr()->cart->mapper_number, unsigned(size));
    codec = Board::GetInstance().GetAudioCodec();
    if (codec) {
        wasOutput = codec->output_enabled();
        codec->EnableOutput(true);
    }
    ESP_LOGI("NES", "Game audio enabled");
    {
        std::vector<int16_t> pcm(400);
        TickType_t next       = xTaskGetTickCount();
        uint32_t count        = 0;
        int64_t reportTime    = esp_timer_get_time();
        uint32_t reportFrames = 0;
        int64_t emulateUs = 0, audioUs = 0, videoUs = 0, waitUs = 0;
        uint32_t drawn = 0;
        uint16_t lastInput = UINT16_MAX;
        uint32_t audioSamples = 0, sourceNonzero = 0, sourceClipped = 0, sinkNonzero = 0;
        int sourcePeak = 0;
        double sourceSquares = 0;
        while (!quit.load()) {
            if (paused.load()) {
                input_update(0, 0);
                vTaskDelay(pdMS_TO_TICKS(20));
                next = xTaskGetTickCount();
                reportTime = esp_timer_get_time();
                reportFrames = count;
                emulateUs = audioUs = videoUs = waitUs = 0;
                drawn = 0;
                audioSamples = sourceNonzero = sourceClipped = sinkNonzero = 0;
                sourcePeak = 0; sourceSquares = 0;
                continue;
            }
            const int64_t frameStart = esp_timer_get_time();
            const uint16_t currentInput = (buttons.load() | (inputReader ? inputReader() : 0)) & 255;
            input_update(0, currentInput);
            if (currentInput != lastInput) {
                ESP_LOGD("GAME-INPUT", "Emulator buttons=0x%02x", unsigned(currentInput));
                lastInput = currentInput;
            }
            bool draw = (count % 2) == 0;
            nes_emulate(draw);
            const int64_t emulateEnd = esp_timer_get_time();
            // nes_emulate already synthesizes one complete APU frame.
            auto* apu = nes_getptr()->apu;
            pcm.assign(apu->buffer, apu->buffer + apu->samples_per_frame);
            // Inspect generated and submitted PCM. Counters are
            // reported once per window, never stream samples over the UART.
            for (int i = 0; i < apu->samples_per_frame; ++i) {
                const int sample = apu->buffer[i];
                const int magnitude = sample < 0 ? -sample : sample;
                sourcePeak = std::max(sourcePeak, magnitude);
                sourceNonzero += sample != 0;
                sinkNonzero += codec && pcm[i] != 0;
                sourceClipped += sample == INT16_MIN || sample == INT16_MAX;
                sourceSquares += double(sample) * sample;
                ++audioSamples;
            }
            if (codec) codec->OutputData(pcm);
            const int64_t audioEnd = esp_timer_get_time();
            if (draw) {
                int back = 1 - front;
                convertFrame(frames[back], video + NES_SCREEN_OVERDRAW, NES_SCREEN_PITCH, palette);
                std::lock_guard<std::mutex> lock(stateMutex);
                front = back;
                fresh = true;
                ++drawn;
            }
            const int64_t videoEnd = esp_timer_get_time();
            emulateUs += emulateEnd - frameStart;
            audioUs += audioEnd - emulateEnd;
            videoUs += videoEnd - audioEnd;
            {
                std::lock_guard<std::mutex> lock(stateMutex);
                state.frames = ++count;
            }
            if (esp_timer_get_time() - reportTime >= 5000000) {
                int64_t now = esp_timer_get_time();
                const double n = count - reportFrames;
                ESP_LOGI("NES", "Frames %u, logic %.1f FPS, produced %.1f FPS; ms/frame core %.2f audio %.2f video %.2f wait %.2f",
                         unsigned(count), n * 1000000.0 / (now - reportTime),
                         drawn * 1000000.0 / (now - reportTime), emulateUs / (n * 1000),
                         audioUs / (n * 1000), videoUs / (n * 1000), waitUs / (n * 1000));
                ESP_LOGI("NES-AUDIO", "muted=0 source_hz=24000 sink_hz=%d enabled=%d volume=%d samples=%u nonzero=%u peak=%d rms=%.1f clipped=%u sink_nonzero=%u",
                         codec ? codec->output_sample_rate() : 0, codec && codec->output_enabled(),
                         codec ? codec->output_volume() : 0, unsigned(audioSamples), unsigned(sourceNonzero),
                         sourcePeak, audioSamples ? std::sqrt(sourceSquares / audioSamples) : 0.0,
                         unsigned(sourceClipped), unsigned(sinkNonzero));
                audioSamples = sourceNonzero = sourceClipped = sinkNonzero = 0;
                sourcePeak = 0; sourceSquares = 0;
                reportTime   = now;
                reportFrames = count;
                emulateUs = audioUs = videoUs = waitUs = 0;
                drawn = 0;
            }
            // Audio clocks at 24 kHz; fallback/overrun yielding also feeds the watchdog.
            vTaskDelayUntil(&next, pdMS_TO_TICKS(16));
            if (xTaskGetTickCount() - next > pdMS_TO_TICKS(100)) next = xTaskGetTickCount();
            waitUs += esp_timer_get_time() - videoEnd;
        }
    }
cleanup:
    if (codec && !wasOutput) codec->EnableOutput(false);
    if (initialized) nes_shutdown();
    heap_caps_free(data);
    heap_caps_free(video);
    free(palette);
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        heap_caps_free(frames[0]);
        heap_caps_free(frames[1]);
        frames[0] = frames[1] = nullptr;
        fresh                 = false;
        state.running         = false;
        state.paused          = false;
        state.finished        = true;
    }
    active.store(false);
    vTaskDeleteWithCaps(nullptr);
}
}  // namespace
bool start(const std::string& path)
{
    bool expected = false;
    if (!active.compare_exchange_strong(expected, true)) return false;
    quit         = false;
    paused       = false;
    buttons      = 0;
    selectedPath = path;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        state         = {};
        state.running = true;
        fresh         = false;
        front         = 0;
    }
    // The emulator does not run with the flash cache disabled. Its large stack
    // belongs in PSRAM, leaving internal SRAM for I2S DMA and wireless drivers.
    // LVGL runs on core 1; emulation and framebuffer conversion run on core 0.
    if (xTaskCreatePinnedToCoreWithCaps(worker, "nes-runtime", 12288, nullptr, 3, nullptr, 0,
                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        active = false;
        std::lock_guard<std::mutex> lock(stateMutex);
        state.running  = false;
        state.finished = true;
        state.error    = "Not enough memory for NES task";
        ESP_LOGE("NES", "Task allocation failed: internal free %u largest %u, PSRAM free %u",
                 unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                 unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
                 unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)));
        return false;
    }
    return true;
}
void stop()
{
    quit    = true;
    buttons = 0;
    // Never delete the worker externally: SD/I2S and core cleanup must unwind safely.
    const int64_t deadline = esp_timer_get_time() + 3000000;
    while (active.load() && esp_timer_get_time() < deadline) vTaskDelay(pdMS_TO_TICKS(10));
    if (active.load()) fail("NES is still stopping; please wait");
}
void pause(bool value)
{
    paused = value;
    if (value) buttons = 0;
    std::lock_guard<std::mutex> lock(stateMutex);
    state.paused = value && state.running;
}
void setButtons(uint16_t mask)
{
    buttons = mask & 255;
}
void setInputReader(std::function<uint16_t()> reader)
{
    // Ownership changes only before starting or after joining the worker.
    if (!active.load()) inputReader = std::move(reader);
}
bool takeFrame(uint16_t*& dest)
{
    if (!dest) return false;
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!fresh || !frames[front]) return false;
    // The producer writes only 1-front and changes front under this mutex.
    // Swapping the completed slot cannot touch its in-progress back buffer.
    std::swap(dest, frames[front]);
    fresh = false;
    return true;
}
RuntimeStatus runtimeStatus()
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return state;
}
}  // namespace game
