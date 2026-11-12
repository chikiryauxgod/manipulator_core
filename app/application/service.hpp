#pragma once

#include "kinematics/solver.hpp"

namespace manipulator {

enum class ServiceStatus : uint16_t { ok, not_ready, full, invalid, incomplete, pending };

struct Snapshot {
    bool runtime = false, configuring = false, busy = false, has_result = false;
    uint16_t queued = 0, results = 0;
    k_status configuration_status = K_NOT_READY;
    Result result{};
};

// Called by the protocol task; implementations synchronize with the worker.
class Service {
public:

    virtual ~Service() = default;
    virtual ServiceStatus begin_configuration() = 0;
    virtual ServiceStatus activate(const Configuration&) = 0;
    virtual ServiceStatus submit(const Request&) = 0;
    virtual ServiceStatus acknowledge(uint32_t) = 0;
    virtual Snapshot snapshot() = 0;
};

}
