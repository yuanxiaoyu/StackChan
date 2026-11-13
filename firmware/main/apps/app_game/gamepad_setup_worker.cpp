#include "gamepad_setup_worker.h"
#include <hal/hal.h>
#include <algorithm>

LV_FONT_DECLARE(agent_tasks_cjk_14);
namespace game {
namespace {
lv_obj_t* text(lv_obj_t* parent, const char* value, int x, int y, int width)
{
    auto* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, &agent_tasks_cjk_14, 0);
    lv_obj_set_pos(label, x, y); lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_label_set_text(label, value);
    return label;
}
lv_obj_t* control(lv_obj_t* parent, const char* value, int x, int y, int width)
{
    auto* button = lv_button_create(parent);
    lv_obj_set_pos(button, x, y); lv_obj_set_size(button, width, 33);
    lv_obj_set_style_shadow_width(button, 0, 0);
    lv_obj_set_style_radius(button, 10, 0);
    auto* label = text(button, value, 0, 0, width - 10);
    lv_obj_center(label);
    return button;
}
}

GamepadSetupWorker::GamepadSetupWorker()
{
    gamepad::initialize(); GetHAL().startBleServer();
    root_ = lv_obj_create(lv_screen_active());
    lv_obj_set_size(root_, 320, 240); lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    text(root_, "Gamepad / 蓝牙手柄", 12, 8, 295);
    state_ = text(root_, "BLE HID / CodexPad C10", 12, 35, 295);
    lv_obj_set_height(state_, 34);
    for (size_t i = 0; i < buttons_.size(); ++i) {
        buttons_[i] = control(root_, "", 10, 75 + i * 35, 300);
        rows_[i] = {this, i};
        lv_obj_add_event_cb(buttons_[i], [](lv_event_t* event) {
            const auto* row = static_cast<Row*>(lv_event_get_user_data(event));
            const size_t index = row->self->page_ * 3 + row->index;
            if (index < row->self->devices_.size()) gamepad::connect(row->self->devices_[index].id);
        }, LV_EVENT_CLICKED, &rows_[i]);
    }
    auto* scan = control(root_, "Scan", 8, 187, 69);
    auto* next = control(root_, ">", 81, 187, 40);
    auto* forget = control(root_, "Forget", 125, 187, 86);
    auto* back = control(root_, "Back", 215, 187, 96);
    lv_obj_add_event_cb(scan, [](lv_event_t*) { gamepad::scan(); }, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(next, [](lv_event_t* e) {
        auto* self = static_cast<GamepadSetupWorker*>(lv_event_get_user_data(e));
        self->page_ = (self->page_ + 1) % std::max(size_t(1), (self->devices_.size() + 2) / 3);
        self->refreshRows();
    }, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(forget, [](lv_event_t*) { gamepad::forget(); }, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(back, [](lv_event_t* e) {
        static_cast<GamepadSetupWorker*>(lv_event_get_user_data(e))->_is_done = true;
    }, LV_EVENT_CLICKED, this);
    text(root_, "自带遥控器使用 GAME 的 Remote 模式", 12, 222, 295);
    gamepad::scan();
}

void GamepadSetupWorker::refreshRows()
{
    for (size_t i = 0; i < buttons_.size(); ++i) {
        const size_t index = page_ * 3 + i;
        if (index < devices_.size()) {
            lv_obj_remove_flag(buttons_[i], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(lv_obj_get_child(buttons_[i], 0), devices_[index].name.c_str());
        } else lv_obj_add_flag(buttons_[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void GamepadSetupWorker::update()
{
    const auto now = GetHAL().millis();
    if (now - last_update_ < 250) return;
    last_update_ = now;
    const auto snapshot = gamepad::snapshot();
    std::string status = snapshot.status;
    if (snapshot.connected) status = "Connected: " + snapshot.name;
    lv_label_set_text(state_, status.c_str());
    auto list = gamepad::devices();
    bool changed = list.size() != devices_.size();
    if (!changed) for (size_t i = 0; i < list.size(); ++i) {
        if (list[i].id != devices_[i].id || list[i].name != devices_[i].name) { changed = true; break; }
    }
    if (changed) { devices_ = std::move(list); page_ = 0; refreshRows(); }
}

GamepadSetupWorker::~GamepadSetupWorker()
{
    gamepad::cancel_scan();
    if (root_) lv_obj_delete(root_);
}
}
