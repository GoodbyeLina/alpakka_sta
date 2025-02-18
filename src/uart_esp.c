// SPDX-License-Identifier: GPL-2.0-only
// Copyright (C) 2022, Input Labs Oy.

#include <stdio.h>
#include <pico/bootrom.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"

#include <hardware/watchdog.h>
#include "config.h"
#include "self_test.h"
#include "logging.h"
#include "uart_esp.h"

#include "pico/stdlib.h"
#include "hardware/uart.h"

#define UART_ID uart0
#define BAUD_RATE 115200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

#define UART_TX_PIN 0
#define UART_RX_PIN 1

void init_uart() {
    uart_init(UART_ID, BAUD_RATE);

    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // uart_set_hw_flow(UART_ID, false, false);
    // uart_set_format(UART_ID, DATA_BITS, STOP_BITS, PARITY);

    uart_puts(UART_ID, "+++");
    sleep_ms(1000);
    while (uart_is_readable(UART_ID))
        uart_getc(UART_ID);
    sleep_ms(2000);
}
