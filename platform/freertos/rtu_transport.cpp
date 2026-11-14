#include "rtu_transport.hpp"

#include "transport/rtu/crc.hpp"
#include <cstring>

namespace manipulator {

bool FreeRtosRtuTransport::initialize(uint32_t baud, bool even_parity) {
    if (baud < 38400 || baud > 115200) {
        return false;
    }
    owner_ = xTaskGetCurrentTaskHandle();
    framer_.character_duration((11000000U + baud - 1) / baud);

    // Priority 6 may call FreeRTOS FromISR; both IRQs are non-preempting peers.
    constexpr uint32_t priority = 6;
    using stm32::drivers::SerialFormat;
    if (!owner_ || !timer_.initialize(silence, this, priority) ||
        !uart_.initialize(baud,
                          even_parity ? SerialFormat::eight_e_one : SerialFormat::eight_n_two)) {
        return false;
    }
    uart_.enable_receive_interrupt(received, this, priority);
    return true;
}

void FreeRtosRtuTransport::received(void* context, const stm32::drivers::ReceivedByte& byte) {
    auto& self = *static_cast<FreeRtosRtuTransport*>(context);
    self.framer_.byte(byte.value, self.timer_.now(), byte.error);
    self.timer_.arm_after(self.framer_.completion_delay());
}

void FreeRtosRtuTransport::silence(void* context) {
    auto& self = *static_cast<FreeRtosRtuTransport*>(context);
    ++self.timer_interrupts_;
    const uint32_t remaining = self.framer_.silence(self.timer_.now());
    if (remaining != 0) {
        self.timer_.arm_after(remaining);
        return;
    }
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(self.owner_, &woken);
    portYIELD_FROM_ISR(woken);
}

void FreeRtosRtuTransport::acquire_frame() {
    for (;;) {
        taskENTER_CRITICAL();
        const bool available = framer_.pop(frame_, size_);
        taskEXIT_CRITICAL();

        if (available) {
            ++frames_;
            last_size_ = size_;
            if (rtu::crc16(frame_, size_) != 0) {
                ++crc_errors_;
                continue;
            }
            if (frame_[0] == 0) {
                continue; // API requires acknowledged unicast commands
            }
            position_ = 0;
            return;
        }

        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100)) == 0) {
            size_ = position_ = 0;
            return;
        }
    }
}

int32_t FreeRtosRtuTransport::read(uint8_t* output, uint16_t count, int32_t) {
    const uint16_t available = size_ - position_;
    const uint16_t copied = count < available ? count : available;
    std::memcpy(output, frame_ + position_, copied);
    position_ += copied;
    return copied;
}

int32_t FreeRtosRtuTransport::write(const uint8_t* data, uint16_t count, int32_t) {
    // Framed protocol supplies the whole response. Preserve the transport's
    // finite 200 ms whole-response guard independently of nanoMODBUS read timeout.
    return uart_.write(data, count, 200) ? count : -1;
}

void FreeRtosRtuTransport::discard() {
    position_ = size_; // never consume the next queued ADU
}

RtuDiagnostics FreeRtosRtuTransport::diagnostics() {
    taskENTER_CRITICAL();
    RtuDiagnostics result;
    result.uart = uart_.statistics();
    result.timer_interrupts = timer_interrupts_;
    result.frames = frames_;
    result.last_size = last_size_;
    result.crc_errors = crc_errors_;
    result.dropped_frames = framer_.dropped() + crc_errors_;
    taskEXIT_CRITICAL();
    return result;
}

} // namespace manipulator
