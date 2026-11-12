#pragma once

#include <cstddef>
#include <cstdint>

namespace manipulator::rtu {

inline uint16_t crc16(const uint8_t* bytes, std::size_t count)
{
    uint16_t crc = 0xffff;
    for (std::size_t i = 0; i < count; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xa001 : 0);
        }
    }
    return crc;
}

}
