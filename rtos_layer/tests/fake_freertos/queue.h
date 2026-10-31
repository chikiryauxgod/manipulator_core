#pragma once
#include "FreeRTOS.h"
#include <cstring>
struct StaticQueue_t { unsigned capacity, size, head, used; uint8_t* storage; };
using QueueHandle_t=StaticQueue_t*;
inline QueueHandle_t xQueueCreateStatic(unsigned n,unsigned s,uint8_t* b,StaticQueue_t* q) {
    *q={n,s,0,0,b}; return q;
}
inline int xQueueSend(QueueHandle_t q,const void* p,TickType_t t) {
    last_timeout=t; if(q->used==q->capacity) return 0;
    std::memcpy(q->storage+((q->head+q->used)%q->capacity)*q->size,p,q->size);
    ++q->used; return 1;
}
inline int xQueueReceive(QueueHandle_t q,void* p,TickType_t t) {
    last_timeout=t; if(!q->used) return 0;
    std::memcpy(p,q->storage+q->head*q->size,q->size);
    q->head=(q->head+1)%q->capacity; --q->used; return 1;
}
inline int xQueueSendFromISR(QueueHandle_t q,const void* p,int* w) {
    int result=xQueueSend(q,p,0); if(result) *w=1; return result;
}
inline int xQueueReceiveFromISR(QueueHandle_t q,void* p,int* w) {
    int result=xQueueReceive(q,p,0); if(result) *w=1; return result;
}
inline void vQueueDelete(QueueHandle_t) {}
