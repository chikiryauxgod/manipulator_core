#pragma once

#include "service.hpp"
#include <array>

namespace manipulator {

// Transport/RTOS-independent mailbox. All methods must be serialized externally.
// Work owns its inputs; heavy Solver calls happen outside mailbox locks.
class Engine final : public Service {
public:

    static constexpr std::size_t capacity = 8;

    struct Work {
        uint64_t epoch = 0;
        bool configuration = false;
        Configuration model{};
        Request request{};
    };

    ServiceStatus begin_configuration() override {
        ++epoch_;
        runtime_ = configuring_ = false;
        pending_model_ = false;
        head_ = count_ = result_head_ = result_count_ = 0;
        config_status_ = K_NOT_READY;
        return ServiceStatus::ok;
    }

    ServiceStatus activate(const Configuration& c) override {
        if (runtime_ || configuring_) {
            return ServiceStatus::not_ready;
        }

        model_ = c;
        pending_model_ = configuring_ = true;
        config_status_ = K_NOT_READY;
        return ServiceStatus::pending;
    }

    ServiceStatus submit(const Request& r) override {
        if (!runtime_) {
            return ServiceStatus::not_ready;
        }

        if (count_ == capacity) {
            return ServiceStatus::full;
        }

        requests_[(head_ + count_) % capacity] = r;
        ++count_;
        return ServiceStatus::ok;
    }

    ServiceStatus acknowledge(uint32_t id) override {
        if (!result_count_ || results_[result_head_].id != id) {
            return ServiceStatus::invalid;
        }

        result_head_ = (result_head_ + 1) % capacity;
        --result_count_;
        return ServiceStatus::ok;
    }

    Snapshot snapshot() override {
        Snapshot s;
        s.runtime = runtime_;
        s.configuring = configuring_;
        s.busy = busy_;
        s.queued = static_cast<uint16_t>(count_);
        s.results = static_cast<uint16_t>(result_count_);
        s.configuration_status = config_status_;
        s.has_result = result_count_ != 0;
        if (s.has_result) {
            s.result = results_[result_head_];
        }

        return s;
    }

    bool take_work(Work& w) {
        if (busy_) {
            return false;
        }

        if (pending_model_) {
            w.epoch = epoch_;
            w.configuration = true;
            w.model = model_;
            pending_model_ = false;
            busy_ = true;
            return true;
        }

        if (!runtime_ || !count_ || result_count_ == capacity) {
            return false;
        }

        w.epoch = epoch_;
        w.configuration = false;
        w.request = requests_[head_];
        head_ = (head_ + 1) % capacity;
        --count_;
        busy_ = true;
        return true;
    }

    // Exactly one worker calls complete once for each take_work.
    void complete(const Work& w, k_status configuration_status, const Result& result) {
        busy_ = false;
        if (w.epoch != epoch_) {
            return;
        }
        // cancelled session, including configuration
        if (w.configuration) {
            configuring_ = false;
            config_status_ = configuration_status;
            runtime_ = configuration_status == K_OK;
        } else {
            results_[(result_head_ + result_count_) % capacity] = result;
            ++result_count_;
        }
    }

private:

    uint64_t epoch_ = 0;
    bool runtime_ = false, configuring_ = false, pending_model_ = false, busy_ = false;
    k_status config_status_ = K_NOT_READY;
    Configuration model_{};
    std::array<Request, capacity> requests_{};
    std::array<Result, capacity> results_{};
    std::size_t head_ = 0, count_ = 0, result_head_ = 0, result_count_ = 0;
};

} // namespace manipulator
