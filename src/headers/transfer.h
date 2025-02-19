#pragma once
#include <stdbool.h>
#include <pico/stdlib.h>
#include "ctrl.h"
#include <string.h>
#include <pico/time.h>
#include <hardware/gpio.h>
#include "button.h"
#include "config.h"
#include "profile.h"
#include "hid.h"
#include "bus.h"
#include "pin.h"
#include "common.h"
#include "switch_pro.h"


typedef struct {
    bool wifi_allow_communication;  // Extern.
    bool synced_keyboard;
    bool synced_mouse;
    bool synced_gamepad;
    uint16_t alarms;
    alarm_pool_t *alarm_pool;

    uint8_t wifi_matrix[256];   // Assuming this is an array of 256 uint8_t values.
    int16_t mouse_x;
    int16_t mouse_y;
    double gamepad_lx;
    double gamepad_ly;
    double gamepad_rx;
    double gamepad_ry;
    double gamepad_lz;
    double gamepad_rz;

    Vector gamepad_gyro;   // Assuming this is a custom struct or typedef.
    Vector gamepad_accel;  // Assuming this is a custom struct or typedef.
    // bool synced_switch_pro;
    // input_report_t switch_pro_gamepad_data;
    SwitchProUsb switchProUsb;  // Assuming this is another custom struct or typedef.
} transfer_struct;

// void wifi_thanks();
void wifi_matrix_reset(uint8_t keep);
void wifi_press(uint8_t key);
void wifi_release(uint8_t key);
void wifi_press_multiple(uint8_t *keys);
void wifi_release_multiple(uint8_t *keys);
void wifi_press_later(uint8_t key, uint16_t delay);
void wifi_release_later(uint8_t key, uint16_t delay);
void wifi_press_multiple_later(uint8_t *keys, uint16_t delay);
void wifi_release_multiple_later(uint8_t *keys, uint16_t delay);
void wifi_press_later_callback(alarm_id_t alarm, uint8_t key);
void wifi_release_later_callback(alarm_id_t alarm, uint8_t key);
void wifi_press_multiple_later_callback(alarm_id_t alarm, uint8_t *keys);
void wifi_release_multiple_later_callback(alarm_id_t alarm, uint8_t *keys);
void wifi_macro(uint8_t index);
bool wifi_is_axis(uint8_t key);
bool wifi_is_mouse_move(uint8_t key);
void wifi_mouse_move(int16_t x, int16_t y);
//void wifi_mouse_wheel(int8_t z);
void wifi_gamepad_lx(double value);
void wifi_gamepad_ly(double value);
void wifi_gamepad_rx(double value);
void wifi_gamepad_ry(double value);
void wifi_gamepad_lz(double value);
void wifi_gamepad_rz(double value);
void wifi_gamepad_gyro(double x, double y, double z);
void wifi_gamepad_accel(double x, double y, double z);

void wifi_report();
void wifi_init();

extern bool wifi_allow_communication;
