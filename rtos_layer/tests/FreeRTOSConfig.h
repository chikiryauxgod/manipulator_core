#pragma once

#include <stdlib.h>
#define configUSE_PREEMPTION 1
#define configUSE_IDLE_HOOK 0
#define configUSE_TICK_HOOK 0
#define configCPU_CLOCK_HZ 100000000UL
#define configTICK_RATE_HZ 1000
#define configMAX_PRIORITIES 5
#define configMINIMAL_STACK_SIZE 4096
#define configTOTAL_HEAP_SIZE (1024 * 1024)
#define configMAX_TASK_NAME_LEN 16
#define configUSE_16_BIT_TICKS 0
#define configIDLE_SHOULD_YIELD 1
#define configUSE_MUTEXES 1
#define configUSE_TASK_NOTIFICATIONS 1
#define configSUPPORT_STATIC_ALLOCATION 1
#define configSUPPORT_DYNAMIC_ALLOCATION 1
#define configUSE_TIMERS 0
#define configUSE_CO_ROUTINES 0
#define configUSE_MALLOC_FAILED_HOOK 0
#define configCHECK_FOR_STACK_OVERFLOW 0
#define configUSE_POSIX_ERRNO 1
#define INCLUDE_vTaskDelay 1
#define INCLUDE_vTaskSuspend 1
#define INCLUDE_vTaskDelete 1
#define configASSERT(x)                                                                            \
    do {                                                                                           \
        if (!(x))                                                                                  \
            abort();                                                                               \
    } while (0)
