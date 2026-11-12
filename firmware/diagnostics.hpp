#pragma once

#include "firmware.h"
#include "stm32/drivers/usart2.hpp"

// Diagnostics are composed here; drivers never depend on the firmware globals.
inline void publish_uart_statistics(const manipulator::stm32::drivers::UartStatistics& stats)
{
    firmware_diagnostics.uart_irq_count = stats.interrupts;
    firmware_diagnostics.uart_rx_bytes = stats.received;
    firmware_diagnostics.uart_error_count = stats.errors;
    firmware_diagnostics.uart_last_status = stats.last_status;
    firmware_diagnostics.uart_last_byte = stats.last_byte;
    firmware_diagnostics.uart_tx_bytes = stats.transmitted;
}
