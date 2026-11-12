#include "protocol/modbus/registers.hpp"
#include "application/engine.hpp"
#include <cstdlib>
#define CHECK(x) do { if(!(x)) std::abort(); } while(false)
using namespace manipulator;
using namespace manipulator::modbus;
static void command(Registers& bank, Command c) {
    const auto word = static_cast<uint16_t>(c);
    CHECK(bank.write_holding(0, 1, &word) == Exception::none);
}

static void stage(Registers& bank) {
    uint16_t words[model_words] {};

    words[0] = 3;
    for (unsigned i = 0; i < 3; ++i) {
        encode_float(0.1f, words + 1 + i * 16);
        encode_float( - 3.14f, words + 9 + i * 16);
        encode_float(3.14f, words + 11 + i * 16);
    }

    for (unsigned i = 0; i < 4; ++i) {
        encode_float(1, words + 97 + 2 * (i * 4 + i));
        encode_float(1, words + 129 + 2 * (i * 4 + i));
    }

    CHECK(bank.write_holding(model_address, 100, words) == Exception::none);
    CHECK(bank.write_holding(model_address + 100, 61, words + 100) == Exception::none);
}

int main() {
    Engine engine;
    Registers bank(engine);
    uint16_t word = 2, out[24] {};

    CHECK(bank.write_holding(0, 1, &word) == Exception::value);
    // incomplete
    CHECK(bank.write_holding(0xffff, 1, &word) == Exception::address);
    CHECK(bank.read_input(23, 2, out) == Exception::address);
    stage(bank);
    command(bank, Command::apply);
    CHECK(engine.snapshot().configuring);
    Engine::Work work;
    Solver solver;
    CHECK(engine.take_work(work));
    auto status = solver.configure(work.model);
    CHECK(status == K_OK);
    engine.complete(work, status, {});
    CHECK(engine.snapshot().runtime);
    CHECK(bank.write_holding(model_address, 1, &word) == Exception::busy);
    uint16_t request[request_words] {};

    encode_u32(0x12345678, request);
    encode_float(0.3f, request + 2);
    CHECK(bank.write_holding(request_address, request_words, request) == Exception::none);
    command(bank, Command::submit);
    word = 3;
    CHECK(bank.write_holding(0, 1, &word) == Exception::value);
    // stale input mask
    CHECK(engine.take_work(work));
    auto result = solver.solve(work.request);
    CHECK(result.status == K_OK);
    engine.complete(work, K_OK, result);
    CHECK(bank.read_input(0, 24, out) == Exception::none);
    CHECK(out[6] == 1 && decode_u32(out + 7) == 0x12345678 && out[9] == K_OK);
    CHECK(bank.write_holding(request_address, 2, request) == Exception::none);
    command(bank, Command::acknowledge);
    CHECK(!engine.snapshot().has_result);
    CHECK(bank.write_holding(request_address, 20, request) == Exception::none);
    command(bank, Command::submit);
    CHECK(engine.take_work(work));
    command(bank, Command::begin);
    // IK is in flight
    stage(bank);
    command(bank, Command::apply);
    engine.complete(work, K_OK, result);
    // cancelled result must disappear
    CHECK(!engine.snapshot().has_result && engine.snapshot().configuring);
    CHECK(engine.take_work(work) && work.configuration);
    command(bank, Command::begin);
    // cancel configuration itself
    engine.complete(work, K_OK, {});
    CHECK(!engine.snapshot().runtime && !engine.snapshot().configuring);
    // Bounded queues and result backpressure using the actual mailbox.
    CHECK(engine.activate(Configuration{}) == ServiceStatus::pending);
    CHECK(engine.take_work(work));
    engine.complete(work, K_OK, {});
    Request r{};
    for (unsigned i = 0; i < 8; ++i) {
        r.id = i;
        CHECK(engine.submit(r) == ServiceStatus::ok);
    }

    CHECK(engine.submit(r) == ServiceStatus::full);
    for (unsigned i = 0; i < 8; ++i) {
        CHECK(engine.take_work(work));
        Result done{};
        done.id = work.request.id;
        engine.complete(work, K_OK, done);
    }

    CHECK(engine.submit(r) == ServiceStatus::ok);
    CHECK(!engine.take_work(work));
    CHECK(engine.acknowledge(0) == ServiceStatus::ok);
    CHECK(engine.take_work(work));
    engine.complete(work, K_OK, {});
    // Rejected FC16 does not partially mutate the last valid register.
    uint16_t before;
    CHECK(bank.read_holding(request_address + 19, 1, &before) == Exception::none);
    uint16_t invalid[2] = {42, 43};

    CHECK(bank.write_holding(request_address + 19, 2, invalid) == Exception::address);
    CHECK(bank.read_holding(request_address + 19, 1, out) == Exception::none && out[0] == before);
}
