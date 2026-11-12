#pragma once

#include <cstddef>
#include <cstdint>

namespace manipulator {

// Two independent erasable slots. write() replaces only the requested slot,
// programs words in order, and may fail after a partial write.
class WordStorage {
public:

    virtual ~WordStorage() = default;
    virtual bool read(unsigned slot, uint32_t* words, std::size_t count) const = 0;
    virtual bool write(unsigned slot, const uint32_t* words, std::size_t count) = 0;
};

}
