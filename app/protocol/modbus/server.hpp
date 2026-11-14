#pragma once

#include "registers.hpp"
#include "transport/byte_stream.hpp"
#include <nanomodbus.h>

namespace manipulator::modbus {

// Owns the nanoMODBUS binding. Single protocol-task owner.
class Server final {
public:

    explicit Server(Registers& registers) : registers_(registers) {
    }

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    nmbs_error initialize(uint8_t address, ByteStream& stream) {
        nmbs_platform_conf platform;
        nmbs_platform_conf_create(&platform);
        platform.transport = NMBS_TRANSPORT_RTU;
        platform.arg = &stream;
        platform.read = [](uint8_t* bytes, uint16_t count, int32_t timeout, void* context) {
            return static_cast<ByteStream*>(context)->read(bytes, count, timeout);
        };
        platform.write = [](const uint8_t* bytes, uint16_t count, int32_t timeout, void* context) {
            return static_cast<ByteStream*>(context)->write(bytes, count, timeout);
        };
        platform.flush = [](nmbs_t*, void* context) {
            static_cast<ByteStream*>(context)->discard();
        };
        return initialize(address, platform);
    }

    nmbs_error initialize(uint8_t address, const nmbs_platform_conf& platform) {
        if (address < 1 || address > 247 || platform.transport != NMBS_TRANSPORT_RTU) {
            return NMBS_ERROR_INVALID_ARGUMENT;
        }
        nmbs_callbacks callbacks;
        nmbs_callbacks_create(&callbacks);
        callbacks.arg = &registers_;
        callbacks.read_holding_registers = [](uint16_t a, uint16_t n, uint16_t* out, uint8_t,
                                              void* p) {
            return static_cast<nmbs_error>(static_cast<Registers*>(p)->read_holding(a, n, out));
        };

        callbacks.read_input_registers = [](uint16_t a, uint16_t n, uint16_t* out, uint8_t,
                                            void* p) {
            return static_cast<nmbs_error>(static_cast<Registers*>(p)->read_input(a, n, out));
        };

        callbacks.write_single_register = [](uint16_t a, uint16_t v, uint8_t, void* p) {
            return static_cast<nmbs_error>(static_cast<Registers*>(p)->write_holding(a, 1, &v));
        };

        callbacks.write_multiple_registers = [](uint16_t a, uint16_t n, const uint16_t* v, uint8_t,
                                                void* p) {
            return static_cast<nmbs_error>(static_cast<Registers*>(p)->write_holding(a, n, v));
        };

        auto status = nmbs_server_create(&instance_, address, &platform, &callbacks);
        initialized_ = status == NMBS_ERROR_NONE;
        return status;
    }

    nmbs_error poll() {
        return initialized_ ? nmbs_server_poll(&instance_) : NMBS_ERROR_INVALID_ARGUMENT;
    }

    void timeouts(int32_t read_ms, int32_t byte_ms) {
        nmbs_set_read_timeout(&instance_, read_ms);
        nmbs_set_byte_timeout(&instance_, byte_ms);
    }

private:

    Registers& registers_;
    nmbs_t instance_{};
    bool initialized_ = false;
};

} // namespace manipulator::modbus
