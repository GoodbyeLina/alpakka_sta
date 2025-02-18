#pragma once

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


// 定义结构体来组织数据
typedef struct {
    bool synced_keyboard;
    bool synced_mouse;
    bool synced_gamepad;
    uint8_t state_matrix[256];
} DataPacket;

// 在这里定义矩阵大小常量（如果还没有定义的话）
#ifndef MATRIX_ROWS
#define MATRIX_ROWS 6
#endif
#ifndef MATRIX_COLS
#define MATRIX_COLS 14
#endif

// 声明全局变量
extern bool wifi_comm_allowed;
extern bool keyboard_synced;
extern bool mouse_synced;
extern bool gamepad_synced;

extern uint8_t current_alarms;
extern uint8_t alarm_pool_status;

extern uint8_t wifi_matrix_copy[MATRIX_ROWS][MATRIX_COLS];

extern int16_t mouse_pos_x;
extern int16_t mouse_pos_y;

extern int16_t gamepad_left_x;
extern int16_t gamepad_left_y;
extern int16_t gamepad_right_x;
extern int16_t gamepad_right_y;
extern int16_t gamepad_left_z;
extern int16_t gamepad_right_z;

extern int16_t gamepad_gyro_data;
extern int16_t gamepad_accel_data;

extern bool switch_pro_usb_status;

bool sendCMD(const char *cmd, const char *act);
void connectToWifi();

void send_data_to_esp8285(uint8_t *data, int data_size);
void sendPacketOverWiFi(DataPacket packet);


