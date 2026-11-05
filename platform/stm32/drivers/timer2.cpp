#include "timer2.hpp"

#include "stm32f4xx_hal.h"

namespace {

manipulator::stm32::drivers::Timer2* active = nullptr;

}

namespace manipulator::stm32::drivers {

bool Timer2::initialize(AlarmHandler handler, void* context, uint32_t priority)
{
    if (active && active != this) {
        return false;
    }

    RCC_ClkInitTypeDef clocks{};
    uint32_t latency;
    HAL_RCC_GetClockConfig(&clocks, &latency);
    uint32_t clock = HAL_RCC_GetPCLK1Freq();
    if (clocks.APB1CLKDivider != RCC_HCLK_DIV1) {
        clock *= 2;
    }
    if (clock < 1000000U || clock % 1000000U != 0) {
        return false;
    }

    HAL_NVIC_DisableIRQ(TIM2_IRQn);
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_TIM2_FORCE_RESET();
    __HAL_RCC_TIM2_RELEASE_RESET();
    TIM2->PSC = clock / 1000000U - 1;
    TIM2->ARR = 0xffffffffU;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0;
    TIM2->CR1 = TIM_CR1_CEN;

    handler_ = handler;
    context_ = context;
    active = this;
    HAL_NVIC_SetPriority(TIM2_IRQn, priority, 0);
    HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    return true;
}

uint32_t Timer2::now() const
{
    return TIM2->CNT;
}

void Timer2::arm_after(uint32_t microseconds)
{
    TIM2->CCR1 = TIM2->CNT + microseconds;
    TIM2->SR = ~TIM_SR_CC1IF;
    TIM2->DIER |= TIM_DIER_CC1IE;
}

void Timer2::handle_interrupt()
{
    if ((TIM2->SR & TIM_SR_CC1IF) && (TIM2->DIER & TIM_DIER_CC1IE)) {
        TIM2->SR = ~TIM_SR_CC1IF;
        TIM2->DIER &= ~TIM_DIER_CC1IE;
        if (handler_) {
            handler_(context_);
        }
    }
}

}

extern "C" void TIM2_IRQHandler(void)
{
    if (active) {
        active->handle_interrupt();
    }
}
