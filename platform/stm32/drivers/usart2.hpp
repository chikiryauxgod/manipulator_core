#pragma once

#include <cstdint>

namespace manipulator::stm32::drivers {

enum class SerialFormat { eight_n_one, eight_n_two, eight_e_one };

struct ReceivedByte {
    uint8_t value = 0;
    bool has_byte = false;
    bool error = false;
};

struct UartStatistics {
    uint32_t interrupts = 0;
    uint32_t received = 0;
    uint32_t errors = 0;
    uint32_t last_status = 0;
    uint32_t last_byte = 0;
    uint32_t transmitted = 0;
};

// Board binding: USART2 on PA2/PA3 AF7. One static-lifetime instance owns IRQ.
// No RTOS, protocol or application dependencies. Polling and IRQ are exclusive.
class Usart2 final {
public:

    using ReceiveHandler = void (*)(void*, const ReceivedByte&);

    Usart2() = default;
    Usart2(const Usart2&) = delete;
    Usart2& operator=(const Usart2&) = delete;

    bool initialize(uint32_t baud, SerialFormat format);
    void enable_receive_interrupt(ReceiveHandler handler, void* context, uint32_t priority);
    bool try_receive(ReceivedByte& byte);
    bool try_transmit(uint8_t byte);
    bool write(const uint8_t* bytes, uint16_t size, uint32_t timeout_ms);
    UartStatistics statistics() const;
    void handle_interrupt();

private:

    ReceiveHandler receive_handler_ = nullptr;
    void* receive_context_ = nullptr;
    volatile UartStatistics statistics_{};
};

} // namespace manipulator::stm32::drivers
