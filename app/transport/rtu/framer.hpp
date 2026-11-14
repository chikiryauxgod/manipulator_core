#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace manipulator::rtu {

// Single interrupt-context producer. Consumer must exclude that interrupt.
// Fixed high-speed RTU timings (>19200 baud), in microseconds.
class Framer {
public:

    static constexpr uint32_t inter_character_us = 750;
    static constexpr uint32_t frame_gap_us = 1750;

    // RXNE timestamps the END of a character, not its start. A one-character
    // guard prevents accepting a frame while a byte is still on the wire.
    void character_duration(uint32_t microseconds) { character_us_ = microseconds; }
    uint32_t completion_delay() const { return frame_gap_us + character_us_; }

    void byte(uint8_t value, uint32_t now, bool error = false)
    {
        if (receiving_ && uint32_t(now - last_) >= completion_delay()) {
            finish();
        }

        if (receiving_ && uint32_t(now - last_) > inter_character_us + character_us_) {
            invalid_ = true;
        }

        receiving_ = true;
        last_ = now;
        invalid_ = invalid_ || error;

        if (length_ < current_.size()) {
            current_[length_++] = value;
        } else {
            invalid_ = true;
        }
    }

    // Return the remaining delay if a compare interrupt arrived before t3.5.
    uint32_t silence(uint32_t now)
    {
        if (receiving_) {
            const uint32_t elapsed = now - last_;
            if (elapsed < completion_delay()) {
                return completion_delay() - elapsed;
            }
            finish();
        }
        return 0;
    }

    bool pop(uint8_t* output, uint16_t& size)
    {
        if (count_ == 0) {
            return false;
        }

        size = sizes_[head_];
        for (uint16_t i = 0; i < size; ++i) {
            output[i] = frames_[head_][i];
        }

        head_ = (head_ + 1) % frames_.size();
        --count_;
        return true;
    }

    uint32_t dropped() const { return dropped_; }

private:

    void finish()
    {
        if (!invalid_ && length_ >= 4 && count_ < frames_.size()) {
            const auto tail = (head_ + count_) % frames_.size();
            frames_[tail] = current_;
            sizes_[tail] = length_;
            ++count_;
        } else {
            ++dropped_;
        }

        receiving_ = invalid_ = false;
        length_ = 0;
    }

    std::array<uint8_t, 256> current_{};
    std::array<std::array<uint8_t, 256>, 4> frames_{};
    std::array<uint16_t, 4> sizes_{};
    std::size_t head_ = 0, count_ = 0;
    uint16_t length_ = 0;
    uint32_t last_ = 0, dropped_ = 0, character_us_ = 0;
    bool receiving_ = false, invalid_ = false;
};

}
