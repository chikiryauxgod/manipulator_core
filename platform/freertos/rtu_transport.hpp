#pragma once

#include "transport/byte_stream.hpp"
#include "transport/rtu/framer.hpp"
#include "stm32/drivers/usart2.hpp"
#include "stm32/drivers/timer2.hpp"
#include "FreeRTOS.h"
#include "task.h"

namespace manipulator {

struct RtuDiagnostics {
    stm32::drivers::UartStatistics uart;
    uint32_t timer_interrupts = 0;
    uint32_t frames = 0;
    uint32_t last_size = 0;
    uint32_t crc_errors = 0;
    uint32_t dropped_frames = 0;
};

// Owns framing and task synchronization; no registers, HAL or nanoMODBUS here.
// The two device IRQs use the same priority so framer producer calls serialize.
class FreeRtosRtuTransport final : public ByteStream {
public:

    bool initialize(uint32_t baud, bool even_parity);
    void acquire_frame();
    int32_t read(uint8_t*, uint16_t, int32_t timeout_ms) override;
    int32_t write(const uint8_t*, uint16_t, int32_t timeout_ms) override;
    void discard() override;
    RtuDiagnostics diagnostics();

private:

    static void received(void*, const stm32::drivers::ReceivedByte&);
    static void silence(void*);

    stm32::drivers::Usart2 uart_;
    stm32::drivers::Timer2 timer_;
    rtu::Framer framer_;
    uint8_t frame_[256]{};
    uint16_t size_ = 0, position_ = 0;
    uint32_t frames_ = 0, last_size_ = 0, crc_errors_ = 0;
    volatile uint32_t timer_interrupts_ = 0;
    TaskHandle_t owner_ = nullptr;
};

} // namespace manipulator
