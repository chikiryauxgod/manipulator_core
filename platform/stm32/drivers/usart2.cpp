#include "usart2.hpp"

#include "stm32f4xx_hal.h"

namespace {

manipulator::stm32::drivers::Usart2* active = nullptr;
constexpr uint32_t receive_errors = USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE;

}

namespace manipulator::stm32::drivers {

bool Usart2::initialize(uint32_t baud, SerialFormat format)
{
    const uint32_t clock = HAL_RCC_GetPCLK1Freq();
    if (baud == 0 || baud > clock / 16 || (active && active != this)) {
        return false;
    }

    HAL_NVIC_DisableIRQ(USART2_IRQn);
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_USART2_FORCE_RESET();
    __HAL_RCC_USART2_RELEASE_RESET();

    GPIO_InitTypeDef gpio{};
    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    USART2->BRR = (clock + baud / 2) / baud;
    USART2->CR2 = format == SerialFormat::eight_n_two ? USART_CR2_STOP_1 : 0U;
    USART2->CR3 = 0;
    // M=1 with parity gives eight payload bits plus parity.
    USART2->CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE |
                 (format == SerialFormat::eight_e_one ? USART_CR1_M | USART_CR1_PCE : 0U);
    active = this;
    return true;
}

void Usart2::enable_receive_interrupt(ReceiveHandler handler, void* context, uint32_t priority)
{
    HAL_NVIC_DisableIRQ(USART2_IRQn);
    receive_handler_ = handler;
    receive_context_ = context;
    USART2->CR3 |= USART_CR3_EIE;
    USART2->CR1 |= USART_CR1_RXNEIE | USART_CR1_PEIE;
    HAL_NVIC_SetPriority(USART2_IRQn, priority, 0);
    HAL_NVIC_ClearPendingIRQ(USART2_IRQn);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

bool Usart2::try_receive(ReceivedByte& byte)
{
    const uint32_t status = USART2->SR;
    statistics_.last_status = status;
    if ((status & (USART_SR_RXNE | receive_errors)) == 0) {
        return false;
    }

    // SR followed by DR clears RXNE and the receive-error flags.
    byte.value = USART2->DR;
    byte.has_byte = (status & USART_SR_RXNE) != 0;
    byte.error = (status & receive_errors) != 0;
    statistics_.last_byte = byte.value;
    if (byte.has_byte) {
        ++statistics_.received;
    }
    if (byte.error) {
        ++statistics_.errors;
    }
    return true;
}

bool Usart2::try_transmit(uint8_t byte)
{
    if ((USART2->SR & USART_SR_TXE) == 0) {
        return false;
    }
    USART2->DR = byte;
    ++statistics_.transmitted;
    return true;
}

bool Usart2::write(const uint8_t* bytes, uint16_t size, uint32_t timeout_ms)
{
    const auto start = HAL_GetTick();
    for (uint16_t i = 0; i < size; ++i) {
        while (!try_transmit(bytes[i])) {
            if (HAL_GetTick() - start > timeout_ms) {
                return false;
            }
        }
    }
    while ((USART2->SR & USART_SR_TC) == 0) {
        if (HAL_GetTick() - start > timeout_ms) {
            return false;
        }
    }
    return true;
}

UartStatistics Usart2::statistics() const
{
    return {statistics_.interrupts, statistics_.received, statistics_.errors,
            statistics_.last_status, statistics_.last_byte, statistics_.transmitted};
}

void Usart2::handle_interrupt()
{
    ++statistics_.interrupts;
    ReceivedByte byte;
    if (try_receive(byte) && receive_handler_) {
        receive_handler_(receive_context_, byte);
    }
}

}

extern "C" void USART2_IRQHandler(void)
{
    if (active) {
        active->handle_interrupt();
    }
}
