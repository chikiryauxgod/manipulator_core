#include "storage/persistent_store.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition))                                                                          \
            std::abort();                                                                          \
    } while (false)

namespace {

using manipulator::ConfigurationRecord;
using manipulator::PersistentConfigurationStore;
using manipulator::SavedConfiguration;

class MemorySlots final : public manipulator::WordStorage {
public:

    MemorySlots() {
        for (auto& bank : banks) {
            bank.fill(0xffffffffU);
        }
    }

    bool read(unsigned slot, uint32_t* output, std::size_t count) const override {
        if (static_cast<int>(slot) == unreadable) {
            return false;
        }
        std::copy_n(banks.at(slot).data(), count, output);
        return true;
    }

    bool write(unsigned slot, const uint32_t* data, std::size_t count) override {
        ++writes;
        auto& bank = banks.at(slot);
        bank.fill(0xffffffffU);
        for (std::size_t i = 0; i < count; ++i) {
            if (static_cast<int>(i) == fail_after) {
                return false;
            }
            bank[i] = data[i];
        }
        if (corrupt_after_write) {
            bank[10] ^= 1;
        }
        return true;
    }

    std::array<ConfigurationRecord::Data, 2> banks;
    int fail_after = -1, unreadable = -1;
    unsigned writes = 0;
    bool corrupt_after_write = false;
};

} // namespace

int main() {
    SavedConfiguration original, updated, result;
    original.slave = 1;
    updated.slave = 7;

    MemorySlots memory;
    PersistentConfigurationStore store(memory);
    result.slave = 99;
    CHECK(!store.load(result) && result.slave == 99);
    CHECK(store.save(original));
    CHECK(store.save(updated));
    CHECK(store.load(result) && result.slave == 7);
    CHECK(memory.banks[0][2] == 0 && memory.banks[1][2] == 1);

    // Every incomplete programming boundary, including the final commit word,
    // must leave the older valid copy readable and byte-for-byte unchanged.
    for (unsigned boundary = 0; boundary < ConfigurationRecord::words; ++boundary) {
        MemorySlots failing;
        const auto old_copy = ConfigurationRecord::encode(original, 42);
        failing.banks[0] = old_copy;
        failing.fail_after = boundary;
        PersistentConfigurationStore storage(failing);
        CHECK(!storage.save(updated));
        CHECK(failing.banks[0] == old_copy);
        CHECK(storage.load(result) && result.slave == original.slave);
    }

    // CRC failure in the newer committed record falls back to the older copy.
    memory.banks[1][10] ^= 1;
    CHECK(store.load(result) && result.slave == 1);
    memory.corrupt_after_write = true;
    CHECK(!store.save(updated));
    CHECK(store.load(result) && result.slave == 1);

    // A read failure must not be interpreted as an empty slot and trigger erase.
    memory.unreadable = 0;
    const auto previous_writes = memory.writes;
    CHECK(!store.save(updated) && memory.writes == previous_writes);

    MemorySlots wrapped;
    wrapped.banks[0] = ConfigurationRecord::encode(original, std::numeric_limits<uint32_t>::max());
    wrapped.banks[1] = ConfigurationRecord::encode(updated, 0);
    PersistentConfigurationStore wrap_store(wrapped);
    CHECK(wrap_store.load(result) && result.slave == 7);
    CHECK(wrap_store.save(original) && wrapped.banks[0][2] == 1);
    CHECK(wrap_store.load(result) && result.slave == 1);

    SavedConfiguration invalid;
    invalid.slave = 0;
    const auto writes_before_invalid = wrapped.writes;
    CHECK(!wrap_store.save(invalid) && wrapped.writes == writes_before_invalid);
}
