#pragma once
#include <mooncake.h>
#include <lvgl.h>
#include "game_runtime.h"
#include "game_remote.h"
#include <array>
#include <atomic>
#include <memory>
#include <mutex>

class AppGame : public mooncake::AppAbility {
public:
    AppGame();
    void onOpen() override;
    void onRunning() override;
    void onClose() override;
private:
    enum class Screen { Library, Loading, Playing, Paused, Error };
    enum class Action { None, Refresh, Launch, Previous, Next, Resume, Library, Exit, Pause, Start, Select, Remote };
    struct Binding { AppGame* app; Action action; size_t index; };
    struct ScanResult { std::atomic_bool done{false}; game::Library library; };
    struct RadioState { std::mutex mutex; game::RemoteInput input; uint16_t logged_buttons = UINT16_MAX; };
    Screen screen_ = Screen::Library;
    std::atomic<Action> action_{Action::None};
    std::atomic<size_t> launch_index_{SIZE_MAX};
    std::vector<std::unique_ptr<Binding>> bindings_;
    std::shared_ptr<ScanResult> scan_;
    std::shared_ptr<RadioState> radio_;
    game::ConnectionPause connection_pause_;
    game::Library library_;
    lv_obj_t *root_ = nullptr, *image_ = nullptr, *info_ = nullptr;
    std::array<lv_obj_t*, 4> rows_{};
    std::array<lv_obj_t*, 5> pause_buttons_{};
    lv_image_dsc_t frame_desc_{};
    lv_timer_t* video_timer_ = nullptr;
    uint16_t* frame_ = nullptr;
    size_t selected_ = 0, pause_selected_ = 0;
    uint32_t last_move_ = 0, combo_at_ = 0, pulse_until_ = 0;
    uint16_t previous_buttons_ = 0, pulse_ = 0;
    bool radio_started_ = false, radio_requested_ = true, connected_before_ = false, combo_used_ = false;
    int radio_connection_ = -1;
    std::string message_;
    static void onRendered(lv_event_t* event);
    static void onVideoTimer(lv_timer_t* timer);
    void presentFrame();
    std::atomic<uint32_t> refresh_count_{0};
    int64_t perf_since_ = 0, ui_copy_us_ = 0;
    uint32_t ui_frames_ = 0;
    void draw(Screen screen);
    void drawLibraryRows();
    void beginScan();
    void updateInput(uint32_t now);
    void enableRemote();
    lv_obj_t* button(const char* label, int x, int y, int width, Action action, size_t index = 0);
};
