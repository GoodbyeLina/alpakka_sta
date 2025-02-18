// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022, Input Labs Oy.

#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/multicore.h>
#include <pico/time.h>
#include <tusb.h>
#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include "config.h"
#include "tusb_config.h"
#include "led.h"
#include "bus.h"
#include "profile.h"
#include "touch.h"
#include "imu.h"
#include "hid.h"
#include "uart_esp.h"
#include "uart.h"
#include "wifi_sta.h"
#include "logging.h"
#include "common.h"
#include "transfer.h"

#if __has_include("version.h")
#include "version.h"
#else
#define VERSION "undefined"
#endif

#define SYS_CLOCK_MHZ 48
#define PLL_SYS_KHZ (SYS_CLOCK_MHZ * 1000)

void title()
{
    info("╔====================╗\n");
    info("║ Input Labs Oy.     ║\n");
    info("║ Alpakka controller ║\n");
    info("╚====================╝\n");
    info("Firmware version: %s\n", VERSION);
}

void sys_clock_reset()
{
    if (SYS_CLOCK_MHZ > 270)
        vreg_set_voltage(VREG_VOLTAGE_1_30); // 300MHz需要调压，如果270MHz不需要加这句
    if (SYS_CLOCK_MHZ != 48)
        set_sys_clock_khz(PLL_SYS_KHZ, true);
}

void main_init()
{
    // config_write_init();
    // LED feedback ASAP after booting.
    led_init();
    // Init stdio and logging.
    // #if SINGLE_THUMBSTICK
    //     stdio_uart_init();
    // #endif
    // 初始化所有的标准输入输出设备，包括串口、GPIO等。这在嵌入式系统中非常重要，因为它确保了系统能够正确地与外部设备进行通信。
    stdio_init_all();
    // 日志初始化
    logging_init();
    // 加载手柄布局配置
    title();
    config_init();
    // 重置时钟频率.
    sys_clock_reset();
    // 初始化 USB.
    tusb_init();
    wait_for_usb_init();
    if (current_protocol_compatible_with_webusb())  // 当前协议兼容 webUSB
    {
        webusb_set_shut_off(false);
    }
    // 总线初始化
    bus_init();
    // HID 初始化，创建一个警报池
    hid_init();

    // 连接WIFI
    sendCMD("AT+CIPSEND", ">");
    led_blink_ms(2000);

    uart_puts(UART_ID, "Hello World!");
    uart_puts(UART_ID, "\r\n");

    char send_data[3] = {'1', '2', '3'};
    // send_data_to_esp8285(send_data, sizeof(send_data));

    DataPacket mypacket;
    mypacket.synced_keyboard = true;
    mypacket.synced_mouse = false;
    mypacket.synced_gamepad = true;

    for (int i = 0; i < 256; i++)
    {
        mypacket.state_matrix[i] = 1;
    }

    // 摇杆初始化
    thumbstick_init();
#if DUAL_THUMBSTICK // 第二个摇杆的初始化
    right_thumbstick_init();
#endif
    // 触控初始化
    touch_init();
    // 旋转初始化
    rotary_init();
    // 手柄映射初始化
    profile_init();
    // 惯性单元 初始化
    imu_init();
}

void main_loop()
{
    info("INIT: Main loop\n");
    int16_t i = 0;
    logging_set_onloop(true);
    while (true)
    {
        i++;
        // Start timer.
        uint32_t tick_start = time_us_32();
        // Config.
        config_sync();
        // Report.
        profile_report_active();
        hid_report();
        // Tick interval control.
        uint32_t tick_completed = time_us_32() - tick_start;
        uint16_t tick_interval = 1000000 / CFG_TICK_FREQUENCY;
        int32_t tick_idle = tick_interval - (int32_t)tick_completed;
        // Listen to incoming UART messages. 每 250 次循环后监听一次
        uart_listen_char(i);
        // Timing stats.
        if (logging_get_level() >= LOG_DEBUG)
        {
            // 平均时间计算
            static float average = 0;
            average = smooth(average, tick_completed, 100);
            if (!(i % 2000))
                debug("Loop: avg=%.0f (us)\n", average);
        }
        // Idling control.
        if (tick_idle > 0)
            sleep_us((uint32_t)tick_idle);
        else
            info("+");
    }
}

int main()
{
    main_init();
    main_loop();
}
