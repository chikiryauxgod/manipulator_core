#pragma once

#include <cstdint>

namespace manipulator::stm32::drivers {

uint32_t milliseconds();
void delay_ms(uint32_t duration);
void disable_interrupts();
void idle_instruction();

// Black Pill user LED PC13, active low.
class StatusLed final {
public:

    void initialize();
    void set(bool on);
};

} // namespace manipulator::stm32::drivers
