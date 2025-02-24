// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022, Input Labs Oy.

/*
HID layer is responsible for managing all the outputs that are sent to the
operating system (via USB or wireless), by keeping a register of all the active
actions (requested by the current profile) and syncing its state with the
available interfaces (keyboard, mouse, gamepad...).

Physical controller --> Profile --> [HID layer] --> HID keyboard
                    --> Profile -->             --> HID mouse
                    --> Profile -->             --> XInput gamepad
                    --> Profile -->             --> Internal procedures

At the end of each cycle (determined by the polling rate) the HID layer checks
if the potential new report is different from the last report sent to the
interfaces (USB keyboard, USB mouse, gamepad...), and sends the report if
required.

The state matrix is a representation of all the actions that could be sent
(output) and internal operations (procedures) requested by the user. It keep
track of how many references to these actions are active.
As a simplification: Button presses increase the counter by one, and button
releases decrease the counter by one.

To avoid orphan references, the state matrix is usually re-initialized (reset)
to zeros when the user changes the active profile, otherwise the disabled
profile won't ever trigger the corresponding counter decrease of held buttons
during the profile change.
*/

#include <tusb.h>
#include "config.h"
#include "ctrl.h"
#include "hid.h"
#include "led.h"
#include "profile.h"
#include "xinput.h"
#include "common.h"
#include "webusb.h"
#include "logging.h"
#include "thanks.h"
#include "generic.h"
#include "keybrd.h"
#include "mouse.h"
#include "switch_pro.h"
#include "xbox1914.h"
#include "util.h"
#include "dual_shock_4.h"
#include "dual_sense.h"
#include "vector.h"

bool hid_allow_communication = true; // Extern.
bool synced_keyboard = false;
bool synced_mouse = false;
bool synced_gamepad = false;
uint16_t alarms = 0;
alarm_pool_t *alarm_pool;

uint8_t state_matrix[256] = {
    0,
};
int16_t mouse_x = 0;
int16_t mouse_y = 0;
double gamepad_lx = 0;
double gamepad_ly = 0;
double gamepad_rx = 0;
double gamepad_ry = 0;
double gamepad_lz = 0;
double gamepad_rz = 0;

Vector gamepad_gyro = {0};
Vector gamepad_accel = {0};
// bool synced_switch_pro = true;
// input_report_t switch_pro_gamepad_data;
SwitchProUsb switchProUsb;

// 重置state_matrix中的信息
void hid_matrix_reset(uint8_t keep)
{
    for (uint8_t action = 0; action < 255; action++)
    {
        if (action == keep)
            continue; // Optionally do not reset specific actions.
        state_matrix[action] = 0;
    }
    synced_keyboard = false;
    synced_mouse = false;
    synced_gamepad = false;
}

// 处理按下动作
void hid_procedure_press(uint8_t procedure)
{
    if (procedure == PROC_HOME)
        profile_set_home(true); // Hold home.
    if (procedure == PROC_HOME_GAMEPAD)
        profile_set_home_gamepad(true); // Double-click-hold home.
    if (procedure == PROC_PROFILE_1)
        profile_set_active(1);
    if (procedure == PROC_PROFILE_2)
        profile_set_active(2);
    if (procedure == PROC_PROFILE_3)
        profile_set_active(3);
    if (procedure == PROC_PROFILE_4)
        profile_set_active(4);
    if (procedure == PROC_PROFILE_5)
        profile_set_active(5);
    if (procedure == PROC_PROFILE_6)
        profile_set_active(6);
    if (procedure == PROC_PROFILE_7)
        profile_set_active(7);
    if (procedure == PROC_PROFILE_8)
        profile_set_active(8);
    if (procedure == PROC_PROFILE_9)
        profile_set_active(9);
    if (procedure == PROC_PROFILE_10)
        profile_set_active(10);
    if (procedure == PROC_PROFILE_11)
        profile_set_active(11);
    if (procedure == PROC_PROFILE_12)
        profile_set_active(12);
    if (procedure == PROC_TUNE_UP)
        config_tune(1);
    if (procedure == PROC_TUNE_DOWN)
        config_tune(0);
    if (procedure == PROC_TUNE_OS)
        config_tune_set_mode(procedure);
    if (procedure == PROC_TUNE_MOUSE_SENS)
        config_tune_set_mode(procedure);
    if (procedure == PROC_TUNE_TOUCH_SENS)
        config_tune_set_mode(procedure);
    if (procedure == PROC_TUNE_DEADZONE)
        config_tune_set_mode(procedure);
    if (procedure == PROC_CALIBRATE)
        config_calibrate();
    if (procedure == PROC_RESTART)
        config_reboot();
    if (procedure == PROC_BOOTSEL)
        config_bootsel();
    if (procedure == PROC_THANKS)
        hid_thanks();
    if (procedure == PROC_IGNORE_LED_WARNINGS)
        config_ignore_problems();
    // Scrollwheel alternative modes. (Used for example in Racing profile).
    if (procedure == PROC_ROTARY_MODE_0)
        rotary_set_mode(0);
    if (procedure == PROC_ROTARY_MODE_1)
        rotary_set_mode(1);
    if (procedure == PROC_ROTARY_MODE_2)
        rotary_set_mode(2);
    if (procedure == PROC_ROTARY_MODE_3)
        rotary_set_mode(3);
    if (procedure == PROC_ROTARY_MODE_4)
        rotary_set_mode(4);
    if (procedure == PROC_ROTARY_MODE_5)
        rotary_set_mode(5);
    // Macros.
    if (procedure == PROC_MACRO_1)
        hid_macro(1);
    if (procedure == PROC_MACRO_2)
        hid_macro(2);
    if (procedure == PROC_MACRO_3)
        hid_macro(3);
    if (procedure == PROC_MACRO_4)
        hid_macro(4);
    if (procedure == PROC_MACRO_5)
        hid_macro(5);
    if (procedure == PROC_MACRO_6)
        hid_macro(6);
    if (procedure == PROC_MACRO_7)
        hid_macro(7);
    if (procedure == PROC_MACRO_8)
        hid_macro(8);
}

// 处理松开动作
void hid_procedure_release(uint8_t procedure)
{
    if (procedure == PROC_HOME)
        profile_set_home(false);
    if (procedure == PROC_HOME_GAMEPAD)
        profile_set_home_gamepad(false);
}

// 调用hid_procedure_press，实现按下动作
// 程序使用state_matrix[]数组来存储按键动作,
void hid_press(uint8_t key)
{
    if (key == KEY_NONE)
        return;
    else if (key >= PROC_INDEX)
        hid_procedure_press(key);
    else
    {
        // 根据 key 的范围更新相应设备的同步标志
        state_matrix[key] += 1;
        if (key >= GAMEPAD_INDEX)
            synced_gamepad = false;
        else if (key >= MOUSE_INDEX)
            synced_mouse = false;
        else
            synced_keyboard = false;
    }
}

// 调用 hid_procedure_release，实现按下动作
void hid_release(uint8_t key)
{
    if (key == KEY_NONE)
        return;
    else if (key == MOUSE_SCROLL_UP)
        return;
    else if (key == MOUSE_SCROLL_DOWN)
        return;
    else if (key >= PROC_INDEX)
        hid_procedure_release(key);
    else
    {
        if (state_matrix[key] > 0)
        { // Do not allow to wrap / go negative.
            state_matrix[key] -= 1;
            if (key >= GAMEPAD_INDEX)
                synced_gamepad = false;
            else if (key >= MOUSE_INDEX)
                synced_mouse = false;
            else
                synced_keyboard = false;
        }
    }
}

void hid_press_multiple(uint8_t *keys)
{
    for (uint8_t i = 0; i < ACTIONS_LEN; i++)
    {
        if (keys[i] == 0)
            return;
        hid_press(keys[i]);
    }
}

void hid_release_multiple(uint8_t *keys)
{
    for (uint8_t i = 0; i < ACTIONS_LEN; i++)
    {
        if (keys[i] == 0)
            return;
        hid_release(keys[i]);
    }
}

void hid_press_later(uint8_t key, uint16_t delay)
{
    alarm_pool_add_alarm_in_ms(
        alarm_pool,
        delay,
        (alarm_callback_t)hid_press_later_callback,
        (void *)(uint32_t)key,
        true);
    alarms++;
}

void hid_release_later(uint8_t key, uint16_t delay)
{
    alarm_pool_add_alarm_in_ms(
        alarm_pool,
        delay,
        (alarm_callback_t)hid_release_later_callback,
        (void *)(uint32_t)key,
        true);
    alarms++;
}

void hid_press_multiple_later(uint8_t *keys, uint16_t delay)
{
    alarm_pool_add_alarm_in_ms(
        alarm_pool,
        delay,
        (alarm_callback_t)hid_press_multiple_later_callback,
        keys,
        true);
    alarms++;
}

void hid_release_multiple_later(uint8_t *keys, uint16_t delay)
{
    alarm_pool_add_alarm_in_ms(
        alarm_pool,
        delay,
        (alarm_callback_t)hid_release_multiple_later_callback,
        keys,
        true);
    alarms++;
}

void hid_press_later_callback(alarm_id_t alarm, uint8_t key)
{
    alarm_pool_cancel_alarm(alarm_pool, alarm);
    hid_press(key);
    alarms--;
}

void hid_release_later_callback(alarm_id_t alarm, uint8_t key)
{
    alarm_pool_cancel_alarm(alarm_pool, alarm);
    hid_release(key);
    alarms--;
}

void hid_press_multiple_later_callback(alarm_id_t alarm, uint8_t *keys)
{
    alarm_pool_cancel_alarm(alarm_pool, alarm);
    hid_press_multiple(keys);
    alarms--;
}

void hid_release_multiple_later_callback(alarm_id_t alarm, uint8_t *keys)
{
    alarm_pool_cancel_alarm(alarm_pool, alarm);
    hid_release_multiple(keys);
    alarms--;
}

void hid_macro(uint8_t index)
{
    uint8_t section = SECTION_MACRO_1 + ((index - 1) / 2);
    uint8_t subindex = (index - 1) % 2;
    CtrlProfile *profile = config_profile_read(profile_get_active_index(false));
    uint8_t *macro = profile->sections[section].macro.macro[subindex];
    if (alarms > 0)
        return; // Disallows parallel macros. TODO fix.
    uint16_t time = 10;
    for (uint8_t i = 0; i < 28; i++)
    {
        if (macro[i] == 0)
            break;
        hid_press_later(macro[i], time);
        time += 10;
        hid_release_later(macro[i], time);
        time += 10;
    }
}

bool hid_is_axis(uint8_t key)
{
    return is_between(key, GAMEPAD_AXIS_INDEX, PROC_INDEX - 1);
}

bool hid_is_mouse_move(uint8_t key)
{
    return is_between(key, MOUSE_X, MOUSE_Y_NEG);
}

void hid_mouse_move(int16_t x, int16_t y)
{
    mouse_x += x;
    mouse_y += y;
    synced_mouse = false;
}

void hid_gamepad_lx(double value)
{
    if (value == gamepad_lx)
        return;
    gamepad_lx += value; // Multiple inputs can be combined.
    synced_gamepad = false;
}

void hid_gamepad_ly(double value)
{
    if (value == gamepad_ly)
        return;
    gamepad_ly += value; // Multiple inputs can be combined.
    synced_gamepad = false;
}

void hid_gamepad_lz(double value)
{
    if (value == gamepad_lz)
        return;
    gamepad_lz += value; // Multiple inputs can be combined.
    synced_gamepad = false;
}

void hid_gamepad_rx(double value)
{
    if (value == gamepad_rx)
        return;
    gamepad_rx += value; // Multiple inputs can be combined.
    synced_gamepad = false;
}

void hid_gamepad_ry(double value)
{
    if (value == gamepad_ry)
        return;
    gamepad_ry += value; // Multiple inputs can be combined.
    synced_gamepad = false;
}

void hid_gamepad_rz(double value)
{
    if (value == gamepad_rz)
        return;
    gamepad_rz += value; // Multiple inputs can be combined.
    synced_gamepad = false;
}

void hid_gamepad_gyro(double x, double y, double z)
{
    bool b = false;
    if (gamepad_gyro.x != x)
    {
        gamepad_gyro.x = x;
        b = true;
    }
    if (gamepad_gyro.y != y)
    {
        gamepad_gyro.y = y;
        b = true;
    }
    if (gamepad_gyro.z != z)
    {
        gamepad_gyro.z = z;
        b = true;
    }
    if (b)
    {
        synced_gamepad = false;
        // synced_gamepad_gyroscope = false;
    };
}

void hid_gamepad_accel(double x, double y, double z)
{
    bool b = false;
    if (gamepad_accel.x != x)
    {
        gamepad_accel.x = x;
        b = true;
    }
    if (gamepad_accel.y != y)
    {
        gamepad_accel.y = y;
        b = true;
    }
    if (gamepad_accel.z != z)
    {
        gamepad_accel.z = z;
        b = true;
    }
    if (b)
    {
        synced_gamepad = false;
        // synced_gamepad_gyroscope = false;
    };
}

void hid_mouse_report()
{
    // Create button bitmask.
    int8_t buttons = 0;
    for (int i = 0; i < 5; i++)
    {
        buttons += state_matrix[MOUSE_INDEX + i] << i;
    }
    uint8_t scroll = state_matrix[MOUSE_SCROLL_UP] - state_matrix[MOUSE_SCROLL_DOWN];
    // Create report.
    hid_mouse_custom_report_t report = {buttons, mouse_x, mouse_y, scroll, 0};
    // Reset values.
    mouse_x = 0;
    mouse_y = 0;
    state_matrix[MOUSE_SCROLL_UP] = 0;
    state_matrix[MOUSE_SCROLL_DOWN] = 0;
    // Send report.
    tud_hid_report(MOUSE_INPUT_ID, &report, sizeof(report));
}

// 函数讲state_matrix[i]的信息，读到report[]与modifier里面，state_matrix中的信息来自于hid_press、hid_release这两个函数
void hid_keyboard_report()
{
    uint8_t report[6] = {0};    // 初始化report信息数组
    uint8_t keys_available = 6;
    for (int i = 0; i <= 115; i++)  // 115键键盘
    {
        if (state_matrix[i] >= 1)   // 信息都存在state_matrix[i]里面，如果[i]按键按下，则为 1 。
        {
            report[keys_available - 1] = (uint8_t)i;    // 当对应的键盘按键处于按下状态时，
                                                        // 将该按键的编码（这里就是循环变量 i）转换为无符号 8 位整数类型，
                                                        // 并存储到 report 数组中可用位置的最后一个元素。
            keys_available--;
            if (keys_available == 0)
            {
                break;
            }
        }
    }
    // 存储计算得到的修饰键信息
    uint8_t modifier = 0;
    // 对 8 个可能的修饰键状态进行检查和计算
    for (int i = 0; i < 8; i++)
    {
        modifier += !!state_matrix[MODIFIER_INDEX + i] << i;
    }
    // 发送刚刚生成的键盘报告。在这里需要进行修改，在手柄端将报告数组通过串口发送给ESP8285，在监视狗端把函数经由USB发送给PC
    tud_hid_keyboard_report(
        KEYBRD_INPUT_ID,
        modifier,
        report);
}

double hid_axis(
    double value,
    uint8_t matrix_index_pos,
    uint8_t matrix_index_neg)
{
    if (matrix_index_neg)
    {
        if (state_matrix[matrix_index_neg])
            return -1;
        else if (state_matrix[matrix_index_pos])
            return 1;
        else
            return constrain(value, -1, 1);
    }
    else
    {
        if (state_matrix[matrix_index_pos])
            return 1;
        else
            return constrain(fabs(value), 0, 1);
    }
}

void hid_gamepad_report()
{
    // Sorted so the most common assigned buttons are lower and easier to
    // identify in-game.
    int32_t buttons = ((state_matrix[GAMEPAD_A] << 0) +
                       (state_matrix[GAMEPAD_B] << 1) +
                       (state_matrix[GAMEPAD_X] << 2) +
                       (state_matrix[GAMEPAD_Y] << 3) +
                       (state_matrix[GAMEPAD_L1] << 4) +
                       (state_matrix[GAMEPAD_R1] << 5) +
                       (state_matrix[GAMEPAD_L3] << 6) +
                       (state_matrix[GAMEPAD_R3] << 7) +
                       (state_matrix[GAMEPAD_LEFT] << 8) +
                       (state_matrix[GAMEPAD_RIGHT] << 9) +
                       (state_matrix[GAMEPAD_UP] << 10) +
                       (state_matrix[GAMEPAD_DOWN] << 11) +
                       (state_matrix[GAMEPAD_SELECT] << 12) +
                       (state_matrix[GAMEPAD_START] << 13) +
                       (state_matrix[GAMEPAD_HOME] << 14));
    // Adjust range from [-1,1] to [-32767,32767].
    int16_t lx_report = hid_axis(gamepad_lx, GAMEPAD_AXIS_LX, GAMEPAD_AXIS_LX_NEG) * BIT_15;
    int16_t ly_report = hid_axis(gamepad_ly, GAMEPAD_AXIS_LY, GAMEPAD_AXIS_LY_NEG) * BIT_15;
    int16_t rx_report = hid_axis(gamepad_rx, GAMEPAD_AXIS_RX, GAMEPAD_AXIS_RX_NEG) * BIT_15;
    int16_t ry_report = hid_axis(gamepad_ry, GAMEPAD_AXIS_RY, GAMEPAD_AXIS_RY_NEG) * BIT_15;
    // HID triggers must be also defined as unsigned in the USB descriptor, and has to be manually
    // value-shifted from signed to unsigned here, otherwise Windows is having erratic behavior and
    // inconsistencies between games (not sure if a bug in Windows' DirectInput or TinyUSB).
    int16_t lz_report = ((hid_axis(gamepad_lz, GAMEPAD_AXIS_LZ, 0) * 2) - 1) * BIT_15;
    int16_t rz_report = ((hid_axis(gamepad_rz, GAMEPAD_AXIS_RZ, 0) * 2) - 1) * BIT_15;
    hid_gamepad_custom_report_t report = {
        lx_report,
        ly_report,
        rx_report,
        ry_report,
        lz_report,
        rz_report,
        buttons,
    };
    tud_hid_report(GENERIC_INPUT_ID, &report, sizeof(report));
}

void hid_xinput_report()
{
    int8_t buttons_0 = 0;
    int8_t buttons_1 = 0;
    for (int i = 0; i < 8; i++)
    {
        buttons_0 += state_matrix[GAMEPAD_INDEX + i] << i;
    }
    for (int i = 0; i < 8; i++)
    {
        buttons_1 += state_matrix[GAMEPAD_INDEX + i + 8] << i;
    }
    // Adjust range from [-1,1] to [-32767,32767].
    int16_t lx_report = hid_axis(gamepad_lx, GAMEPAD_AXIS_LX, GAMEPAD_AXIS_LX_NEG) * BIT_15;
    int16_t ly_report = hid_axis(gamepad_ly, GAMEPAD_AXIS_LY, GAMEPAD_AXIS_LY_NEG) * BIT_15;
    int16_t rx_report = hid_axis(gamepad_rx, GAMEPAD_AXIS_RX, GAMEPAD_AXIS_RX_NEG) * BIT_15;
    int16_t ry_report = hid_axis(gamepad_ry, GAMEPAD_AXIS_RY, GAMEPAD_AXIS_RY_NEG) * BIT_15;
    // Adjust range from [0,1] to [0,255].
    uint16_t lz_report = hid_axis(gamepad_lz, GAMEPAD_AXIS_LZ, 0) * BIT_8;
    uint16_t rz_report = hid_axis(gamepad_rz, GAMEPAD_AXIS_RZ, 0) * BIT_8;
    xinput_report report = {
        .report_id = 0,
        .report_size = XINPUT_REPORT_SIZE,
        .buttons_0 = buttons_0,
        .buttons_1 = buttons_1,
        .lz = lz_report,
        .rz = rz_report,
        .lx = lx_report,
        .ly = -ly_report,
        .rx = rx_report,
        .ry = -ry_report,
        .reserved = {0, 0, 0, 0, 0, 0}};
    xinput_send_report(&report);
}

// void hid_switch_pro_report_simple()
// {
//     // Adjust range from [-1,1] to [0,65535].
//     uint16_t lx_report = (hid_axis(gamepad_lx, GAMEPAD_AXIS_LX, GAMEPAD_AXIS_LX_NEG) + 1) * BIT_15;
//     uint16_t ly_report = (hid_axis(gamepad_ly, GAMEPAD_AXIS_LY, GAMEPAD_AXIS_LY_NEG) + 1) * BIT_15;
//     uint16_t rx_report = (hid_axis(gamepad_rx, GAMEPAD_AXIS_RX, GAMEPAD_AXIS_RX_NEG) + 1) * BIT_15;
//     uint16_t ry_report = (hid_axis(gamepad_ry, GAMEPAD_AXIS_RY, GAMEPAD_AXIS_RY_NEG) + 1) * BIT_15;
//     // Adjust range from [0,1] to [0,255].
//     // uint16_t lz_report = hid_axis(gamepad_lz, GAMEPAD_AXIS_LZ, 0) * BIT_8;
//     // uint16_t rz_report = hid_axis(gamepad_rz, GAMEPAD_AXIS_RZ, 0) * BIT_8;
//     // switchProInputReport63_t report = {0};
//     // report.btnA = state_matrix[GAMEPAD_A];
//     // report.btnB = state_matrix[GAMEPAD_B];
//     // report.btnX = state_matrix[GAMEPAD_X];
//     // report.btnY = state_matrix[GAMEPAD_Y];
//     // report.btnL = state_matrix[GAMEPAD_L1];
//     // report.btnR = state_matrix[GAMEPAD_R1];
//     // report.btnZL = hid_axis(gamepad_lz, GAMEPAD_AXIS_LZ, 0);
//     // report.btnZR = hid_axis(gamepad_rz, GAMEPAD_AXIS_RZ, 0);
//     // report.btnMinus = state_matrix[GAMEPAD_SELECT];
//     // report.btnPlus = state_matrix[GAMEPAD_START];
//     // report.btnLS = state_matrix[GAMEPAD_L3];
//     // report.btnRS = state_matrix[GAMEPAD_R3];
//     // report.btnHome = state_matrix[GAMEPAD_HOME];
//     // report.btnCapture = 0;
//     // report.hat = dpad_button_to_hat_switch_8(
//     //     state_matrix[GAMEPAD_UP],
//     //     state_matrix[GAMEPAD_DOWN],
//     //     state_matrix[GAMEPAD_LEFT],
//     //     state_matrix[GAMEPAD_RIGHT]
//     // );
//     // report.X = lx_report;
//     // report.Y = ly_report;
//     // report.Z = rx_report;
//     // report.Rz = ry_report;

//     switchProUsbInputReport30_t report = {0};
//     report.reportId = SWITCH_PRO_INPUT_ID_FULL_CONTROLLER_STATE;
//     report.ButtonA = state_matrix[GAMEPAD_A];
//     report.ButtonB = state_matrix[GAMEPAD_B];
//     report.ButtonX = state_matrix[GAMEPAD_X];
//     report.ButtonY = state_matrix[GAMEPAD_Y];
//     report.ButtonL = state_matrix[GAMEPAD_L1];
//     report.ButtonR = state_matrix[GAMEPAD_R1];
//     report.ButtonZL = hid_axis(gamepad_lz, GAMEPAD_AXIS_LZ, 0);
//     report.ButtonZR = hid_axis(gamepad_rz, GAMEPAD_AXIS_RZ, 0);
//     report.ButtonMinus = state_matrix[GAMEPAD_SELECT];
//     report.ButtonPlus = state_matrix[GAMEPAD_START];
//     report.ButtonLS = state_matrix[GAMEPAD_L3];
//     report.ButtonRS = state_matrix[GAMEPAD_R3];
//     report.ButtonHome = state_matrix[GAMEPAD_HOME];
//     report.ButtonCapture = 0;
//     report.X = lx_report;
//     report.Y = ly_report;
//     report.Z = rx_report;
//     report.Rz = ry_report;
//     report.HatSwitch = dpad_button_to_hat_switch_8(
//         state_matrix[GAMEPAD_UP],
//         state_matrix[GAMEPAD_DOWN],
//         state_matrix[GAMEPAD_LEFT],
//         state_matrix[GAMEPAD_RIGHT]);

//     const uint8_t *u8_report = (const uint8_t *)&report;
//     tud_hid_report(u8_report[0], &u8_report[1], sizeof(report) - 1);
// }

void switch_pro_gamepad_data_update()
{
    // Adjust range from [-1,1] to [0,4095].
    uint16_t lx_report = (hid_axis(gamepad_lx, GAMEPAD_AXIS_LX, GAMEPAD_AXIS_LX_NEG) + 1) * BIT_11;
    uint16_t ly_report = (hid_axis(gamepad_ly, GAMEPAD_AXIS_LY, GAMEPAD_AXIS_LY_NEG) + 1) * BIT_11;
    uint16_t rx_report = (hid_axis(gamepad_rx, GAMEPAD_AXIS_RX, GAMEPAD_AXIS_RX_NEG) + 1) * BIT_11;
    uint16_t ry_report = (hid_axis(gamepad_ry, GAMEPAD_AXIS_RY, GAMEPAD_AXIS_RY_NEG) + 1) * BIT_11;
    // Adjust range from [0,1] to [0,255].
    // uint16_t lz_report = hid_axis(gamepad_lz, GAMEPAD_AXIS_LZ, 0) * BIT_8;
    // uint16_t rz_report = hid_axis(gamepad_rz, GAMEPAD_AXIS_RZ, 0) * BIT_8;
    switchProUsb.gamepad_data.id = SWITCH_PRO_USB_INPUT_ID_FULL_CONTROLLER_STATE;
    switchProUsb.gamepad_data.controller_data.button.A = state_matrix[GAMEPAD_A];
    switchProUsb.gamepad_data.controller_data.button.B = state_matrix[GAMEPAD_B];
    switchProUsb.gamepad_data.controller_data.button.X = state_matrix[GAMEPAD_X];
    switchProUsb.gamepad_data.controller_data.button.Y = state_matrix[GAMEPAD_Y];
    switchProUsb.gamepad_data.controller_data.button.L = state_matrix[GAMEPAD_L1];
    switchProUsb.gamepad_data.controller_data.button.R = state_matrix[GAMEPAD_R1];
    switchProUsb.gamepad_data.controller_data.button.ZL = hid_axis(gamepad_lz, GAMEPAD_AXIS_LZ, 0);
    switchProUsb.gamepad_data.controller_data.button.ZR = hid_axis(gamepad_rz, GAMEPAD_AXIS_RZ, 0);
    switchProUsb.gamepad_data.controller_data.button.MINUS = state_matrix[GAMEPAD_SELECT];
    switchProUsb.gamepad_data.controller_data.button.PLUS = state_matrix[GAMEPAD_START];
    switchProUsb.gamepad_data.controller_data.button.LS = state_matrix[GAMEPAD_L3];
    switchProUsb.gamepad_data.controller_data.button.RS = state_matrix[GAMEPAD_R3];
    switchProUsb.gamepad_data.controller_data.button.HOME = state_matrix[GAMEPAD_HOME];
    switchProUsb.gamepad_data.controller_data.button.CAPTURE = 0;
    switchProUsb.gamepad_data.controller_data.button.DPAD_UP = state_matrix[GAMEPAD_UP];
    switchProUsb.gamepad_data.controller_data.button.DPAD_DOWN = state_matrix[GAMEPAD_DOWN];
    switchProUsb.gamepad_data.controller_data.button.DPAD_LEFT = state_matrix[GAMEPAD_LEFT];
    switchProUsb.gamepad_data.controller_data.button.DPAD_RIGHT = state_matrix[GAMEPAD_RIGHT];
    switchProUsb.gamepad_data.controller_data.left_stick.X = lx_report;
    switchProUsb.gamepad_data.controller_data.left_stick.Y = ly_report;
    switchProUsb.gamepad_data.controller_data.right_stick.X = rx_report;
    switchProUsb.gamepad_data.controller_data.right_stick.Y = ry_report;

    // static uint8_t packetTimer = 0;
    // if (packetTimer < BIT_8)
    //     packetTimer += 3;
    // else packetTimer = 0;
    // report.timer = packetTimer;
    // report.controller_state.power = 0;
    // report.controller_state.battery = 2;

    switchProUsb.handle_input_0x30(&switchProUsb);
    if (switchProUsb.hid_report_open)
        tud_hid_report(switchProUsb.hid_report_buffer[0], &switchProUsb.hid_report_buffer[1], SWITCH_PRO_USB_REPORT_SIZE);
    // const uint8_t *u8_report = (const uint8_t *)&report;
    // tud_hid_report(u8_report[0], &u8_report[1], SWITCH_PRO_USB_INPUT_REPORT_FULL_SIZE);
}

void hid_switch_pro_report(uint8_t *hid_rx_buffer, uint8_t len)
{
    switchProUsb.do_work(&switchProUsb, hid_rx_buffer, len);
    if (switchProUsb.hid_report_open)
    {
        tud_task();
        if (tud_ready())
            if (tud_hid_ready())
                tud_hid_report(switchProUsb.hid_report_buffer[0], &switchProUsb.hid_report_buffer[1], SWITCH_PRO_USB_REPORT_SIZE);
    }
}

// void hid_xbox1914_report()
// {
//     // Adjust range from [-1,1] to [0,65535].
//     uint16_t lx_report = (hid_axis(gamepad_lx, GAMEPAD_AXIS_LX, GAMEPAD_AXIS_LX_NEG) + 1) * BIT_15;
//     uint16_t ly_report = (hid_axis(gamepad_ly, GAMEPAD_AXIS_LY, GAMEPAD_AXIS_LY_NEG) + 1) * BIT_15;
//     uint16_t rx_report = (hid_axis(gamepad_rx, GAMEPAD_AXIS_RX, GAMEPAD_AXIS_RX_NEG) + 1) * BIT_15;
//     uint16_t ry_report = (hid_axis(gamepad_ry, GAMEPAD_AXIS_RY, GAMEPAD_AXIS_RY_NEG) + 1) * BIT_15;
//     // Adjust range from [0,1] to [0,255].
//     uint16_t lz_report = hid_axis(gamepad_lz, GAMEPAD_AXIS_LZ, 0) * BIT_8;
//     uint16_t rz_report = hid_axis(gamepad_rz, GAMEPAD_AXIS_RZ, 0) * BIT_8;

//     xbox1914BtInputReport01_t report = {0};
//     report.reportId = XBOX_1914_INPUT_ID01;
//     report.X = lx_report;
//     report.Y = ly_report;
//     report.Z = rx_report;
//     report.Rz = ry_report;
//     report.Brake = lz_report;
//     report.Accelerator = rz_report;
//     report.HatSwitch = dpad_button_to_hat_switch_0(
//         state_matrix[GAMEPAD_UP],
//         state_matrix[GAMEPAD_DOWN],
//         state_matrix[GAMEPAD_LEFT],
//         state_matrix[GAMEPAD_RIGHT]);
//     report.ButtonA = state_matrix[GAMEPAD_A];
//     report.ButtonB = state_matrix[GAMEPAD_B];
//     report.ButtonX = state_matrix[GAMEPAD_X];
//     report.ButtonY = state_matrix[GAMEPAD_Y];
//     report.ButtonLB = state_matrix[GAMEPAD_L1];
//     report.ButtonRB = state_matrix[GAMEPAD_R1];
//     report.ButtonBack = state_matrix[GAMEPAD_SELECT];
//     report.ButtonStart = state_matrix[GAMEPAD_START];
//     report.ButtonXbox = state_matrix[GAMEPAD_HOME];
//     report.ButtonLS = state_matrix[GAMEPAD_L3];
//     report.ButtonRS = state_matrix[GAMEPAD_R3];

//     const uint8_t *u8_report = (const uint8_t *)&report;
//     tud_hid_report(u8_report[0], &u8_report[1], sizeof(report) - 1);
// }

void hid_dual_shock_4_report()
{
    // Adjust range from [-1,1] to [0,255].
    uint8_t lx_report = (hid_axis(gamepad_lx, GAMEPAD_AXIS_LX, GAMEPAD_AXIS_LX_NEG) + 1) * BIT_7;
    uint8_t ly_report = (hid_axis(gamepad_ly, GAMEPAD_AXIS_LY, GAMEPAD_AXIS_LY_NEG) + 1) * BIT_7;
    uint8_t rx_report = (hid_axis(gamepad_rx, GAMEPAD_AXIS_RX, GAMEPAD_AXIS_RX_NEG) + 1) * BIT_7;
    uint8_t ry_report = (hid_axis(gamepad_ry, GAMEPAD_AXIS_RY, GAMEPAD_AXIS_RY_NEG) + 1) * BIT_7;
    // Adjust range from [0,1] to [0,255].
    uint16_t lz_report = hid_axis(gamepad_lz, GAMEPAD_AXIS_LZ, 0) * BIT_8;
    uint16_t rz_report = hid_axis(gamepad_rz, GAMEPAD_AXIS_RZ, 0) * BIT_8;

    struct DualShock4_ReportIn01USB report = {0};
    report.ReportID = DUAL_SHOCK_4_REPORT_INPUT_ID01;
    report.State.State.State.LeftStickX = lx_report;
    report.State.State.State.LeftStickY = ly_report;
    report.State.State.State.RightStickX = rx_report;
    report.State.State.State.RightStickY = ry_report;
    report.State.State.State.DPad = dpad_button_to_hat_switch_8(
        state_matrix[GAMEPAD_UP],
        state_matrix[GAMEPAD_DOWN],
        state_matrix[GAMEPAD_LEFT],
        state_matrix[GAMEPAD_RIGHT]);
    report.State.State.State.ButtonSquare = state_matrix[GAMEPAD_Y];
    report.State.State.State.ButtonCross = state_matrix[GAMEPAD_B];
    report.State.State.State.ButtonCircle = state_matrix[GAMEPAD_A];
    report.State.State.State.ButtonTriangle = state_matrix[GAMEPAD_X];
    report.State.State.State.ButtonL1 = state_matrix[GAMEPAD_L1];
    report.State.State.State.ButtonR1 = state_matrix[GAMEPAD_R1];
    report.State.State.State.ButtonL2 = lz_report > 0;
    report.State.State.State.ButtonR2 = rz_report > 0;
    report.State.State.State.ButtonShare = state_matrix[GAMEPAD_SELECT];
    report.State.State.State.ButtonOptions = state_matrix[GAMEPAD_START];
    report.State.State.State.ButtonL3 = state_matrix[GAMEPAD_L3];
    report.State.State.State.ButtonR3 = state_matrix[GAMEPAD_R3];
    report.State.State.State.ButtonHome = state_matrix[GAMEPAD_HOME];
    report.State.State.State.TriggerLeft = lz_report;
    report.State.State.State.TriggerRight = rz_report;

    report.State.State.AngularVelocityX = (int16_t)(-gamepad_gyro.y / 1.9);
    report.State.State.AngularVelocityZ = (int16_t)(-gamepad_gyro.x / 1.9);
    report.State.State.AngularVelocityY = (int16_t)(gamepad_gyro.z / 1.9);
    report.State.State.AccelerometerX = (int16_t)(-gamepad_accel.z / 1.9);
    report.State.State.AccelerometerY = (int16_t)(-gamepad_accel.x / 1.9);
    report.State.State.AccelerometerZ = (int16_t)(gamepad_accel.y / 1.9);

    const uint8_t *u8_report = (const uint8_t *)&report;
    tud_hid_report(u8_report[0], &u8_report[1], sizeof(report) - 1);
}

void hid_dual_sense_report()
{
    // Adjust range from [-1,1] to [0,255].
    uint8_t lx_report = (hid_axis(gamepad_lx, GAMEPAD_AXIS_LX, GAMEPAD_AXIS_LX_NEG) + 1) * BIT_7;
    uint8_t ly_report = (hid_axis(gamepad_ly, GAMEPAD_AXIS_LY, GAMEPAD_AXIS_LY_NEG) + 1) * BIT_7;
    uint8_t rx_report = (hid_axis(gamepad_rx, GAMEPAD_AXIS_RX, GAMEPAD_AXIS_RX_NEG) + 1) * BIT_7;
    uint8_t ry_report = (hid_axis(gamepad_ry, GAMEPAD_AXIS_RY, GAMEPAD_AXIS_RY_NEG) + 1) * BIT_7;
    // Adjust range from [0,1] to [0,255].
    uint8_t lz_report = hid_axis(gamepad_lz, GAMEPAD_AXIS_LZ, 0) * BIT_8;
    uint8_t rz_report = hid_axis(gamepad_rz, GAMEPAD_AXIS_RZ, 0) * BIT_8;

    struct DualSense_ReportIn01USB report = {0};
    report.ReportID = DUAL_SENSE_USB_REPORT_INPUT_ID01;
    report.State.LeftStickX = lx_report;
    report.State.LeftStickY = ly_report;
    report.State.RightStickX = rx_report;
    report.State.RightStickY = ry_report;
    report.State.TriggerLeft = lz_report;
    report.State.TriggerRight = rz_report;
    report.State.DPad = dpad_button_to_hat_switch_8(
        state_matrix[GAMEPAD_UP],
        state_matrix[GAMEPAD_DOWN],
        state_matrix[GAMEPAD_LEFT],
        state_matrix[GAMEPAD_RIGHT]);
    report.State.ButtonSquare = state_matrix[GAMEPAD_Y];
    report.State.ButtonCross = state_matrix[GAMEPAD_B];
    report.State.ButtonCircle = state_matrix[GAMEPAD_A];
    report.State.ButtonTriangle = state_matrix[GAMEPAD_X];
    report.State.ButtonL1 = state_matrix[GAMEPAD_L1];
    report.State.ButtonR1 = state_matrix[GAMEPAD_R1];
    report.State.ButtonL2 = lz_report > 0;
    report.State.ButtonR2 = rz_report > 0;
    report.State.ButtonCreate = state_matrix[GAMEPAD_SELECT];
    report.State.ButtonOptions = state_matrix[GAMEPAD_START];
    report.State.ButtonL3 = state_matrix[GAMEPAD_L3];
    report.State.ButtonR3 = state_matrix[GAMEPAD_R3];
    report.State.ButtonHome = state_matrix[GAMEPAD_HOME];

    report.State.AngularVelocityX = (int16_t)(-gamepad_gyro.y / 1.9);
    report.State.AngularVelocityZ = (int16_t)(-gamepad_gyro.x / 1.9);
    report.State.AngularVelocityY = (int16_t)(gamepad_gyro.z / 1.9);
    report.State.AccelerometerX = (int16_t)(-gamepad_accel.z / 1.9);
    report.State.AccelerometerY = (int16_t)(-gamepad_accel.x / 1.9);
    report.State.AccelerometerZ = (int16_t)(gamepad_accel.y / 1.9);

    const uint8_t *u8_report = (const uint8_t *)&report;
    tud_hid_report(u8_report[0], &u8_report[1], sizeof(report) - 1);
}

void hid_gamepad_reset()
{
    gamepad_lx = 0;
    gamepad_ly = 0;
    gamepad_rx = 0;
    gamepad_ry = 0;
    gamepad_lz = 0;
    gamepad_rz = 0;

    gamepad_gyro = (Vector){0};
    gamepad_accel = (Vector){0};
}

// 人机驱动设备上报信息
void hid_report()
{
    static bool is_tud_ready = false;
    static bool is_tud_ready_logged = false;
    static uint8_t priority_mouse = 0;
    static uint8_t priority_gamepad = 0;
    // synced_gamepad = false;

    // Not all events are sent everytime, they are delivered based on their
    // priority ratio and how long they have been queueing.
    // For example thumbstick movement may be queued for some cycles if there
    // is a lot of mouse data being sent.
    if (!synced_mouse)
        priority_mouse += 1 * CFG_HID_REPORT_PRIORITY_RATIO;
    if (!synced_gamepad)
        priority_gamepad += 1;

    if (!hid_allow_communication)
        return;
    tud_task();
    if (tud_ready())
    {
        is_tud_ready = true;
        if (!is_tud_ready_logged)
        {
            is_tud_ready_logged = true;
            info("USB: tud_ready TRUE\n");
        }
        if (tud_hid_ready())
        {
            // 当前协议兼容 WebUSB ，与inputs lab的网页上位机连接，读取上位机对手柄的修改
            if (current_protocol_compatible_with_webusb())
            {
                webusb_read();      // 读取 config 、status或者...
                webusb_flush();     // ensure webusb data transfer is complete.
            }
            // 键盘同步，发送 modifier 值、report 数组，当在使用键盘，有按下、松开动作的时候，都会同步键盘
            if (!synced_keyboard)
            {
                // report 键盘信息，tud_hid_keyboard_report函数上报信息，
                hid_keyboard_report();
                synced_keyboard = true;
            }
            // 鼠标信息还没同步，且鼠标优先级计数高于手柄按键，当在使用键盘，且鼠标优先级高于键盘时，有按下、松开动作的时候，都会同步鼠标
            else if (!synced_mouse && (priority_mouse > priority_gamepad))
            {
                // report 鼠标信息，返回 report 数组
                hid_mouse_report();
                synced_mouse = true;
                priority_mouse = 0;
            }
            // 手柄按键信息还没同步，且按键优先级高于鼠标时，在有按键按下、松开，摇杆运动时，都会同步游戏手柄的数据信息
            else if (!synced_gamepad)
            {
                // 原生按键协议
                if (config_get_protocol() == PROTOCOL_GENERIC)
                {
                    // report 按键信息，返回 report 数组
                    hid_gamepad_report();
                    synced_gamepad = true;
                    priority_gamepad = 0;
                }
                // switch pro 协议
                else if (config_get_protocol() == PROTOCOL_SWITCH_PRO)
                {
                    // report switch 按键信息，返回 report 数组
                    switch_pro_gamepad_data_update();
                    synced_gamepad = true;
                    priority_gamepad = 0;
                }
                // else if (config_get_protocol() == PROTOCOL_XBOX_1914) {
                //     // hid_xbox1914_report();
                //     synced_gamepad = true;
                //     priority_gamepad = 0;
                // }
                // dual shock 4 协议
                else if (config_get_protocol() == PROTOCOL_DUAL_SHOCK_4)
                {
                    // report PS4 dualshock 4 按键信息，返回 u8_report 数组
                    hid_dual_shock_4_report();
                    synced_gamepad = true;
                    priority_gamepad = 0;
                }
                // dual sense 协议
                else if (config_get_protocol() == PROTOCOL_DUAL_SENSE)
                {
                    hid_dual_sense_report();
                    synced_gamepad = true;
                    priority_gamepad = 0;
                }
            }
        }
        // xinput 协议
        if (!synced_gamepad)
        {
            if (config_current_protocol_is_xusb())
            {
                if (tud_suspended())
                {
                    tud_remote_wakeup();
                }
                hid_xinput_report();
                priority_gamepad = 0;
            }
        }
        // Gamepad values being reset so potentially unsent values are not
        // aggregated with the next cycle.
        hid_gamepad_reset();
    }
    else
    {
        is_tud_ready = false;
        if (is_tud_ready_logged)
        {
            is_tud_ready_logged = false;
            info("USB: tud_ready FALSE\n");
        }
    }
}

// A not-so-secret easter egg.
void hid_thanks_(alarm_id_t alarm)
{
    cancel_alarm(alarm);
    static uint8_t x = 0;
    static bool p = 0;
    static uint8_t r;
    if (x == 0 && p == false)
    {
        r = random8() % thanks_len;
    }
    if (thanks_list[r][x] == 0)
    {
        x = 0;
        p = 0;
        return;
    }
    if (!p)
    {
        hid_press(thanks_list[r][x]);
        p = true;
    }
    else if (p)
    {
        hid_release(thanks_list[r][x]);
        p = false;
        x += 1;
    }
    add_alarm_in_ms(5, (alarm_callback_t)hid_thanks_, NULL, true);
}

void hid_thanks()
{
    add_alarm_in_ms(5, (alarm_callback_t)hid_thanks_, NULL, true);
}

/* 调用 alarm_pool_create 的主要目的是为了初始化与 HID（人机接口设备）相关的警报（alarm）管理机制。
    通过创建一个专门的警报池（alarm pool）结构，
    来为后续可能涉及到的基于定时器的警报处理、事件触发等操作做好准备工作，
    以确保 HID 设备相关功能能够准确、及时地响应各种定时相关的情况。
 */
void hid_init()
{
    info("INIT: HID\n");
    alarm_pool = alarm_pool_create(2, 255);
    switchProUsb = SwitchProUsb_();
}
