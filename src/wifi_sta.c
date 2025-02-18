#include "pico/stdlib.h"
#include "hardware/uart.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "uart_esp.h"
#include "uart.h"
#include "wifi_sta.h"
#include "led.h"
#include "transfer.h"

// char SSID[] = "HUAWEI-CR18QS";
char SSID[] = "OnePlus Ace 3";
char password[] = "wxy19990308";
char ServerIP[] = "192.168.74.199";
char Port[] = "8080";

// char response[256];
// char ip_address[16];  // 足够存储常见的IP地址格式
char buf[256] = {0};
char uart_command[50] = "";
// int port;

// 定义全局变量
bool wifi_comm_allowed = false;
bool keyboard_synced = false;
bool mouse_synced = false;
bool gamepad_synced = false;

uint8_t current_alarms = 0;
uint8_t alarm_pool_status = 0;

uint8_t wifi_matrix_copy[MATRIX_ROWS][MATRIX_COLS] = {0};

int16_t mouse_pos_x = 0;
int16_t mouse_pos_y = 0;

int16_t gamepad_left_x = 0;
int16_t gamepad_left_y = 0;
int16_t gamepad_right_x = 0;
int16_t gamepad_right_y = 0;
int16_t gamepad_left_z = 0;
int16_t gamepad_right_z = 0;

int16_t gamepad_gyro_data = 0;
int16_t gamepad_accel_data = 0;

bool switch_pro_usb_status = false;

bool sendCMD(const char *cmd, const char *act)
{
    int i = 0;
    uint64_t t = 0;

    uart_puts(UART_ID, cmd);
    uart_puts(UART_ID, "\r\n");

    t = time_us_64();
    while (time_us_64() - t < 2500 * 1000)
    {
        while (uart_is_readable_within_us(UART_ID, 2000))
        {
            buf[i++] = uart_getc(UART_ID);
        }
        if (i > 0)
        {
            buf[i] = '\0';
            printf("%s\r\n", buf);
            if (strstr(buf, act) != NULL)
            {
                return true;
            }
            else
            {
                i = 0;
            }
        }
    }
    //printf("false\r\n");
    return false;
}

void connectToWifi() {
    sendCMD("AT", "OK");
    sendCMD("AT+CWMODE=3", "OK");
    sprintf(uart_command, "AT+CWJAP=\"%s\",\"%s\"", SSID, password);
    sendCMD(uart_command, "OK");

    led_blink_ms(2000);
    // Uncomment to get the IP address of the ESP
    sendCMD("AT+CIFSR", "OK"); // ASK IP

    led_blink_ms(2000);

    sprintf(uart_command, "AT+CIPSTART=\"TCP\",\"%s\",%s", ServerIP, Port);
    sendCMD(uart_command, "OK");
    led_blink_ms(2000);

    // Send data
    sendCMD("AT+CIPMODE=1", "OK");
    led_blink_ms(1000);

}

// 发送数据函数
void send_data_to_esp8285(uint8_t *data, int data_size){

    // send_at_command("AT+CIPSEND");

    for (int i = 0; i < data_size; i++) {
        uart_putc(UART_ID, data[i]);
        // uart_puts(UART_ID, "\r\n");

    }

    // send_at_command("\x1A");

}

// 从ESP8285接收数据的函数
uint8_t* receive_data_from_esp8285(int expected_size) {
    uint8_t* buffer = (uint8_t*)malloc(expected_size);
    if (buffer == NULL) {
        printf("内存分配失败，无法创建接收缓冲区\n");
        return NULL;
    }

    int received = 0;
    while (received < expected_size) {
        if (uart_is_readable(UART_ID)) {
            buffer[received] = uart_getc(UART_ID);
            received++;
        }
    }

    return buffer;
}

// send struct
void sendPacketOverWiFi(transfer_struct packet) {
    // 计算结构体数据总字节数
    size_t data_size = sizeof(packet.wifi_allow_communication) +
                       sizeof(packet.synced_keyboard) +
                       sizeof(packet.synced_mouse) +
                       sizeof(packet.synced_gamepad) +
                       sizeof(packet.alarms) +
                       sizeof(packet.alarm_pool) +
                       sizeof(packet.wifi_matrix) +
                       sizeof(packet.mouse_x) +
                       sizeof(packet.mouse_y) +
                       sizeof(packet.gamepad_lx) +
                       sizeof(packet.gamepad_ly) +
                       sizeof(packet.gamepad_rx) +
                       sizeof(packet.gamepad_ry) +
                       sizeof(packet.gamepad_lz) +
                       sizeof(packet.gamepad_rz) +
                       sizeof(packet.gamepad_gyro) +
                       sizeof(packet.gamepad_accel) +
                       sizeof(packet.switchProUsb);

    // 创建用于存放结构体字节流数据的缓冲区
    uint8_t * buffer = (uint8_t*)malloc(data_size);
    if (buffer == NULL) {
        printf("内存分配失败，无法创建发送缓冲区\n");
        return;
    }

    // 将结构体成员依次复制到缓冲区，先处理布尔类型成员
    memcpy(buffer, &packet.wifi_allow_communication, sizeof(packet.wifi_allow_communication));
    memcpy(buffer + sizeof(packet.wifi_allow_communication), &packet.synced_keyboard, sizeof(packet.synced_keyboard));
    memcpy(buffer + sizeof(packet.wifi_allow_communication) +
                    sizeof(packet.synced_keyboard), &packet.synced_mouse, sizeof(packet.synced_mouse));
    memcpy(buffer + sizeof(packet.wifi_allow_communication) +
                    sizeof(packet.synced_keyboard) +
                    sizeof(packet.synced_mouse), &packet.synced_gamepad, sizeof(packet.synced_gamepad));
    memcpy(buffer + sizeof(packet.wifi_allow_communication) +
                    sizeof(packet.synced_keyboard) +
                    sizeof(packet.synced_mouse) +
                    sizeof(packet.synced_gamepad), &packet.alarms, sizeof(packet.alarms));
    memcpy(buffer + sizeof(packet.wifi_allow_communication) +
                    sizeof(packet.synced_keyboard) +
                    sizeof(packet.synced_mouse) +
                    sizeof(packet.synced_gamepad) +
                    sizeof(packet.alarms), &packet.alarm_pool, sizeof(packet.alarm_pool));
    // 再处理数组类型成员
    memcpy(buffer + sizeof(packet.wifi_allow_communication) +
                    sizeof(packet.synced_keyboard) +
                    sizeof(packet.synced_mouse) +
                    sizeof(packet.synced_gamepad) +
                    sizeof(packet.alarms) +
                    sizeof(packet.alarm_pool), packet.wifi_matrix, sizeof(packet.wifi_matrix));
    memcpy(buffer + sizeof(packet.wifi_allow_communication) +
                    sizeof(packet.synced_keyboard) +
                    sizeof(packet.synced_mouse) +
                    sizeof(packet.synced_gamepad) +
                    sizeof(packet.alarms) +
                    sizeof(packet.alarm_pool) +
                    sizeof(packet.wifi_matrix), &packet.mouse_x, sizeof(packet.mouse_x));
    memcpy(buffer + sizeof(packet.wifi_allow_communication) +
                    sizeof(packet.synced_keyboard) +
                    sizeof(packet.synced_mouse) +
                    sizeof(packet.synced_gamepad) +
                    sizeof(packet.alarms) +
                    sizeof(packet.alarm_pool) +
                    sizeof(packet.wifi_matrix) +
                    sizeof(packet.mouse_x), &packet.mouse_y, sizeof(packet.mouse_y));
    memcpy(buffer + sizeof(packet.wifi_allow_communication) +
                    sizeof(packet.synced_keyboard) +
                    sizeof(packet.synced_mouse) +
                    sizeof(packet.synced_gamepad) +
                    sizeof(packet.alarms) +
                    sizeof(packet.alarm_pool) +
                    sizeof(packet.wifi_matrix) +
                    sizeof(packet.mouse_x) +
                    sizeof(packet.mouse_y), &packet.gamepad_lx, sizeof(packet.gamepad_lx));
    memcpy(buffer + sizeof(packet.wifi_allow_communication) +
                    sizeof(packet.synced_keyboard) +
                    sizeof(packet.synced_mouse) +
                    sizeof(packet.synced_gamepad) +
                    sizeof(packet.alarms) +
                    sizeof(packet.alarm_pool) +
                    sizeof(packet.wifi_matrix) +
                    sizeof(packet.mouse_x) +
                    sizeof(packet.mouse_y) +
                    sizeof(packet.gamepad_lx), &packet.gamepad_ly, sizeof(packet.gamepad_ly));
    memcpy(buffer + sizeof(packet.wifi_allow_communication) +
                    sizeof(packet.synced_keyboard) +
                    sizeof(packet.synced_mouse) +
                    sizeof(packet.synced_gamepad) +
                    sizeof(packet.alarms) +
                    sizeof(packet.alarm_pool) +
                    sizeof(packet.wifi_matrix) +
                    sizeof(packet.mouse_x) +
                    sizeof(packet.mouse_y) +
                    sizeof(packet.gamepad_lx) +
                    sizeof(packet.gamepad_ly), &packet.gamepad_rx, sizeof(packet.gamepad_rx));
    memcpy(buffer + sizeof(packet.wifi_allow_communication) +
                    sizeof(packet.synced_keyboard) +
                    sizeof(packet.synced_mouse) +
                    sizeof(packet.synced_gamepad) +
                    sizeof(packet.alarms) +
                    sizeof(packet.alarm_pool) +
                    sizeof(packet.wifi_matrix) +
                    sizeof(packet.mouse_x) +
                    sizeof(packet.mouse_y) +
                    sizeof(packet.gamepad_lx) +
                    sizeof(packet.gamepad_ly) +
                    sizeof(packet.gamepad_rx), &packet.gamepad_ry, sizeof(packet.gamepad_ry));
    memcpy(buffer + sizeof(packet.wifi_allow_communication) +
                    sizeof(packet.synced_keyboard) +
                    sizeof(packet.synced_mouse) +
                    sizeof(packet.synced_gamepad) +
                    sizeof(packet.alarms) +
                    sizeof(packet.alarm_pool) +
                    sizeof(packet.wifi_matrix) +
                    sizeof(packet.mouse_x) +
                    sizeof(packet.mouse_y) +
                    sizeof(packet.gamepad_lx) +
                    sizeof(packet.gamepad_ly) +
                    sizeof(packet.gamepad_rx) +
                    sizeof(packet.gamepad_ry), &packet.gamepad_lz, sizeof(packet.gamepad_lz));
    memcpy(buffer + sizeof(packet.wifi_allow_communication) +
                    sizeof(packet.synced_keyboard) +
                    sizeof(packet.synced_mouse) +
                    sizeof(packet.synced_gamepad) +
                    sizeof(packet.alarms) +
                    sizeof(packet.alarm_pool) +
                    sizeof(packet.wifi_matrix) +
                    sizeof(packet.mouse_x) +
                    sizeof(packet.mouse_y) +
                    sizeof(packet.gamepad_lx) +
                    sizeof(packet.gamepad_ly) +
                    sizeof(packet.gamepad_rx) +
                    sizeof(packet.gamepad_ry) +
                    sizeof(packet.gamepad_lz), &packet.gamepad_rz, sizeof(packet.gamepad_rz));
    // 复制自定义结构体
    memcpy(buffer + sizeof(packet.wifi_allow_communication) +
                    sizeof(packet.synced_keyboard) +
                    sizeof(packet.synced_mouse) +
                    sizeof(packet.synced_gamepad) +
                    sizeof(packet.alarms) +
                    sizeof(packet.alarm_pool) +
                    sizeof(packet.wifi_matrix) +
                    sizeof(packet.mouse_x) +
                    sizeof(packet.mouse_y) +
                    sizeof(packet.gamepad_lx) +
                    sizeof(packet.gamepad_ly) +
                    sizeof(packet.gamepad_rx) +
                    sizeof(packet.gamepad_ry) +
                    sizeof(packet.gamepad_lz) +
                    sizeof(packet.gamepad_rz), &packet.gamepad_gyro, sizeof(packet.gamepad_gyro));
    memcpy(buffer + sizeof(packet.wifi_allow_communication) +
                    sizeof(packet.synced_keyboard) +
                    sizeof(packet.synced_mouse) +
                    sizeof(packet.synced_gamepad) +
                    sizeof(packet.alarms) +
                    sizeof(packet.alarm_pool) +
                    sizeof(packet.wifi_matrix) +
                    sizeof(packet.mouse_x) +
                    sizeof(packet.mouse_y) +
                    sizeof(packet.gamepad_lx) +
                    sizeof(packet.gamepad_ly) +
                    sizeof(packet.gamepad_rx) +
                    sizeof(packet.gamepad_ry) +
                    sizeof(packet.gamepad_lz) +
                    sizeof(packet.gamepad_rz) +
                    sizeof(packet.gamepad_gyro), &packet.gamepad_accel, sizeof(packet.gamepad_accel));
    memcpy(buffer + sizeof(packet.wifi_allow_communication) +
                    sizeof(packet.synced_keyboard) +
                    sizeof(packet.synced_mouse) +
                    sizeof(packet.synced_gamepad) +
                    sizeof(packet.alarms) +
                    sizeof(packet.alarm_pool) +
                    sizeof(packet.wifi_matrix) +
                    sizeof(packet.mouse_x) +
                    sizeof(packet.mouse_y) +
                    sizeof(packet.gamepad_lx) +
                    sizeof(packet.gamepad_ly) +
                    sizeof(packet.gamepad_rx) +
                    sizeof(packet.gamepad_ry) +
                    sizeof(packet.gamepad_lz) +
                    sizeof(packet.gamepad_rz) +
                    sizeof(packet.gamepad_gyro) +
                    sizeof(packet.gamepad_accel), &packet.switchProUsb, sizeof(packet.switchProUsb));

    // 打印缓冲区内容
    for (int i = 0; i < data_size - 1; i++) {
        printf("%02X\r\n", buffer[i]);
    }

    send_data_to_esp8285(buffer, data_size);

    free(buffer);
}
/* //send array
void send_array_over_wifi(uint8_t buffer[256]){

    send_data_to_esp8285(buffer, 256);

} */



// 接收并解包数据到结构体
transfer_struct receivePacketOverWiFi() {
    transfer_struct received_packet;
    
    // 计算需要接收的总字节数
    size_t data_size = sizeof(received_packet.wifi_allow_communication) + 
                       sizeof(received_packet.synced_keyboard) + 
                       sizeof(received_packet.synced_mouse) + 
                       sizeof(received_packet.synced_gamepad) + 
                       sizeof(received_packet.alarms) + 
                       sizeof(received_packet.alarm_pool) +
                       sizeof(received_packet.wifi_matrix) + 
                       sizeof(received_packet.mouse_x) + 
                       sizeof(received_packet.mouse_y) + 
                       sizeof(received_packet.gamepad_lx) + 
                       sizeof(received_packet.gamepad_ly) + 
                       sizeof(received_packet.gamepad_rx) + 
                       sizeof(received_packet.gamepad_ry) + 
                       sizeof(received_packet.gamepad_lz) + 
                       sizeof(received_packet.gamepad_rz) +
                       sizeof(received_packet.gamepad_gyro) + 
                       sizeof(received_packet.gamepad_accel) +
                       sizeof(received_packet.switchProUsb);

    // 接收数据
    uint8_t* buffer = receive_data_from_esp8285(data_size);
    if (buffer == NULL) {
        // 返回一个空的结构体或错误标志
        memset(&received_packet, 0, sizeof(transfer_struct));
        return received_packet;
    }

    size_t offset = 0;

    // 按照发送时相同的顺序解包数据
    memcpy(&received_packet.wifi_allow_communication, buffer + offset, sizeof(received_packet.wifi_allow_communication));
    offset += sizeof(received_packet.wifi_allow_communication);

    memcpy(&received_packet.synced_keyboard, buffer + offset, sizeof(received_packet.synced_keyboard));
    offset += sizeof(received_packet.synced_keyboard);

    memcpy(&received_packet.synced_mouse, buffer + offset, sizeof(received_packet.synced_mouse));
    offset += sizeof(received_packet.synced_mouse);

    memcpy(&received_packet.synced_gamepad, buffer + offset, sizeof(received_packet.synced_gamepad));
    offset += sizeof(received_packet.synced_gamepad);

    memcpy(&received_packet.alarms, buffer + offset, sizeof(received_packet.alarms));
    offset += sizeof(received_packet.alarms);

    memcpy(&received_packet.alarm_pool, buffer + offset, sizeof(received_packet.alarm_pool));
    offset += sizeof(received_packet.alarm_pool);

    memcpy(received_packet.wifi_matrix, buffer + offset, sizeof(received_packet.wifi_matrix));
    offset += sizeof(received_packet.wifi_matrix);

    memcpy(&received_packet.mouse_x, buffer + offset, sizeof(received_packet.mouse_x));
    offset += sizeof(received_packet.mouse_x);

    memcpy(&received_packet.mouse_y, buffer + offset, sizeof(received_packet.mouse_y));
    offset += sizeof(received_packet.mouse_y);

    memcpy(&received_packet.gamepad_lx, buffer + offset, sizeof(received_packet.gamepad_lx));
    offset += sizeof(received_packet.gamepad_lx);

    memcpy(&received_packet.gamepad_ly, buffer + offset, sizeof(received_packet.gamepad_ly));
    offset += sizeof(received_packet.gamepad_ly);

    memcpy(&received_packet.gamepad_rx, buffer + offset, sizeof(received_packet.gamepad_rx));
    offset += sizeof(received_packet.gamepad_rx);

    memcpy(&received_packet.gamepad_ry, buffer + offset, sizeof(received_packet.gamepad_ry));
    offset += sizeof(received_packet.gamepad_ry);

    memcpy(&received_packet.gamepad_lz, buffer + offset, sizeof(received_packet.gamepad_lz));
    offset += sizeof(received_packet.gamepad_lz);

    memcpy(&received_packet.gamepad_rz, buffer + offset, sizeof(received_packet.gamepad_rz));
    offset += sizeof(received_packet.gamepad_rz);

    memcpy(&received_packet.gamepad_gyro, buffer + offset, sizeof(received_packet.gamepad_gyro));
    offset += sizeof(received_packet.gamepad_gyro);

    memcpy(&received_packet.gamepad_accel, buffer + offset, sizeof(received_packet.gamepad_accel));
    offset += sizeof(received_packet.gamepad_accel);

    memcpy(&received_packet.switchProUsb, buffer + offset, sizeof(received_packet.switchProUsb));

    // 打印接收到的数据（用于调试）
    for (int i = 0; i < data_size; i++) {
        printf("Received: %02X\r\n", buffer[i]);
    }

    free(buffer);
    return received_packet;
}

// 处理接收到的数据包
void process_received_packet(transfer_struct received_packet) {
    // 通信状态标志
    bool wifi_comm_allowed = received_packet.wifi_allow_communication;
    
    // 同步状态
    bool keyboard_synced = received_packet.synced_keyboard;
    bool mouse_synced = received_packet.synced_mouse;
    bool gamepad_synced = received_packet.synced_gamepad;
    
    // 警报相关
    uint8_t current_alarms = received_packet.alarms;
    uint8_t alarm_pool_status = received_packet.alarm_pool;
    
    // 复制WiFi矩阵
    uint8_t wifi_matrix_copy[MATRIX_ROWS][MATRIX_COLS];
    memcpy(wifi_matrix_copy, received_packet.wifi_matrix, sizeof(wifi_matrix_copy));
    
    // 鼠标数据
    int16_t mouse_pos_x = received_packet.mouse_x;
    int16_t mouse_pos_y = received_packet.mouse_y;
    
    // 游戏手柄数据
    int16_t gamepad_left_x = received_packet.gamepad_lx;
    int16_t gamepad_left_y = received_packet.gamepad_ly;
    int16_t gamepad_right_x = received_packet.gamepad_rx;
    int16_t gamepad_right_y = received_packet.gamepad_ry;
    int16_t gamepad_left_z = received_packet.gamepad_lz;
    int16_t gamepad_right_z = received_packet.gamepad_rz;
    
    // 陀螺仪和加速度计数据
    int16_t gamepad_gyro_data = received_packet.gamepad_gyro;
    int16_t gamepad_accel_data = received_packet.gamepad_accel;
    
    // Switch Pro控制器USB状态
    bool switch_pro_usb_status = received_packet.switchProUsb;
    
    // 打印接收到的数据（用于调试）
    printf("WiFi通信状态: %d\n", wifi_comm_allowed);
    printf("键盘同步状态: %d\n", keyboard_synced);
    printf("鼠标同步状态: %d\n", mouse_synced);
    printf("手柄同步状态: %d\n", gamepad_synced);
    printf("当前警报: %d\n", current_alarms);
    printf("警报池状态: %d\n", alarm_pool_status);
    
    // 打印WiFi矩阵（如果需要）
    printf("WiFi矩阵内容:\n");
    for (int i = 0; i < MATRIX_ROWS; i++) {
        for (int j = 0; j < MATRIX_COLS; j++) {
            printf("%02X ", wifi_matrix_copy[i][j]);
        }
        printf("\n");
    }
    
    printf("鼠标位置: X=%d, Y=%d\n", mouse_pos_x, mouse_pos_y);
    printf("游戏手柄左摇杆: X=%d, Y=%d\n", gamepad_left_x, gamepad_left_y);
    printf("游戏手柄右摇杆: X=%d, Y=%d\n", gamepad_right_x, gamepad_right_y);
    printf("游戏手柄扳机键: L=%d, R=%d\n", gamepad_left_z, gamepad_right_z);
    printf("陀螺仪数据: %d\n", gamepad_gyro_data);
    printf("加速度计数据: %d\n", gamepad_accel_data);
    printf("Switch Pro USB状态: %d\n", switch_pro_usb_status);
}
