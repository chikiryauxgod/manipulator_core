#pragma once

#include <cstdint>

namespace manipulator {

// Protocol boundary: blocking/timeout behavior belongs to the implementation.
// Framed RTU implementations only expose bytes from one acquired ADU at a time.
class ByteStream {
public:

    virtual ~ByteStream() = default;
    virtual int32_t read(uint8_t* bytes, uint16_t count, int32_t timeout_ms) = 0;
    virtual int32_t write(const uint8_t* bytes, uint16_t count, int32_t timeout_ms) = 0;
    virtual void discard() = 0;
};

}
