#pragma once

#include <cstdint>

namespace manipulator::stm32::drivers {

// Free-running 1 MHz TIM2 plus one-shot compare alarm, wrap-safe uint32 ticks.
class Timer2 final {
public:

    using AlarmHandler = void (*)(void*);

    Timer2() = default;
    Timer2(const Timer2&) = delete;
    Timer2& operator=(const Timer2&) = delete;

    bool initialize(AlarmHandler handler, void* context, uint32_t priority);
    uint32_t now() const;
    void arm_after(uint32_t microseconds);
    void handle_interrupt();

private:

    AlarmHandler handler_ = nullptr;
    void* context_ = nullptr;
};

} // namespace manipulator::stm32::drivers
