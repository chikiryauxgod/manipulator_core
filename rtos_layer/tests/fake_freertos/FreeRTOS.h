#pragma once
#include <cassert>
#include <cstdint>
using BaseType_t = int;
using UBaseType_t = unsigned;
using TickType_t = unsigned;
constexpr int pdTRUE=1, pdFALSE=0, pdPASS=1;
#define configASSERT(x) assert(x)
inline TickType_t last_timeout=0;
