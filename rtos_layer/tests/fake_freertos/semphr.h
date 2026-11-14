#pragma once

#include "FreeRTOS.h"

struct StaticSemaphore_t {
    bool locked = false;
};

using SemaphoreHandle_t = StaticSemaphore_t*;
inline unsigned releases = 0;

inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* s) {
    return s;
}

inline int xSemaphoreTake(SemaphoreHandle_t s, TickType_t t) {
    last_timeout = t;
    if (s->locked) {
        return 0;
    }

    s->locked = true;
    return 1;
}

inline int xSemaphoreGive(SemaphoreHandle_t s) {
    if (!s->locked) {
        return 0;
    }

    s->locked = false;
    ++releases;
    return 1;
}

inline void vSemaphoreDelete(SemaphoreHandle_t) {
}
