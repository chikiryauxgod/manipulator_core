#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include "FreeRTOS.h"
#include "queue.h"

namespace rtos_layer {

// Owner must outlive all users; destruction requires no blocked tasks or ISR users.
template <class T, std::size_t Capacity> class StaticQueue final {
    static_assert(std::is_trivially_copyable_v<T>, "Queue messages are copied bytewise");
    static_assert(Capacity > 0 && Capacity <= std::numeric_limits<UBaseType_t>::max());
    static_assert(sizeof(T) <= std::numeric_limits<UBaseType_t>::max());
    static_assert(Capacity <= std::numeric_limits<std::size_t>::max() / sizeof(T));

public:

    StaticQueue() noexcept
        : handle_(xQueueCreateStatic(static_cast<UBaseType_t>(Capacity),
                                     static_cast<UBaseType_t>(sizeof(T)), storage_, &control_)) {
        configASSERT(handle_ != nullptr);
    }

    ~StaticQueue() {
        vQueueDelete(handle_);
    }

    StaticQueue(const StaticQueue&) = delete;
    StaticQueue& operator=(const StaticQueue&) = delete;

    bool send(const T& value, TickType_t timeout = 0) noexcept {
        return xQueueSend(handle_, &value, timeout) == pdPASS;
    }

    bool receive(T& value, TickType_t timeout = 0) noexcept {
        return xQueueReceive(handle_, &value, timeout) == pdPASS;
    }

    // Initialise woken=pdFALSE once per ISR, share across operations, then
    // call the port-specific portYIELD_FROM_ISR(woken) at the ISR boundary.
    bool send_from_isr(const T& value, BaseType_t& woken) noexcept {
        return xQueueSendFromISR(handle_, &value, &woken) == pdPASS;
    }

    bool receive_from_isr(T& value, BaseType_t& woken) noexcept {
        return xQueueReceiveFromISR(handle_, &value, &woken) == pdPASS;
    }

private:

    StaticQueue_t control_{};
    alignas(std::max_align_t) std::uint8_t storage_[Capacity * sizeof(T)]{};

    QueueHandle_t handle_;
};

} // namespace rtos_layer
