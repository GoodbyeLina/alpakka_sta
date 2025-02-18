#pragma once

#include <stdint.h>

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#define UART_ID uart0
#define BAUD_RATE 115200
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

#define UART_TX_PIN 0
#define UART_RX_PIN 1

void init_uart();


