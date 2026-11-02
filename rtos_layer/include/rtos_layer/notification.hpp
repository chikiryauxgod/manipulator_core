#pragma once

#include "FreeRTOS.h"
#include "task.h"

namespace rtos_layer {

// Counting notification at index 0, reserved exclusively for this channel.
// Target handle is borrowed; the task must remain alive during every give.
class Notification final {
public:

    explicit Notification(TaskHandle_t target) noexcept : target_(target) {
        configASSERT(target_ != nullptr);
    }

    void give() const noexcept {
        xTaskNotifyGive(target_);
    }

    void give_from_isr(BaseType_t& woken) const noexcept {
        vTaskNotifyGiveFromISR(target_, &woken);
    }

    // Always waits on the CALLING task's notification, not a stored target.
    static uint32_t take(bool clear_count, TickType_t timeout) noexcept {
        return ulTaskNotifyTake(clear_count ? pdTRUE : pdFALSE, timeout);
    }

private:

    TaskHandle_t target_;
};

}
