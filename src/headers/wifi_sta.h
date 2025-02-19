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
// 定义全局变量
extern bool hid_allow_communication = true;
extern bool synced_keyboard = false;
extern bool synced_mouse = false;
extern bool synced_gamepad = false;

extern uint16_t alarms = 0;
extern alarm_pool_t *alarm_pool;

extern uint8_t state_matrix[256] = {
    0,
};

extern int16_t mouse_x = 0;
extern int16_t mouse_y = 0;

extern double gamepad_lx = 0;
extern double gamepad_ly = 0;
extern double gamepad_rx = 0;
extern double gamepad_ry = 0;
extern double gamepad_lz = 0;
extern double gamepad_rz = 0;

extern Vector gamepad_gyro = 0;
extern Vector gamepad_accel = 0;

extern SwitchProUsb switchProUsb;

bool sendCMD(const char *cmd, const char *act);
void connectToWifi();

void send_data_to_esp8285(uint8_t *data, int data_size);
void sendPacketOverWiFi(DataPacket packet);


