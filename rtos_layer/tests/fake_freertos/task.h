#pragma once
#include "FreeRTOS.h"
struct FakeTask { uint32_t count=0; };
using TaskHandle_t=FakeTask*;
inline FakeTask current_task;
inline int xTaskNotifyGive(TaskHandle_t t) {++t->count; return 1;}
inline void vTaskNotifyGiveFromISR(TaskHandle_t t,int* w) {++t->count; *w=1;}
inline uint32_t ulTaskNotifyTake(int clear,TickType_t timeout) {
    last_timeout=timeout; auto n=current_task.count;
    if(clear) current_task.count=0; else if(n) --current_task.count;
    return n;
}
