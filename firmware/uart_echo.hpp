#pragma once

// Diagnostic loop; entered after HAL and clock setup, before FreeRTOS starts.
[[noreturn]] void uart_echo_run();
