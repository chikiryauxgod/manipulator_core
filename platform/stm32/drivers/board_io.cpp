#include "board_io.hpp"

#include "stm32f4xx_hal.h"

namespace manipulator::stm32::drivers {

uint32_t milliseconds() {
    return HAL_GetTick();
}

void delay_ms(uint32_t duration) {
    HAL_Delay(duration);
}

void disable_interrupts() {
    __disable_irq();
}

void idle_instruction() {
    __NOP();
}

void StatusLed::initialize() {
    __HAL_RCC_GPIOC_CLK_ENABLE();
    set(false);
    GPIO_InitTypeDef gpio{};
    gpio.Pin = GPIO_PIN_13;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio);
}

void StatusLed::set(bool on) {
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

} // namespace manipulator::stm32::drivers
