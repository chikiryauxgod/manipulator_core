#pragma once

#include "application/service.hpp"
#include "storage/store.hpp"
#include <array>
#include <cstring>

namespace manipulator::modbus {

// Zero-based Modbus addresses, never 4xxxx display notation.
constexpr uint16_t command_address = 0;
constexpr uint16_t model_address = 0x100, model_words = 161;
constexpr uint16_t request_address = 0x200, request_words = 20;
constexpr uint16_t input_words = 24;

enum class Exception : uint8_t { none = 0, address = 2, value = 3, failure = 4, busy = 4 };

enum class Command : uint16_t { begin = 1, apply = 2, submit = 3, acknowledge = 4, save = 5, load = 6 };

class Registers final {
public:

    explicit Registers(Service& service, ConfigurationStore* store = nullptr)
        : service_(service), store_(store) {
    }

    bool restore();
    const SavedConfiguration& settings() const { return saved_; }

    Exception read_holding(uint16_t address, uint16_t count, uint16_t* output) const;
    Exception write_holding(uint16_t address, uint16_t count, const uint16_t* input);
    Exception read_input(uint16_t address, uint16_t count, uint16_t* output);
private:

    Exception execute(uint16_t command);
    static Exception translate(ServiceStatus status);
    Service& service_;
    ConfigurationStore* store_;
    SavedConfiguration saved_{};
    std::array<uint16_t, 4> link_{{1, 1, 49664, 1}}; // ID, 115200 high/low, even

    std::array<uint16_t, model_words> model_{};
    std::array<bool, model_words> model_written_{};
    std::array<uint16_t, request_words> request_{};
    std::array<bool, request_words> request_written_{};
};

inline uint32_t decode_u32(const uint16_t* words) {
    return (uint32_t(words[0]) << 16)|words[1];
}

inline float decode_float(const uint16_t* words) {
    static_assert(sizeof(float) == 4);
    const uint32_t bits = decode_u32(words);
    float result;
    std::memcpy(&result, &bits, 4);
    return result;
}

inline void encode_u32(uint32_t value, uint16_t* words) {
    words[0] = static_cast<uint16_t>(value >> 16);
    words[1] = static_cast<uint16_t>(value);
}

inline void encode_float(float value, uint16_t* words) {
    uint32_t bits;
    std::memcpy(&bits, &value, 4);
    encode_u32(bits, words);
}
}
