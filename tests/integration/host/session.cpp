#include "application/engine.hpp"
#include "protocol/modbus/server.hpp"

#include <array>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <vector>

#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            std::abort();                                                                          \
    } while (false)

using namespace manipulator;
using namespace manipulator::modbus;

namespace {

uint16_t crc(const std::vector<uint8_t>& bytes) {
    uint16_t result = 0xffff;
    for (auto byte : bytes) {
        result ^= byte;
        for (unsigned i = 0; i < 8; ++i) {
            result = (result >> 1) ^ ((result & 1) ? 0xa001 : 0);
        }
    }
    return result;
}

struct Wire : ByteStream {
    std::vector<uint8_t> input, output;
    std::size_t position = 0;

    static int32_t read_callback(uint8_t* buffer, uint16_t count, int32_t, void* argument) {
        auto& wire = *static_cast<Wire*>(argument);
        const auto size = std::min<std::size_t>(count, wire.input.size() - wire.position);
        std::memcpy(buffer, wire.input.data() + wire.position, size);
        wire.position += size;
        return size;
    }

    static int32_t write_callback(const uint8_t* buffer, uint16_t count, int32_t, void* argument) {
        auto& wire = *static_cast<Wire*>(argument);
        wire.output.insert(wire.output.end(), buffer, buffer + count);
        return count;
    }

    int32_t read(uint8_t* data, uint16_t count, int32_t timeout) override {
        return read_callback(data, count, timeout, this);
    }

    int32_t write(const uint8_t* data, uint16_t count, int32_t timeout) override {
        return write_callback(data, count, timeout, this);
    }

    void discard() override {
        position = input.size();
    }

    void send(Server& server, std::vector<uint8_t> bytes) {
        const auto checksum = crc(bytes);
        bytes.push_back(checksum & 255);
        bytes.push_back(checksum >> 8);
        input = bytes;
        output.clear();
        position = 0;
        CHECK(server.poll() == NMBS_ERROR_NONE);
        CHECK(output.size() >= 5 && crc(output) == 0);
        CHECK(output[1] == input[1]); // no exception expected
    }

    void command(Server& server, uint16_t value) {
        send(server, {1, 6, 0, 0, uint8_t(value >> 8), uint8_t(value)});
    }

    void words(Server& server, uint16_t address, const uint16_t* words, uint16_t count) {
        CHECK(count <= 123);
        std::vector<uint8_t> bytes{1,
                                   16,
                                   uint8_t(address >> 8),
                                   uint8_t(address),
                                   uint8_t(count >> 8),
                                   uint8_t(count),
                                   uint8_t(count * 2)};
        for (unsigned i = 0; i < count; ++i) {
            bytes.push_back(words[i] >> 8);
            bytes.push_back(words[i]);
        }
        send(server, bytes);
    }
};

} // namespace

int main() {
    Engine engine;
    Registers bank(engine);
    Server server(bank);
    Wire wire;
    CHECK(server.initialize(1, wire) == NMBS_ERROR_NONE);
    server.timeouts(0, 0);

    wire.command(server, 1);
    std::array<uint16_t, model_words> model{};
    model[0] = 3;
    for (unsigned i = 0; i < 3; ++i) {
        encode_float(0.1f, model.data() + 1 + i * 16);
        encode_float(-3.f, model.data() + 9 + i * 16);
        encode_float(3.f, model.data() + 11 + i * 16);
    }
    for (unsigned i = 0; i < 4; ++i) {
        encode_float(1.f, model.data() + 97 + i * 10);
        encode_float(1.f, model.data() + 129 + i * 10);
    }
    wire.words(server, model_address, model.data(), 100);
    wire.words(server, model_address + 100, model.data() + 100, 61);
    wire.command(server, 2);
    Engine::Work work;
    Solver solver;
    CHECK(engine.take_work(work));
    const auto configured = solver.configure(work.model);
    CHECK(configured == K_OK);
    engine.complete(work, configured, {});

    std::array<uint16_t, request_words> request{};
    encode_u32(12345, request.data());
    encode_float(0.3f, request.data() + 2);
    wire.words(server, request_address, request.data(), request.size());
    wire.command(server, 3);
    CHECK(engine.take_work(work));
    const auto result = solver.solve(work.request);
    CHECK(result.id == 12345 && result.status == K_OK);
    engine.complete(work, K_OK, result);
    wire.send(server, {1, 4, 0, 0, 0, 24});
    CHECK(wire.output.size() == 53);
    CHECK(wire.output[3 + 6 * 2 + 1] == 1); // has_result
    CHECK(wire.output[3 + 9 * 2 + 1] == K_OK);
    wire.words(server, request_address, request.data(), 2);
    wire.command(server, 4);
    CHECK(!engine.snapshot().has_result);
}
