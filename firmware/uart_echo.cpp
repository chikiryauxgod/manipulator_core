#include "uart_echo.hpp"

#include "diagnostics.hpp"
#include "stm32/drivers/board_io.hpp"
#include "stm32/drivers/usart2.hpp"

[[noreturn]] void uart_echo_run()
{
    using namespace manipulator::stm32::drivers;

    StatusLed led;
    led.initialize();
    for (unsigned i = 0; i < 3; ++i) {
        led.set(true);
        delay_ms(100);
        led.set(false);
        delay_ms(100);
    }

    static Usart2 uart;
    if (!uart.initialize(115200, SerialFormat::eight_n_one)) {
        firmware_assert_failed(__FILE__, __LINE__);
    }
    firmware_diagnostics.uart_echo_mode = 1;
    bool led_active = false;
    uint32_t last_activity = 0;

    for (;;) {
        const uint32_t now = milliseconds();
        firmware_diagnostics.hal_tick = now;
        if (led_active && now - last_activity >= 100) {
            led.set(false);
            led_active = false;
        }

        ReceivedByte byte;
        const bool received = uart.try_receive(byte);
        publish_uart_statistics(uart.statistics());
        if (!received) {
            continue;
        }
        if (byte.has_byte) {
            led.set(true);
            last_activity = now;
            led_active = true;
        }
        if (byte.error || !byte.has_byte) {
            continue;
        }

        const uint32_t start = milliseconds();
        while (!uart.try_transmit(byte.value)) {
            if (milliseconds() - start >= 100) {
                firmware_assert_failed(__FILE__, __LINE__);
            }
        }
        publish_uart_statistics(uart.statistics());
    }
}
