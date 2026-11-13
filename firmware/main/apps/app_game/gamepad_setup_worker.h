#pragma once
#include <apps/app_setup/workers/workers.h>
#include <hal/gamepad/gamepad.h>
#include <lvgl.h>
#include <array>

namespace game {
class GamepadSetupWorker : public setup_workers::WorkerBase {
public:
    GamepadSetupWorker();
    ~GamepadSetupWorker() override;
    void update() override;
private:
    struct Row { GamepadSetupWorker* self; size_t index; };
    lv_obj_t *root_ = nullptr, *state_ = nullptr;
    std::array<lv_obj_t*, 3> buttons_{};
    std::array<Row, 3> rows_{};
    std::vector<gamepad::Device> devices_;
    uint32_t last_update_ = 0;
    size_t page_ = 0;
    void refreshRows();
};
}
