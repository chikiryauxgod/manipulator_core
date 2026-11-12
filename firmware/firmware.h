#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Static debugger-visible diagnostics. Stack values are in StackType_t words. */
typedef struct {
    uint32_t protocol_heartbeat;
    uint32_t scheduler_tick;
    uint32_t hal_tick;
    uint32_t worker_stack_free_min;
    uint32_t protocol_stack_free_min;
    uint32_t configuration_loaded;
    uint32_t dropped_frames;
    uint32_t modbus_errors;
    int32_t last_modbus_error;
    /* Each counter has a single writer; debugger observes after Suspend. */
    uint32_t uart_echo_mode;
    uint32_t uart_irq_count;
    uint32_t uart_rx_bytes;
    uint32_t uart_error_count;
    uint32_t uart_last_status;
    uint32_t uart_last_byte;
    uint32_t rtu_timer_count;
    uint32_t rtu_frames;
    uint32_t rtu_last_size;
    uint32_t rtu_crc_errors;
    uint32_t uart_tx_bytes;
    uint32_t fault_line;
    const char* fault_file;
} FirmwareDiagnostics;

extern volatile FirmwareDiagnostics firmware_diagnostics;

/* Called once after HAL and system-clock initialization. Does not return. */
void firmware_start(void);
void firmware_assert_failed(const char* file, uint32_t line);

#ifdef __cplusplus
}
#endif
