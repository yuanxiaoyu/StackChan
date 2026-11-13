#include "game_storage.h"
#include "builtin_rom.h"
#include "rom_info.h"
#include "hal/board/game_board.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <mutex>
#include <esp_log.h>
#include <sys/stat.h>
#include <driver/sdspi_host.h>
#include <esp_vfs_fat.h>
#include <esp_heap_caps.h>
namespace game {
namespace {
std::mutex storageMutex;
sdmmc_card_t* card = nullptr;
const char* dirs[] = {"/room", "/rom", "/roms/nes", "/roms", "/ROM"};
struct Access {
    bool ok;
    Access() : ok(game_board::storageBegin())
    {
    }
    ~Access()
    {
        if (ok) game_board::storageEnd();
    }
};
bool mount(std::string& error)
{
    if (card) return true;
    sdmmc_host_t host                    = SDSPI_HOST_DEFAULT();
    host.slot                            = SPI3_HOST;
    host.max_freq_khz                    = 4000;
    sdspi_device_config_t slot           = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.host_id                         = SPI3_HOST;
    slot.gpio_cs                         = GPIO_NUM_4;
    esp_vfs_fat_sdmmc_mount_config_t cfg = {};
    cfg.format_if_mount_failed           = false;
    cfg.max_files                        = 4;
    cfg.allocation_unit_size             = 16384;
    auto e                               = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot, &cfg, &card);
    if (e != ESP_OK) {
        card  = nullptr;
        error = "SD card unavailable (" + std::to_string(e) + ")";
        return false;
    }
    ESP_LOGI("NES-SD", "SD mounted at 4 MHz, capacity %llu MiB",
             (unsigned long long)card->csd.capacity * card->csd.sector_size / (1024 * 1024));
    return true;
}
bool allowed(const std::string& path)
{
    if (!isNesPath(path) || path.find("..") != std::string::npos) return false;
    for (auto d : dirs) {
        std::string prefix = std::string("/sdcard") + d + "/";
        if (path.rfind(prefix, 0) == 0 && path.find('/', prefix.size()) == std::string::npos) return true;
    }
    return false;
}
}  // namespace
Library scanRoms()
{
    Library result;
    const auto builtin = builtinRom();
    if (builtin.data) {
        const auto info = inspectRom(builtin.data, builtin.size, builtin.size);
        if (!info.valid) {
            result.status = "Built-in ROM: " + info.error;
            return result;
        }
        result.games.push_back({"Jackal / 赤色要塞", builtinRomPath, builtin.size});
        result.status = "Built-in game / SD scan disabled";
        return result;
    }
    std::lock_guard<std::mutex> lock(storageMutex);
    Access access;
    if (!access.ok) {
        result.status = "Display/SD bus busy";
        return result;
    }
    if (!mount(result.status)) return result;
    unsigned rejected = 0;
    for (auto dir : dirs) {
        std::string root = std::string("/sdcard") + dir;
        DIR* dp          = opendir(root.c_str());
        if (!dp) continue;
        while (auto* item = readdir(dp)) {
            std::string path = root + "/" + item->d_name;
            if (!allowed(path)) continue;
            struct stat st{};
            if (stat(path.c_str(), &st) || !S_ISREG(st.st_mode)) continue;
            uint8_t h[16];
            FILE* f = fopen(path.c_str(), "rb");
            if (!f) {
                ++rejected;
                continue;
            }
            size_t n = fread(h, 1, 16, f);
            fclose(f);
            auto info = inspectRom(h, n, st.st_size);
            if (!info.valid) {
                ++rejected;
                continue;
            }
            if (result.games.size() < 8)
                ESP_LOGI("NES-SD", "ROM mapper %u, bytes %u", info.mapper, unsigned(st.st_size));
            if (result.games.size() < 128) result.games.push_back({item->d_name, path, size_t(st.st_size)});
        }
        closedir(dp);
    }
    ESP_LOGI("NES-SD", "ROM scan: %u accepted, %u rejected", unsigned(result.games.size()), rejected);
    std::sort(result.games.begin(), result.games.end(),
              [](const RomEntry& a, const RomEntry& b) { return a.name < b.name; });
    result.status = result.games.empty() ? "No supported .nes ROMs in /room or /roms" : "Select a ROM";
    if (rejected) result.status += " (" + std::to_string(rejected) + " unsupported/invalid)";
    return result;
}
uint8_t* loadRom(const std::string& path, size_t& size, std::string& error)
{
    size = 0;
    if (path == builtinRomPath) {
        const auto builtin = builtinRom();
        const auto info = inspectRom(builtin.data, builtin.size, builtin.size);
        if (!info.valid) {
            error = "Built-in ROM: " + info.error;
            return nullptr;
        }
        auto* data = static_cast<uint8_t*>(heap_caps_malloc(builtin.size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!data) {
            error = "Not enough PSRAM for ROM";
            return nullptr;
        }
        std::memcpy(data, builtin.data, builtin.size);
        size = builtin.size;
        ESP_LOGI("NES", "Loaded built-in Jackal, %u bytes", unsigned(size));
        return data;
    }
    if (!allowed(path)) {
        error = "Invalid ROM path";
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(storageMutex);
    Access access;
    if (!access.ok) {
        error = "Display/SD bus busy";
        return nullptr;
    }
    if (!mount(error)) return nullptr;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        error = "Cannot open ROM";
        return nullptr;
    }
    struct stat st{};
    uint8_t h[16];
    size_t n = fread(h, 1, 16, f);
    if (fstat(fileno(f), &st)) {
        fclose(f);
        error = "Cannot read ROM size";
        return nullptr;
    }
    auto info = inspectRom(h, n, st.st_size);
    if (!info.valid) {
        fclose(f);
        error = info.error;
        return nullptr;
    }
    size       = st.st_size;
    auto* data = static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!data) {
        fclose(f);
        error = "Not enough PSRAM for ROM";
        return nullptr;
    }
    rewind(f);
    n = fread(data, 1, size, f);
    fclose(f);
    if (n != size) {
        heap_caps_free(data);
        error = "SD read failed";
        size  = 0;
        return nullptr;
    }
    return data;
}
}  // namespace game
