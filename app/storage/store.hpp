#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include "kinematics/options.hpp"

namespace manipulator {

struct SavedConfiguration {
    std::array<uint16_t, 161> model{};
    uint16_t slave = 1;
    uint32_t baud = 115200;
    bool even_parity = true;
};

class ConfigurationStore {
public:

    virtual ~ConfigurationStore() = default;
    virtual bool load(SavedConfiguration&) = 0;
    virtual bool save(const SavedConfiguration&) = 0;
};

// Explicit uint32 word format; no structure-layout or k_real ABI dependence.
struct ConfigurationRecord {
    static constexpr uint32_t magic = 0x4d434631;
    static constexpr std::size_t words = 176;
    using Data = std::array<uint32_t, words>;

    static uint32_t crc(const Data& data)
    {
        uint32_t result = 0xffffffffU;
        for (std::size_t i = 0; i < words - 2; ++i) {
            for (unsigned byte = 0; byte < 4; ++byte) {
                result ^= (data[i] >> (8 * byte)) & 255U;
                for (unsigned bit = 0; bit < 8; ++bit) {
                    result = (result >> 1) ^ ((result & 1) ? 0xedb88320U : 0U);
                }
            }
        }
        return ~result;
    }

    static std::array<uint32_t, 7> solver_parameters()
    {
        std::array<uint32_t, 7> result{IkDefaults::iterations, IkDefaults::backtracking};
        const float values[] = {IkDefaults::tolerance, IkDefaults::length_scale,
                                IkDefaults::damping, IkDefaults::joint_scale, IkDefaults::max_step};
        for (unsigned i = 0; i < 5; ++i) {
            std::memcpy(&result[2 + i], &values[i], sizeof(float));
        }
        return result;
    }

    static Data encode(const SavedConfiguration& config, uint32_t sequence)
    {
        Data data{};
        data[0] = magic;
        data[1] = 1;
        data[2] = sequence;
        data[3] = config.slave;
        data[4] = config.baud;
        data[5] = config.even_parity;
        for (std::size_t i = 0; i < config.model.size(); ++i) {
            data[6 + i] = config.model[i];
        }
        const auto parameters = solver_parameters();
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            data[167 + i] = parameters[i];
        }
        data[words - 2] = crc(data);
        data[words - 1] = 0; // commit marker, programmed LAST
        return data;
    }

    static bool decode(const Data& data, SavedConfiguration& config)
    {
        if (data[0] != magic || data[1] != 1 || data[words - 1] != 0 ||
            data[words - 2] != crc(data) || data[3] < 1 || data[3] > 247 ||
            data[4] < 38400 || data[4] > 115200 || data[5] > 1) {
            return false;
        }

        const auto parameters = solver_parameters();
        for (std::size_t i = 0; i < parameters.size(); ++i) {
            if (data[167 + i] != parameters[i]) {
                return false; // firmware changed solver policy: reinitialize explicitly
            }
        }

        SavedConfiguration candidate;
        candidate.slave = data[3];
        candidate.baud = data[4];
        candidate.even_parity = data[5] != 0;
        for (std::size_t i = 0; i < candidate.model.size(); ++i) {
            if (data[6 + i] > 65535) {
                return false;
            }
            candidate.model[i] = data[6 + i];
        }
        config = candidate;
        return true;
    }
};

}
