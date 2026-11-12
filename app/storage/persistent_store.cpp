#include "persistent_store.hpp"

namespace manipulator {

bool PersistentConfigurationStore::newer(uint32_t a, uint32_t b)
{
    return a != b && uint32_t(a - b) < 0x80000000U;
}

bool PersistentConfigurationStore::load(SavedConfiguration& config)
{
    ConfigurationRecord::Data first{}, second{};
    SavedConfiguration a, b;
    const bool valid_a = storage_.read(0, first.data(), first.size()) &&
                         ConfigurationRecord::decode(first, a);
    const bool valid_b = storage_.read(1, second.data(), second.size()) &&
                         ConfigurationRecord::decode(second, b);
    if (!valid_a && !valid_b) {
        return false;
    }

    config = valid_b && (!valid_a || newer(second[2], first[2])) ? b : a;
    return true;
}

bool PersistentConfigurationStore::save(const SavedConfiguration& config)
{
    ConfigurationRecord::Data first{}, second{};
    // A read error is different from an erased slot: never erase on uncertain state.
    if (!storage_.read(0, first.data(), first.size()) ||
        !storage_.read(1, second.data(), second.size())) {
        return false;
    }

    SavedConfiguration temporary;
    const bool valid_a = ConfigurationRecord::decode(first, temporary);
    const bool valid_b = ConfigurationRecord::decode(second, temporary);
    const unsigned current = valid_b && (!valid_a || newer(second[2], first[2])) ? 1 : 0;
    const unsigned target = valid_a || valid_b ? 1 - current : 0;
    const uint32_t sequence = valid_a || valid_b ? (current ? second[2] : first[2]) + 1 : 0;
    const auto data = ConfigurationRecord::encode(config, sequence);

    if (!ConfigurationRecord::decode(data, temporary) ||
        !storage_.write(target, data.data(), data.size())) {
        return false;
    }

    ConfigurationRecord::Data verified{};
    return storage_.read(target, verified.data(), verified.size()) && verified == data;
}

}
