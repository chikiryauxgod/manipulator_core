#pragma once

#include "storage/word_storage.hpp"

namespace manipulator::stm32::drivers {

// Sectors 4 and 5 reserved by the linker. No model/CRC/sequence policy here.
class ConfigurationFlash final : public WordStorage {
public:

    bool read(unsigned slot, uint32_t* words, std::size_t count) const override;
    bool write(unsigned slot, const uint32_t* words, std::size_t count) override;
};

}
