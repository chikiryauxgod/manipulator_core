#pragma once

#include <stdint.h>

extern uint32_t SystemCoreClock;

#ifdef __cplusplus
extern "C" {
#endif
void firmware_assert_failed(const char* file, uint32_t line);
#ifdef __cplusplus
}
#endif

#define configUSE_PREEMPTION                     1
#define configUSE_TIME_SLICING                   1
#define configCPU_CLOCK_HZ                      (SystemCoreClock)
#define configTICK_RATE_HZ                      1000U
#define configMAX_PRIORITIES                    5U
#define configMINIMAL_STACK_SIZE                256U
#define configMAX_TASK_NAME_LEN                 16U
#define configUSE_16_BIT_TICKS                   0
#define configSTACK_DEPTH_TYPE                  uint32_t
#define configIDLE_SHOULD_YIELD                  1
#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        0
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES              0
#define configUSE_COUNTING_SEMAPHORES           0
#define configQUEUE_REGISTRY_SIZE               0
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1
#define configUSE_TIMERS                        0
#define configUSE_CO_ROUTINES                   0
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            0
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_TICKLESS_IDLE                 0
#define configUSE_NEWLIB_REENTRANT              0
#define configCHECK_HANDLER_INSTALLATION        1

#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_uxTaskGetStackHighWaterMark      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_vTaskDelete                     0

/* STM32F401 implements four NVIC priority bits. IRQs calling FreeRTOS
 * FromISR APIs must use numerical priorities 5 through 15.
 * TIM1 only increments the HAL tick and does not call FreeRTOS.
 */
#define configPRIO_BITS                         4U
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15U
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5U
#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))

/* The ARM_CM4F port owns these vector-table handlers directly. */
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler
#define xPortSysTickHandler                     SysTick_Handler

#define configASSERT(condition) \
    do { \
        if (!(condition)) { \
            firmware_assert_failed(__FILE__, __LINE__); \
        } \
    } while (0)
