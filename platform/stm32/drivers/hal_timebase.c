/* HAL timebase on TIM1. SysTick is reserved for the future FreeRTOS port.
 * Uses CMSIS registers because the generated project has no HAL TIM driver.
 */
#include "stm32f4xx_hal.h"

HAL_StatusTypeDef HAL_InitTick(uint32_t priority) {
    RCC_ClkInitTypeDef clocks;
    uint32_t latency;
    uint32_t timer_clock = HAL_RCC_GetPCLK2Freq();
    const uint32_t counter_clock = 10000U;

    if (priority >= (1UL << __NVIC_PRIO_BITS)) {
        return HAL_ERROR;
    }

    HAL_RCC_GetClockConfig(&clocks, &latency);
    if (clocks.APB2CLKDivider != RCC_HCLK_DIV1) {
        timer_clock *= 2U;
    }

    if (timer_clock < counter_clock || timer_clock % counter_clock != 0U) {
        return HAL_ERROR;
    }

    __HAL_RCC_TIM1_CLK_ENABLE();
    HAL_NVIC_DisableIRQ(TIM1_UP_TIM10_IRQn);

    TIM1->CR1 = 0U;
    TIM1->DIER = 0U;
    TIM1->PSC = timer_clock / counter_clock - 1U;
    TIM1->ARR = 10U * (uint32_t)uwTickFreq - 1U;
    TIM1->CNT = 0U;
    TIM1->RCR = 0U;
    TIM1->EGR = TIM_EGR_UG;
    TIM1->SR = 0U;

    HAL_NVIC_ClearPendingIRQ(TIM1_UP_TIM10_IRQn);
    HAL_NVIC_SetPriority(TIM1_UP_TIM10_IRQn, priority, 0U);
    uwTickPrio = priority;
    TIM1->DIER = TIM_DIER_UIE;
    HAL_NVIC_EnableIRQ(TIM1_UP_TIM10_IRQn);
    TIM1->CR1 = TIM_CR1_CEN;

    return HAL_OK;
}

void HAL_SuspendTick(void) {
    TIM1->DIER &= ~TIM_DIER_UIE;
}

void HAL_ResumeTick(void) {
    TIM1->DIER |= TIM_DIER_UIE;
}

void TIM1_UP_TIM10_IRQHandler(void) {
    if ((TIM1->SR & TIM_SR_UIF) != 0U && (TIM1->DIER & TIM_DIER_UIE) != 0U) {
        /* Writing zero clears UIF; ones leave other status flags unchanged. */
        TIM1->SR = ~TIM_SR_UIF;
        HAL_IncTick();
    }
}
