#include "application/engine.hpp"
#include "protocol/modbus/registers.hpp"

#include <cstdlib>

#define CHECK(x) do { if (!(x)) std::abort(); } while (false)

using namespace manipulator;
using namespace manipulator::modbus;

struct MemoryStore : ConfigurationStore {
    bool valid = false, fail = false;
    SavedConfiguration data;

    bool load(SavedConfiguration& out) override
    {
        if (!valid) return false;
        out = data;
        return true;
    }

    bool save(const SavedConfiguration& value) override
    {
        if (fail) return false;
        data = value;
        valid = true;
        return true;
    }
};

int main()
{
    MemoryStore store;
    store.valid = true;
    store.data.model[0] = 3;
    for (unsigned i = 0; i < 3; ++i) {
        encode_float(0.1f, store.data.model.data() + 1 + i * 16);
        encode_float(-3.f, store.data.model.data() + 9 + i * 16);
        encode_float(3.f, store.data.model.data() + 11 + i * 16);
    }
    for (unsigned i = 0; i < 4; ++i) {
        encode_float(1.f, store.data.model.data() + 97 + i * 10);
        encode_float(1.f, store.data.model.data() + 129 + i * 10);
    }

    Engine engine;
    Registers bank(engine, &store);
    CHECK(bank.restore());
    CHECK(!engine.snapshot().runtime); // load never starts runtime implicitly
    uint16_t command = 2;
    CHECK(bank.write_holding(0, 1, &command) == Exception::none);
    Engine::Work work;
    Solver solver;
    CHECK(engine.take_work(work));
    const auto status = solver.configure(work.model);
    CHECK(status == K_OK);
    engine.complete(work, status, {});
    uint16_t settings[4] = {17, 0, 57600, 0};
    CHECK(bank.write_holding(0x300, 4, settings) == Exception::none);
    uint16_t readback[4];
    CHECK(bank.read_holding(0x300, 4, readback) == Exception::none);
    CHECK(readback[0] == 17);
    command = 5;
    CHECK(bank.write_holding(0, 1, &command) == Exception::none);
    CHECK(store.data.slave == 17 && store.data.baud == 57600 && !store.data.even_parity);
    store.fail = true;
    CHECK(bank.write_holding(0, 1, &command) == Exception::failure);
    settings[0] = 0;
    CHECK(bank.write_holding(0x300, 4, settings) == Exception::none);
    CHECK(bank.write_holding(0, 1, &command) == Exception::value);
    command = 1;
    CHECK(bank.write_holding(0, 1, &command) == Exception::none);
    command = 6;
    CHECK(bank.write_holding(0, 1, &command) == Exception::none);
    CHECK(bank.settings().slave == 17 && !engine.snapshot().runtime);
}
