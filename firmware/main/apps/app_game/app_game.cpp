#include "app_game.h"
#include "builtin_rom.h"
#include "game_video.h"
#include "game_menu.h"
#include <assets/assets.h>
#include <hal/hal.h>
#include <hal/gamepad/gamepad.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <board.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#include <src/misc/cache/instance/lv_image_cache.h>

LV_FONT_DECLARE(agent_tasks_cjk_14);

namespace {
lv_obj_t* label(lv_obj_t* parent, const char* text, int x, int y, int width)
{
    auto* value = lv_label_create(parent);
    lv_obj_set_style_text_font(value, &agent_tasks_cjk_14, 0);
    lv_obj_set_style_text_color(value, lv_color_hex(0xF4F3EC), 0);
    lv_obj_set_size(value, width, LV_SIZE_CONTENT);
    lv_obj_set_pos(value, x, y);
    lv_label_set_text(value, text);
    return value;
}
}

AppGame::AppGame()
{
    setAppInfo().name = "GAME";
    static auto icon = assets::get_image("icon_game.bin");
    setAppInfo().icon = &icon;
    static uint32_t color = 0xE5A34A;
    setAppInfo().userData = &color;
}

lv_obj_t* AppGame::button(const char* text, int x, int y, int width, Action action, size_t index)
{
    auto* value = lv_button_create(root_);
    lv_obj_set_pos(value, x, y);
    lv_obj_set_size(value, width, 32);
    lv_obj_set_style_radius(value, 9, 0);
    lv_obj_set_style_bg_color(value, lv_color_hex(0x3B4450), 0);
    lv_obj_set_style_shadow_width(value, 0, 0);
    auto* text_label = label(value, text, 0, 0, width - 12);
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_center(text_label);
    bindings_.push_back(std::make_unique<Binding>(Binding{this, action, index}));
    lv_obj_add_event_cb(value, [](lv_event_t* e) {
        const auto* binding = static_cast<Binding*>(lv_event_get_user_data(e));
        if (binding->action == Action::Launch) binding->app->launch_index_ = binding->index;
        binding->app->action_ = binding->action;
    }, LV_EVENT_CLICKED, bindings_.back().get());
    return value;
}

void AppGame::draw(Screen screen)
{
    // Called only on the Mooncake task, with the LVGL lock held.
    if (root_) lv_obj_delete(root_);
    bindings_.clear(); rows_.fill(nullptr); pause_buttons_.fill(nullptr); image_ = info_ = nullptr;
    screen_ = screen;
    root_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(root_, 320, 240);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_bg_color(root_, lv_color_hex(0x20252B), 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    if (screen == Screen::Playing) {
        image_ = lv_image_create(root_);
        lv_image_set_src(image_, &frame_desc_);
        lv_obj_set_pos(image_, 0, 0);
        // Keep all 320x240 pixels for video; tapping opens the existing menu.
        lv_obj_add_event_cb(root_, [](lv_event_t* e) {
            static_cast<AppGame*>(lv_event_get_user_data(e))->action_ = Action::Pause;
        }, LV_EVENT_CLICKED, this);
        return;
    }
    label(root_, "GAME / FC", 12, 7, 230);
    if (screen == Screen::Library) {
        button("Home", 252, 2, 64, Action::Exit);
        info_ = label(root_, message_.c_str(), 12, 30, 296);
        lv_obj_set_height(info_, 34);
        lv_label_set_long_mode(info_, LV_LABEL_LONG_MODE_DOTS);
        drawLibraryRows();
        button("<", 8, 200, 36, Action::Previous);
        button(">", 48, 200, 36, Action::Next);
        button("Refresh", 90, 200, 96, Action::Refresh);
        button(radio_started_ ? "Remote:1" : "Remote", 192, 200, 120, Action::Remote);
    } else if (screen == Screen::Paused) {
        info_ = label(root_, message_.empty() ? "Paused" : message_.c_str(), 12, 34, 296);
        pause_buttons_[0] = button("Resume", 12, 77, 142, Action::Resume);
        pause_buttons_[1] = button("Games", 166, 77, 142, Action::Library);
        pause_buttons_[2] = button("Start", 12, 121, 142, Action::Start);
        pause_buttons_[3] = button("Select", 166, 121, 142, Action::Select);
        pause_buttons_[4] = button("Exit Game", 12, 172, 296, Action::Exit);
        lv_obj_set_style_bg_color(pause_buttons_[pause_selected_], lv_color_hex(0x99642A), 0);
        label(root_, "Stick: choose  A: OK  B: games", 12, 213, 296);
    } else if (screen == Screen::Error) {
        label(root_, message_.c_str(), 12, 47, 296);
        button("Games", 12, 185, 142, Action::Library);
        button("Home", 166, 185, 142, Action::Exit);
    } else {
        label(root_, message_.c_str(), 12, 61, 296);
        button("Home", 12, 185, 296, Action::Exit);
    }
}

void AppGame::drawLibraryRows()
{
    const size_t first = (selected_ / rows_.size()) * rows_.size();
    for (size_t row = 0; row < rows_.size(); ++row) {
        const size_t index = first + row;
        if (index >= library_.games.size()) break;
        std::string name = std::to_string(index + 1) + ". " + library_.games[index].name;
        rows_[row] = button(name.c_str(), 8, 65 + row * 33, 304, Action::Launch, index);
        lv_obj_set_height(rows_[row], 30);
        if (index == selected_) lv_obj_set_style_bg_color(rows_[row], lv_color_hex(0x99642A), 0);
    }
}

void AppGame::beginScan()
{
    if (scan_ && !scan_->done.load()) return;
    if (game::builtinRom().data) {
        library_ = game::scanRoms();
        scan_.reset();
        selected_ = 0;
        message_ = library_.status;
        LvglLockGuard lock;
        draw(Screen::Library);
        return;
    }
    scan_ = std::make_shared<ScanResult>();
    auto* arg = new std::shared_ptr<ScanResult>(scan_);
    if (xTaskCreate([](void* p) {
        std::unique_ptr<std::shared_ptr<ScanResult>> result(static_cast<std::shared_ptr<ScanResult>*>(p));
        (*result)->library = game::scanRoms();
        (*result)->done.store(true);
        result.reset();
        vTaskDelete(nullptr);
    }, "game-sd-scan", 6144, arg, 2, nullptr) != pdPASS) {
        delete arg;
        scan_->library.status = "Cannot create SD task";
        scan_->done = true;
    }
    message_ = "Reading SD card /room ...";
    LvglLockGuard lock;
    draw(Screen::Loading);
}

void AppGame::enableRemote()
{
    if (radio_started_) return;
    radio_requested_ = false;
    radio_started_ = GetHAL().startEspNow(1);
    if (!radio_started_) { message_ = "ESP-NOW unavailable; BLE/Setup still available"; return; }
    radio_ = std::make_shared<RadioState>();
    std::weak_ptr<RadioState> weak = radio_;
    // Poll physical input in the emulator task, independent of main/UI work.
    // The weak state keeps shutdown safe and never retains the AppGame object.
    game::setInputReader([weak]() -> uint16_t {
        const auto ble = gamepad::snapshot();
        if (ble.connected) return ble.buttons;
        if (auto state = weak.lock()) {
            std::lock_guard<std::mutex> lock(state->mutex);
            return state->input.read(GetHAL().millis()).buttons;
        }
        return 0;
    });
    radio_connection_ = GetHAL().onEspNowData.connect([weak](const std::vector<uint8_t>& data) {
        if (auto state = weak.lock()) {
            std::lock_guard<std::mutex> lock(state->mutex);
            const auto now = GetHAL().millis();
            if (state->input.receive(data.data(), data.size(), now)) {
                const auto mask = state->input.read(now).buttons;
                if (mask != state->logged_buttons) {
                    ESP_LOGD("GAME-INPUT", "Radio buttons=0x%03x", unsigned(mask));
                    state->logged_buttons = mask;
                }
            }
        }
    });
}

void AppGame::onRendered(lv_event_t* event)
{
    static_cast<AppGame*>(lv_event_get_user_data(event))->refresh_count_.fetch_add(1);
}

void AppGame::onOpen()
{
    {
        LvglLockGuard lock;
        refresh_count_ = 0;
        lv_display_add_event_cb(lv_display_get_default(), onRendered, LV_EVENT_RENDER_READY, this);
    }
    perf_since_ = esp_timer_get_time();
    ui_copy_us_ = 0;
    ui_frames_ = 0;
    action_ = Action::None; selected_ = 0; previous_buttons_ = 0;
    connected_before_ = false; combo_used_ = false; combo_at_ = 0;
    connection_pause_ = {};
    radio_started_ = false; radio_requested_ = true;
    // I2S requires internal DMA memory. Reserve it before the radio stacks and
    // NES worker consume/fragment that heap; the board keeps the codec alive.
    Board::GetInstance().GetAudioCodec();
    frame_ = static_cast<uint16_t*>(heap_caps_calloc(game::FramePixels, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!frame_) {
        message_ = "Not enough memory for video";
        LvglLockGuard lock; draw(Screen::Error); return;
    }
    frame_desc_ = {};
    frame_desc_.header.magic = LV_IMAGE_HEADER_MAGIC;
    frame_desc_.header.cf = LV_COLOR_FORMAT_RGB565;
    frame_desc_.header.w = game::FrameWidth; frame_desc_.header.h = game::FrameHeight;
    frame_desc_.header.stride = game::FrameWidth * 2;
    frame_desc_.data_size = game::FramePixels * 2;
    frame_desc_.data = reinterpret_cast<const uint8_t*>(frame_);
    // One existing NimBLE host serves both BLE peripherals and gamepad clients.
    gamepad::initialize();
    game::setInputReader([]() -> uint16_t {
        const auto ble = gamepad::snapshot();
        return ble.connected ? ble.buttons : 0;
    });
    GetHAL().startBleServer();
    beginScan();
    if (game::builtinRom().data && !library_.games.empty()) action_ = Action::Launch;
    {
        LvglLockGuard lock;
        video_timer_ = lv_timer_create(onVideoTimer, 16, this);
        // Match presentation and refresh polling while GAME is open. Restore
        // the normal LVGL period on close; other apps keep their usual pacing.
        if (auto* refresh = lv_display_get_refr_timer(lv_display_get_default()))
            lv_timer_set_period(refresh, 16);
    }
}

void AppGame::updateInput(uint32_t now)
{
    const auto ble = gamepad::snapshot();
    game::RemoteSnapshot remote;
    if (radio_) {
        std::lock_guard<std::mutex> lock(radio_->mutex);
        remote = radio_->input.read(GetHAL().millis());
    }
    const bool connected = ble.connected || remote.connected;
    const uint16_t mask = ble.connected ? ble.buttons : remote.buttons;
    const uint16_t pressed = mask & ~previous_buttons_;
    const bool changed = mask != previous_buttons_;
    previous_buttons_ = mask;
    const bool lost_controller = connection_pause_.update(connected, GetHAL().millis());
    if (screen_ == Screen::Playing) {
        if (lost_controller) {
            ESP_LOGW("GAME-INPUT", "Controller expired while playing");
            game::pause(true); pause_selected_ = 0; message_ = "Controller disconnected";
            LvglLockGuard lock; draw(Screen::Paused);
        } else {
            if (changed) ESP_LOGD("GAME-INPUT", "Playing buttons=0x%03x source=%s",
                                 unsigned(mask), ble.connected ? "BLE" : "radio");
            const bool combo = (mask & (gamepad::Start | gamepad::Select)) == (gamepad::Start | gamepad::Select);
            if (combo && !combo_at_) combo_at_ = now;
            if (!combo) { combo_at_ = 0; combo_used_ = false; }
            if ((pressed & gamepad::Menu) || (combo_at_ && now - combo_at_ >= 500 && !combo_used_)) {
                combo_used_ = true; action_ = Action::Pause;
            }
            game::setButtons(now < pulse_until_ ? pulse_ : 0);
        }
    } else if (screen_ == Screen::Library) {
        if (pressed & gamepad::B) action_ = Action::Exit;
        if (library_.games.empty()) {
            if (pressed & (gamepad::A | gamepad::Start)) action_ = Action::Refresh;
            connected_before_ = connected;
            return;
        }
        int delta = (pressed & gamepad::Down) ? 1 : (pressed & gamepad::Up) ? -1 : 0;
        if (!delta && now - last_move_ > 180) delta = (mask & gamepad::Down) ? 1 : (mask & gamepad::Up) ? -1 : 0;
        if (delta) {
            last_move_ = now;
            selected_ = (selected_ + library_.games.size() + delta) % library_.games.size();
            LvglLockGuard lock; draw(Screen::Library);
        }
        if (pressed & (gamepad::A | gamepad::Start)) action_ = Action::Launch;
        if (pressed & gamepad::B) action_ = Action::Exit;
    } else if (screen_ == Screen::Paused) {
        const auto next = game::pauseMenuSelection(pause_selected_, pressed);
        if (next != pause_selected_) {
            pause_selected_ = next;
            LvglLockGuard lock; draw(Screen::Paused);
        }
        static const Action choices[] = {Action::Resume, Action::Library, Action::Start, Action::Select, Action::Exit};
        if (pressed & gamepad::A) action_ = choices[pause_selected_];
        if (pressed & gamepad::Menu) action_ = Action::Resume;
        if (pressed & gamepad::B) action_ = Action::Library;
        if (pressed & gamepad::Select) action_ = Action::Select;
        if (pressed & gamepad::Start) action_ = Action::Start;
    } else if (screen_ == Screen::Error && pressed & (gamepad::A | gamepad::B)) {
        action_ = Action::Library;
    }
    connected_before_ = connected;
}

void AppGame::onRunning()
{
    if (radio_requested_) enableRemote();
    const auto now = GetHAL().millis();
    if (scan_ && scan_->done.load()) {
        library_ = std::move(scan_->library); scan_.reset();
        selected_ = 0; message_ = library_.status;
        LvglLockGuard lock; draw(Screen::Library);
    }
    updateInput(now);
    const auto action = action_.exchange(Action::None);
    const auto launch_index = launch_index_.exchange(SIZE_MAX);
    if (action == Action::Launch && launch_index != SIZE_MAX) selected_ = launch_index;
    if (action == Action::Exit) { close(); return; }
    if (action == Action::Refresh || action == Action::Library) {
        game::stop(); beginScan();
    } else if (action == Action::Launch && selected_ < library_.games.size() && frame_) {
        message_ = "Loading " + library_.games[selected_].name;
        if (game::start(library_.games[selected_].path)) {
            LvglLockGuard lock; draw(Screen::Loading);
        } else {
            const auto status = game::runtimeStatus();
            message_ = status.error.empty() ? "Game task is still busy. Try again." : status.error;
            LvglLockGuard lock; draw(Screen::Error);
        }
    } else if (action == Action::Pause) {
        game::pause(true); pause_selected_ = 0; message_ = "Paused";
        LvglLockGuard lock; draw(Screen::Paused);
    } else if (action == Action::Resume || action == Action::Start || action == Action::Select) {
        pulse_ = action == Action::Start ? 8 : action == Action::Select ? 4 : 0;
        pulse_until_ = now + 100;
        game::pause(false);
        LvglLockGuard lock; draw(Screen::Playing);
    } else if (action == Action::Previous || action == Action::Next) {
        if (!library_.games.empty()) {
            const size_t count = library_.games.size();
            selected_ = action == Action::Next ? (selected_ + 4) % count : (selected_ + count - std::min(size_t(4), count)) % count;
        }
        LvglLockGuard lock; draw(Screen::Library);
    } else if (action == Action::Remote) {
        enableRemote();
        LvglLockGuard lock; draw(Screen::Library);
    }
    const auto state = game::runtimeStatus();
    if (!scan_ && state.finished && screen_ != Screen::Library && screen_ != Screen::Error) {
        message_ = state.error.empty() ? "Game stopped" : state.error;
        LvglLockGuard lock; draw(Screen::Error);
    }
    if (state.running && !state.paused && state.frames && screen_ == Screen::Loading && frame_) {
        LvglLockGuard lock;
        draw(Screen::Playing);
    }
}

void AppGame::onVideoTimer(lv_timer_t* timer)
{
    static_cast<AppGame*>(lv_timer_get_user_data(timer))->presentFrame();
}

void AppGame::presentFrame()
{
    // Runs under the LVGL lock on CPU 1, independently of the emulator and
    // lower-priority Mooncake main task on CPU 0.
    const auto state = game::runtimeStatus();
    const int64_t perfNow = esp_timer_get_time();
    if (!state.running || state.paused || screen_ != Screen::Playing || !frame_ || !image_) {
        perf_since_ = perfNow;
        ui_copy_us_ = 0;
        ui_frames_ = 0;
        refresh_count_ = 0;
        return;
    }
    if (game::takeFrame(frame_)) {
        lv_image_cache_drop(&frame_desc_);
        frame_desc_.data = reinterpret_cast<const uint8_t*>(frame_);
        lv_obj_invalidate(image_);
        ++ui_frames_;
        ui_copy_us_ += esp_timer_get_time() - perfNow;
    }
    if (perfNow - perf_since_ >= 5000000) {
        const double seconds = (perfNow - perf_since_) / 1000000.0;
        ESP_LOGI("NES", "Display submitted %.1f FPS, rendered %.1f FPS; CPU %d copy %.1f ms",
                 ui_frames_ / seconds, refresh_count_.exchange(0) / seconds,
                 xPortGetCoreID(),
                 ui_copy_us_ / (1000.0 * std::max(uint32_t(1), ui_frames_)));
        perf_since_ = perfNow;
        ui_copy_us_ = 0;
        ui_frames_ = 0;
    }
}

void AppGame::onClose()
{
    {
        LvglLockGuard lock;
        if (video_timer_) { lv_timer_delete(video_timer_); video_timer_ = nullptr; }
        if (auto* refresh = lv_display_get_refr_timer(lv_display_get_default()))
            lv_timer_set_period(refresh, LV_DEF_REFR_PERIOD);
        lv_display_remove_event_cb_with_user_data(lv_display_get_default(), onRendered, this);
    }
    // No UI lock while waiting: the SD worker may currently need that lock.
    if (radio_connection_ >= 0) GetHAL().onEspNowData.disconnect(radio_connection_);
    radio_connection_ = -1; radio_.reset();
    game::setButtons(0); game::stop();
    game::setInputReader({});
    while (scan_ && !scan_->done.load()) GetHAL().delay(20);
    scan_.reset();
    {
        LvglLockGuard lock;
        if (root_) { lv_obj_delete(root_); root_ = nullptr; }
        lv_image_cache_drop(&frame_desc_);
        bindings_.clear();
        heap_caps_free(frame_); frame_ = nullptr;
    }
    // The existing ESP-NOW radio owns Wi-Fi until reboot; retain NVS/network settings.
    if (radio_started_) GetHAL().requestWarmReboot(0);
}
