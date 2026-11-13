/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "M5Unified.h"

extern "C" {
#include <stdio.h>
#include "esp_log.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "lvgl.h"

#include "ui.h"
#include "esp_now_init.h"
#include "joystick_handle.h"
#include "joystick/game_controls.h"

#include "lvgl_port.h"

using namespace m5;

joystick_data_t joystick_data;

// extern void lvgl_port_init(M5GFX &gfx);

/**
 * @brief Handle Button Press.
 * 1. Press BtnA to switch setup_mode UI and running_mode UI.
 * 2. Press BtnB to switch espnow-channel or id on setup_mode;
 * 3. Press BtnB to send btnB_status to remote on running_mode.
 */
// Game input uses a distinct seven-byte payload, never a servo packet.
static uint8_t game_sequence;
static uint32_t setup_b_since, exit_since;
static int center_x = X_CENTER, center_y = Y_CENTER;
static bool wait_release;
static game_menu_hold_t menu_hold;
static uint32_t menu_until;

static void send_game(uint16_t buttons)
{
    static uint16_t last_buttons = UINT16_MAX;
    uint8_t packet[7] = {0x47, 0x50, 1, (uint8_t)joystick_data.id,
                         (uint8_t)buttons, (uint8_t)(buttons >> 8), game_sequence++};
    espnow_send_data(packet, sizeof(packet));
    if (buttons != last_buttons) {
        ESP_LOGI("GAME-REMOTE", "target=%u buttons=0x%03x sequence=%u",
                 (unsigned)joystick_data.id, (unsigned)buttons, (unsigned)packet[6]);
        last_buttons = buttons;
    }
}

static void show_mode(uint8_t mode)
{
    while (!lvgl_port_lock()) vTaskDelay(1);
    if (mode == MODE_GAME) {
        static lv_obj_t *game_screen;
        if (!game_screen) {
            game_screen = lv_obj_create(NULL);
            lv_obj_t *label = lv_label_create(game_screen);
            lv_label_set_text(label, "GAME\nA / B: fire\nStick: D-pad\nPress stick: Start\nHold A: Menu\nA+B 2s: Exit\nSelect: in Menu");
            lv_obj_center(label);
        }
        lv_scr_load(game_screen);
    } else {
        // Screen creation functions take the LVGL lock themselves.
        lvgl_port_unlock();
        switch_screen(mode);
        return;
    }
    lvgl_port_unlock();
}

void handle_button_press()
{
    const uint32_t now = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
    if (joystick_data.screen_mode == MODE_GAME) {
        bool a = M5.BtnA.isPressed(), b = M5.BtnB.isPressed();
        if (wait_release) { if (!a && !b) wait_release = false; a = b = false; }
        if (a && b) {
            if (!exit_since) exit_since = now;
            if (now - exit_since >= 2000) {
                send_game(0);
                joystick_data.screen_mode = MODE_SETUP;
                wait_release = true;
                exit_since = setup_b_since = 0;
                show_mode(MODE_SETUP);
                return;
            }
        } else exit_since = 0;
        if (game_menu_hold_update(&menu_hold, a, b, now)) menu_until = now + 500;
        uint16_t x = center_x, y = center_y;
        bool down = false;
        uint16_t buttons = (a ? 1 : 0) | (b ? 2 : 0);
        const bool axes_valid = joystick_read_game(&x, &y, &down);
        buttons |= game_buttons_horizontal_left(axes_valid ? (int32_t)x - center_x : 0,
                                                axes_valid ? (int32_t)y - center_y : 0,
                                                DEAD_ZONE, down);
        // Once the hold opens the menu, keep A consumed until release so it
        // cannot immediately confirm the highlighted item in that menu.
        if (menu_hold.fired) buttons &= ~1u;
        if ((int32_t)(menu_until - now) > 0) buttons |= 256;
        send_game(buttons);
        return;
    }
    if (wait_release) {
        if (!M5.BtnA.isPressed() && !M5.BtnB.isPressed()) wait_release = false;
        return;
    }
    if (joystick_data.screen_mode == MODE_SETUP && M5.BtnB.isPressed()) {
        if (!setup_b_since) setup_b_since = now;
        if (now - setup_b_since >= 1000) {
            joystick_data.screen_mode = MODE_GAME;
            wifi_espnow_reinit(joystick_data.channel);
            // Release the stick while entering: its neutral position is calibrated here.
            uint16_t x, y; bool down;
            if (joystick_read_game(&x, &y, &down)) {
                center_x = x; center_y = y;
                ESP_LOGI("GAME-REMOTE", "Mini JoyC calibrated: x=%u y=%u center=%u channel=%u",
                         (unsigned)x, (unsigned)y, (unsigned)down, (unsigned)joystick_data.channel);
            } else {
                ESP_LOGW("GAME-REMOTE", "Mini JoyC read failed; check HAT connection");
            }
            wait_release = true;
            menu_hold = {}; menu_until = 0;
            show_mode(MODE_GAME);
            send_game(0);
            return;
        }
    } else setup_b_since = 0;
    if (M5.BtnA.wasPressed()) {
        joystick_data.screen_mode = (joystick_data.screen_mode + 1) % 3;
        if (joystick_data.screen_mode == MODE_RUNNING) wifi_espnow_reinit(joystick_data.channel);
        show_mode(joystick_data.screen_mode);
    }
    // Setup selection changes on release, so the long game-entry gesture is unambiguous.
    if (joystick_data.screen_mode == MODE_SETUP && M5.BtnB.wasReleased()) {
        joystick_data.select_mode = !joystick_data.select_mode;
    } else if (joystick_data.screen_mode != MODE_SETUP && M5.BtnB.wasPressed()) {
        joystick_data.btnB_status = !joystick_data.btnB_status;
    }
}

void app_main(void)
{
    imu_data_t imu_data;

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    M5.begin();
    M5.Power.begin();
    M5.Lcd.setBrightness(100);  // set brightness to 100
    M5.Imu.init(&M5.In_I2C);    // init IMU with internal I2C port
    printf("IN_I2C port: %d\n", M5.In_I2C.getPort());
    printf("M5 Display width: %ld, height: %ld\n", M5.Display.width(), M5.Display.height());

    joystick_data = joystick_init();  // init joystick

    lvgl_port_init();  // init LVGL
    ui_init();         // init UI

    // init WiFi and ESP-NOW
    wifi_espnow_init(joystick_data.channel);

    xTaskCreate(handle_setup_screen, "handle_setup_screen", 8192, &joystick_data, 5, NULL);      // handle setup mode
    xTaskCreate(handle_running_screen, "handle_running_screen", 8192, &joystick_data, 5, NULL);  // handle running mode
    xTaskCreate(handle_imu_screen, "handle_imu_screen", 8192, &joystick_data, 5, NULL);

    TickType_t last_wake = xTaskGetTickCount();
    while (1) {
        M5.update();
        // Handle button press
        handle_button_press();
        joystick_data.bat = (M5.Power.Axp192.getBatteryLevel());  // updata battery level

        joystick_data.bat = (joystick_data.bat > 100) ? 100 : joystick_data.bat;
        joystick_data.bat = (joystick_data.bat < 0) ? 0 : joystick_data.bat;

        M5.Imu.update();                              // update IMU data
        imu_data              = M5.Imu.getImuData();  // get IMU data
        joystick_data.accel_x = imu_data.accel.x;
        joystick_data.accel_y = imu_data.accel.y;
        joystick_data.accel_z = imu_data.accel.z;

#if 0
        printf("Accel: (%.2f, %.2f, %.2f), Gyro: (%.2f, %.2f, %.2f)\n",
               joystick_data.accel_x, joystick_data.accel_y, joystick_data.accel_z,
               joystick_data.gyro_x, joystick_data.gyro_y, joystick_data.gyro_z);
#endif
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(20));
    }
}
}
