#pragma once

/* 1: standalone USART2 polling echo, 115200 8N1.
 * 0: normal FreeRTOS / Modbus RTU firmware.
 * Rebuild and reflash after changing this flag.
 */
#ifndef FIRMWARE_UART_ECHO
#define FIRMWARE_UART_ECHO 0
#endif

#if FIRMWARE_UART_ECHO != 0 && FIRMWARE_UART_ECHO != 1
#error "FIRMWARE_UART_ECHO must be 0 or 1"
#endif
