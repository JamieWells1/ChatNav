#include "gps/gps_uart.h"

#include <stdio.h>

#include "config.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/uart.h"

#define GPS_UART uart1
#define GPS_UART_IRQ UART1_IRQ
#define RING_SIZE 1024 /* power of two */

static volatile uint8_t ring[RING_SIZE];
static volatile uint32_t head; /* written by the IRQ */
static volatile uint32_t tail; /* written by the main loop */
static volatile uint32_t overruns;
static volatile uint32_t total;

static void on_uart_rx(void) {
    while (uart_is_readable(GPS_UART)) {
        uint8_t c = (uint8_t)uart_getc(GPS_UART);
        total++;
        if (head - tail >= RING_SIZE) {
            overruns++;
            continue;
        }
        ring[head % RING_SIZE] = c;
        head++;
    }
}

void gps_uart_init(void) {
    uart_init(GPS_UART, GPS_BAUD);
    gpio_set_function(PIN_GPS_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_GPS_RX, GPIO_FUNC_UART);
    uart_set_format(GPS_UART, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(GPS_UART, true);
    irq_set_exclusive_handler(GPS_UART_IRQ, on_uart_rx);
    irq_set_enabled(GPS_UART_IRQ, true);
    uart_set_irq_enables(GPS_UART, true, false);
    printf("[GPS] uart1 up at %d baud, RX GP%d TX GP%d\n", GPS_BAUD, PIN_GPS_RX, PIN_GPS_TX);
}

int gps_uart_read(void) {
    if (tail == head) return -1;
    uint8_t c = ring[tail % RING_SIZE];
    tail++;
    return c;
}

uint32_t gps_uart_overruns(void) { return overruns; }

uint32_t gps_uart_bytes(void) { return total; }
