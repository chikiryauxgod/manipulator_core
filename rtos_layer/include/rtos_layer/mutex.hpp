#pragma once

#include "FreeRTOS.h"
#include "semphr.h"

namespace rtos_layer {

// Non-recursive, task context only. Owner must outlive guards and waiters.
class StaticMutex final {
public:

    StaticMutex() noexcept : handle_(xSemaphoreCreateMutexStatic(&control_)) {
        configASSERT(handle_ != nullptr);
    }

    ~StaticMutex() {
        vSemaphoreDelete(handle_);
    }

    StaticMutex(const StaticMutex&) = delete;
    StaticMutex& operator=(const StaticMutex&) = delete;

    bool lock(TickType_t timeout) noexcept {
        return xSemaphoreTake(handle_, timeout) == pdTRUE;
    }

    bool unlock() noexcept {
        return xSemaphoreGive(handle_) == pdTRUE;
    }

private:

    StaticSemaphore_t control_{};
    SemaphoreHandle_t handle_;
};

// Timed acquisition: caller MUST check owns_lock() before accessing shared data.
class MutexGuard final {
public:

    explicit MutexGuard(StaticMutex& mutex, TickType_t timeout) noexcept
    : mutex_(mutex), owned_(mutex.lock(timeout)) {
    }

    ~MutexGuard() {
        if (owned_) {
            const bool released = mutex_.unlock();
            configASSERT(released);
            (void)released;
        }
    }

    MutexGuard(const MutexGuard&) = delete;
    MutexGuard& operator=(const MutexGuard&) = delete;

    [[nodiscard]] bool owns_lock() const noexcept {
        return owned_;
    }

private:

    StaticMutex& mutex_;
    bool owned_;
};

}
