#include "application/engine.hpp"
#include "protocol/modbus/server.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <poll.h>
#include <unistd.h>

// Linux PTY transport for tests. It does not model UART timing or STM32 IRQs.
class PtyStream final : public manipulator::ByteStream {
public:

    explicit PtyStream(int fd) : fd_(fd) {
    }

    int32_t read(uint8_t* bytes, uint16_t count, int32_t timeout_ms) override {
        using Clock = std::chrono::steady_clock;
        const auto deadline = Clock::now() + std::chrono::milliseconds(timeout_ms);
        int32_t received = 0;
        while (received < count) {
            const auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now())
                    .count();
            pollfd descriptor{fd_, POLLIN, 0};
            const auto ready = ::poll(&descriptor, 1, std::max<int64_t>(0, remaining));
            if (ready < 0 && errno == EINTR) {
                continue;
            }
            if (ready < 0 || (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL))) {
                return -1;
            }
            if (ready == 0) {
                break;
            }
            const auto size = ::read(fd_, bytes + received, count - received);
            if (size < 0 && errno == EINTR) {
                continue;
            }
            if (size <= 0) {
                return -1;
            }
            received += size;
        }
        return received;
    }

    int32_t write(const uint8_t* bytes, uint16_t count, int32_t) override {
        int32_t written = 0;
        while (written < count) {
            const auto size = ::write(fd_, bytes + written, count - written);
            if (size < 0 && errno == EINTR) {
                continue;
            }
            if (size <= 0) {
                return -1;
            }
            written += size;
        }
        return written;
    }

    void discard() override {
    }

private:

    int fd_;
};

int main(int argc, char** argv) {
    if (argc != 2) {
        return 2;
    }
    PtyStream stream(std::atoi(argv[1]));
    manipulator::Engine engine;
    manipulator::Solver solver;
    manipulator::modbus::Registers registers(engine);
    manipulator::modbus::Server server(registers);
    if (server.initialize(1, stream) != NMBS_ERROR_NONE) {
        return 1;
    }
    server.timeouts(20, 100);
    for (;;) {
        const auto status = server.poll();
        if (status == NMBS_ERROR_TRANSPORT) {
            return 1;
        }
        manipulator::Engine::Work work;
        if (engine.take_work(work)) {
            if (work.configuration) {
                engine.complete(work, solver.configure(work.model), {});
            } else {
                engine.complete(work, K_OK, solver.solve(work.request));
            }
        }
    }
}
