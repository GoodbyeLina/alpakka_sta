#include <stdio.h>
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
#include "wifi_sta.h"
#include "transfer.h"

// Initializing transfer structure
transfer_struct transfer = {
    .wifi_allow_communication = true,   // Initialize wifi allow communication flag
                                         // (depending upon your specific initialization needs)
    .synced_keyboard = false,            // Initialize synced keyboard flag
    .synced_mouse = false,               // Initialize synced mouse flag
    .synced_gamepad = false,             // Initialize synced gamepad flag
    .alarms = 0,                         // Initialize alarms to 0 (depending upon your specific initialization needs)
    .alarm_pool = NULL,                  // Assuming you don't need an alarm pool for this struct initially.
    .wifi_matrix[256]= {                     // Initialize wifi matrix array with all elements as 0 or the default value of uint8_t
        0                     // Fill in other values if needed (up to maximum size)
    },
    .mouse_x = 0,                        // Initialize mouse x position to 0
    .mouse_y = 0,                        // Initialize mouse y position to 0.
    .gamepad_lx = 0.0,                   // Initialize gamepad left x-axis value to 0.0
    .gamepad_ly = 0.0,                   // Initialize gamepad left y-axis value to 0.0
    // Similarly initialize other members of the transfer structure...
    .gamepad_rx = 0.0,
    .gamepad_ry = 0.0,
    .gamepad_lz = 0.0,
    .gamepad_rz = 0.0,

    .gamepad_gyro = {0},   // Assuming this is a custom struct or typedef.
    .gamepad_accel = {0}  // Assuming this is a custom struct or typedef.

};

void wifi_matrix_reset(uint8_t keep)
{
    for (uint8_t action = 0; action < 255; action++)
    {
        if (action == keep)
            continue; // Optionally do not reset specific actions.
            transfer.wifi_matrix[action] = 0;
    }
    transfer.synced_keyboard = false;
    transfer.synced_mouse = false;
    transfer.synced_gamepad = false;
}



void wifi_press(uint8_t key)
{
    if (key == KEY_NONE)
        return;
    // else if (key >= PROC_INDEX)
        // wifi_procedure_press(key);
    else
    {
        // 根据 key 的范围更新相应设备的同步标志
        transfer.wifi_matrix[key] += 1;
        if (key >= GAMEPAD_INDEX)
            transfer.synced_gamepad = false;
        else if (key >= MOUSE_INDEX)
            transfer.synced_mouse = false;
        else
            transfer.synced_keyboard = false;
    }
    sendPacketOverWiFi(transfer);;
}

void wifi_press_multiple(uint8_t *keys)
{
    for (uint8_t i = 0; i < ACTIONS_LEN; i++)
    {
        if (keys[i] == 0)
            return;
        wifi_press(keys[i]);
    }
}

// 调用 wifi_procedure_release，实现按下动作
void wifi_release(uint8_t key)
{
    if (key == KEY_NONE)
        return;
    else if (key == MOUSE_SCROLL_UP)
        return;
    else if (key == MOUSE_SCROLL_DOWN)
        return;
    // else if (key >= PROC_INDEX)
        // wifi_procedure_release(key);
    else
    {
        if (transfer.wifi_matrix[key] > 0)
        { // Do not allow to wrap / go negative.
            transfer.wifi_matrix[key] -= 1;
            if (key >= GAMEPAD_INDEX)
                transfer.synced_gamepad = false;
            else if (key >= MOUSE_INDEX)
                transfer.synced_mouse = false;
            else
                transfer.synced_keyboard = false;
            }
        sendPacketOverWiFi(transfer);;

    }
}

void wifi_release_multiple(uint8_t *keys)
{
    for (uint8_t i = 0; i < ACTIONS_LEN; i++)
    {
        if (keys[i] == 0)
            return;
        wifi_release(keys[i]);
    }
}


bool wifi_is_axis(uint8_t key)
{
    return is_between(key, GAMEPAD_AXIS_INDEX, PROC_INDEX - 1);
}

void wifi_gamepad_lx(double value)
{
    if (value == transfer.gamepad_lx)
        return;
        transfer.gamepad_lx += value; // Multiple inputs can be combined.
        transfer.synced_gamepad = false;
        sendPacketOverWiFi(transfer);;

}

void wifi_gamepad_ly(double value)
{
    if (value == transfer.gamepad_ly)
        return;
    transfer.gamepad_ly += value; // Multiple inputs can be combined.
    transfer.synced_gamepad = false;
    sendPacketOverWiFi(transfer);;

}

void wifi_gamepad_lz(double value)
{
    if (value == transfer.gamepad_lz)
        return;
        transfer.gamepad_lz += value; // Multiple inputs can be combined.
    transfer.synced_gamepad = false;
    sendPacketOverWiFi(transfer);;

}

void wifi_gamepad_rx(double value)
{
    if (value == transfer.gamepad_rx)
        return;
        transfer.gamepad_rx += value; // Multiple inputs can be combined.
    transfer.synced_gamepad = false;
    sendPacketOverWiFi(transfer);;

}

void wifi_gamepad_ry(double value)
{
    if (value == transfer.gamepad_ry)
        return;
        transfer.gamepad_ry += value; // Multiple inputs can be combined.
    transfer.synced_gamepad = false;
    sendPacketOverWiFi(transfer);;

}

void wifi_gamepad_rz(double value)
{
    if (value == transfer.gamepad_rz)
        return;
        transfer.gamepad_rz += value; // Multiple inputs can be combined.
    transfer.synced_gamepad = false;
    sendPacketOverWiFi(transfer);;

}

bool wifi_is_mouse_move(uint8_t key)
{
    return is_between(key, MOUSE_X, MOUSE_Y_NEG);
}

void wifi_mouse_move(int16_t x, int16_t y)
{
    transfer.mouse_x += x;
    transfer.mouse_y += y;
    transfer.synced_mouse = false;
    sendPacketOverWiFi(transfer);;

}

void wifi_press_later(uint8_t key, uint16_t delay)
{
    alarm_pool_add_alarm_in_ms(
        transfer.alarm_pool,
        delay,
        (alarm_callback_t)wifi_press_later_callback,
        (void *)(uint32_t)key,
        true);
    transfer.alarms++;
}

void wifi_release_later(uint8_t key, uint16_t delay)
{
    alarm_pool_add_alarm_in_ms(
        transfer.alarm_pool,
        delay,
        (alarm_callback_t)wifi_release_later_callback,
        (void *)(uint32_t)key,
        true);
        transfer.alarms++;
    }

void wifi_press_multiple_later(uint8_t *keys, uint16_t delay)
{
    alarm_pool_add_alarm_in_ms(
        transfer.alarm_pool,
        delay,
        (alarm_callback_t)wifi_press_multiple_later_callback,
        keys,
        true);
        transfer.alarms++;
    }

void wifi_release_multiple_later(uint8_t *keys, uint16_t delay)
{
    alarm_pool_add_alarm_in_ms(
        transfer.alarm_pool,
        delay,
        (alarm_callback_t)wifi_release_multiple_later_callback,
        keys,
        true);
        transfer.alarms++;
    }

void wifi_press_later_callback(alarm_id_t alarm, uint8_t key)
{
    alarm_pool_cancel_alarm(transfer.alarm_pool, alarm);
    wifi_press(key);
    transfer.alarms++;
}

void wifi_release_later_callback(alarm_id_t alarm, uint8_t key)
{
    alarm_pool_cancel_alarm(transfer.alarm_pool, alarm);
    wifi_release(key);
    transfer.alarms++;
    sendPacketOverWiFi(transfer);;

}

void wifi_press_multiple_later_callback(alarm_id_t alarm, uint8_t *keys)
{
    alarm_pool_cancel_alarm(transfer.alarm_pool, alarm);
    wifi_press_multiple(keys);
    transfer.alarms++;
    sendPacketOverWiFi(transfer);;

}

void wifi_release_multiple_later_callback(alarm_id_t alarm, uint8_t *keys)
{
    alarm_pool_cancel_alarm(transfer.alarm_pool, alarm);
    wifi_release_multiple(keys);
    transfer.alarms++;
    sendPacketOverWiFi(transfer);;

}

void wifi_macro(uint8_t index)
{
    uint8_t section = SECTION_MACRO_1 + ((index - 1) / 2);
    uint8_t subindex = (index - 1) % 2;
    CtrlProfile *profile = config_profile_read(profile_get_active_index(false));
    uint8_t *macro = profile->sections[section].macro.macro[subindex];
    if (transfer.alarms > 0)
        return; // Disallows parallel macros. TODO fix.
    uint16_t time = 10;
    for (uint8_t i = 0; i < 28; i++)
    {
        if (macro[i] == 0)
            break;
        wifi_press_later(macro[i], time);
        time += 10;
        wifi_release_later(macro[i], time);
        time += 10;
    }
}

void wifi_gamepad_gyro(double x, double y, double z)
{
    bool b = false;
    if (transfer.gamepad_gyro.x != x)
    {
        transfer.gamepad_gyro.x = x;
        b = true;
    }
    if (transfer.gamepad_gyro.y != y)
    {
        transfer.gamepad_gyro.y = y;
        b = true;
    }
    if (transfer.gamepad_gyro.z != z)
    {
        transfer.gamepad_gyro.z = z;
        b = true;
    }
    if (b)
    {
        transfer.synced_gamepad = false;
        // synced_gamepad_gyroscope = false;
    };
    sendPacketOverWiFi(transfer);;

}

void wifi_gamepad_accel(double x, double y, double z)
{
    bool b = false;
    if (transfer.gamepad_accel.x != x)
    {
        transfer.gamepad_accel.x = x;
        b = true;
    }
    if (transfer.gamepad_accel.y != y)
    {
        transfer.gamepad_accel.y = y;
        b = true;
    }
    if (transfer.gamepad_accel.z != z)
    {
        transfer.gamepad_accel.z = z;
        b = true;
    }
    if (b)
    {
        transfer.synced_gamepad = false;
        // synced_gamepad_gyroscope = false;
    };
    sendPacketOverWiFi(transfer);;

}
