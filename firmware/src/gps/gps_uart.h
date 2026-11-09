#ifndef GPS_UART_H
#define GPS_UART_H

#include <stdbool.h>
#include <stdint.h>

void gps_uart_init(void);

/* Next received byte, or -1 if none waiting. */
int gps_uart_read(void);

/* Bytes dropped because the main loop did not drain the ring in time. */
uint32_t gps_uart_overruns(void);

/* Total bytes received since init. */
uint32_t gps_uart_bytes(void);

#endif
