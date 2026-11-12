#include "transport/rtu/framer.hpp"
#include "transport/rtu/crc.hpp"
#include "storage/store.hpp"

#include <cstdlib>

#define CHECK(x) do { if (!(x)) std::abort(); } while (false)

int main()
{
    const uint8_t query[] = {1, 3, 0, 0, 0, 10, 0xc5, 0xcd};
    CHECK(manipulator::rtu::crc16(query, sizeof(query)) == 0);
    CHECK(manipulator::rtu::crc16(query, sizeof(query) - 2) == 0xcdc5);

    manipulator::rtu::Framer framer;
    uint8_t data[256];
    uint16_t size;
    for (unsigned i = 0; i < 8; ++i) {
        framer.byte(i, i * 100);
    }
    framer.silence(2449);
    CHECK(!framer.pop(data, size));
    framer.silence(2450);
    CHECK(framer.pop(data, size) && size == 8 && data[7] == 7);

    // A gap between t1.5 and t3.5 invalidates the whole frame.
    framer.byte(1, 3000);
    framer.byte(2, 3800);
    framer.byte(3, 3900);
    framer.byte(4, 4000);
    framer.silence(6000);
    CHECK(!framer.pop(data, size));

    // Unsigned clock arithmetic works across the 32-bit timer wrap.
    for (unsigned i = 0; i < 4; ++i) {
        framer.byte(i, 0xffffff00U + i * 100U);
    }
    framer.silence(0xffffff00U + 300U + 1750U);
    CHECK(framer.pop(data, size) && size == 4);

    for (unsigned i = 0; i < 257; ++i) {
        framer.byte(0, 10000 + i * 10);
    }
    framer.silence(15000);
    CHECK(!framer.pop(data, size));

    for (unsigned frame = 0; frame < 5; ++frame) {
        for (unsigned i = 0; i < 4; ++i) {
            framer.byte(i, 20000 + frame * 3000 + i * 100);
        }
        framer.silence(20000 + frame * 3000 + 2100);
    }
    for (unsigned frame = 0; frame < 4; ++frame) {
        CHECK(framer.pop(data, size));
    }
    CHECK(!framer.pop(data, size) && framer.dropped() == 3);

    // End-of-character timestamps: an incomplete character near t3.5 cannot
    // cause a previously valid prefix to be dispatched too early.
    manipulator::rtu::Framer guarded;
    guarded.character_duration(100);
    for (unsigned i = 0; i < 4; ++i) guarded.byte(i, i * 100);
    guarded.silence(300 + 1750);
    CHECK(!guarded.pop(data, size));
    guarded.byte(4, 300 + 1740 + 100);
    guarded.silence(5000);
    CHECK(!guarded.pop(data, size));

    using Record = manipulator::ConfigurationRecord;
    manipulator::SavedConfiguration saved, decoded;
    saved.model[0] = 6;
    saved.model[160] = 65535;
    const auto encoded = Record::encode(saved, 100);
    CHECK(Record::decode(encoded, decoded));
    CHECK(decoded.model == saved.model && decoded.baud == saved.baud);
    for (std::size_t i = 0; i < encoded.size(); ++i) {
        auto corrupt = encoded;
        corrupt[i] ^= 1;
        CHECK(!Record::decode(corrupt, decoded));
    }
    auto unfinished = encoded;
    unfinished.back() = 0xffffffff;
    CHECK(!Record::decode(unfinished, decoded));
}
