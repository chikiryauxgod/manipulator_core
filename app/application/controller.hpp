#pragma once

#include "kinematics/solver.hpp"

namespace manipulator {

// Single-owner state machine. External task/adapter must serialize every call.
// process_one() is synchronous; no concurrent configure/solve calls are allowed.
class Controller final {
public:

    enum class Mode { configuring, runtime };

    enum class Admission { accepted, not_ready, full };

    void begin_configuration() noexcept {
        mode_ = Mode::configuring;
        request_head_ = request_count_ = result_head_ = result_count_ = 0;
    }

    k_status activate(const Configuration& c) {
        if (mode_ != Mode::configuring) {
            return K_INVALID_STATE;
        }

        const auto status = solver_.configure(c);
        if (status == K_OK) {
            mode_ = Mode::runtime;
        }

        return status;
    }

    Admission submit(const Request& request) noexcept {
        if (mode_ != Mode::runtime) {
            return Admission::not_ready;
        }

        if (request_count_ == capacity) {
            return Admission::full;
        }

        requests_[(request_head_ + request_count_) % capacity] = request;
        ++request_count_;
        return Admission::accepted;
    }

    bool process_one() {
        if (mode_ != Mode::runtime || !request_count_ || result_count_ == capacity) {
            return false;
        }

        results_[(result_head_ + result_count_) % capacity] =
            solver_.solve(requests_[request_head_]);
        ++result_count_;
        request_head_ = (request_head_ + 1) % capacity;
        --request_count_;
        return true;
    }

    // Reading does not remove the result; acknowledgement releases its slot.
    bool peek_result(Result& out) const noexcept {
        if (!result_count_) {
            return false;
        }

        out = results_[result_head_];
        return true;
    }

    bool acknowledge(uint32_t id) noexcept {
        if (!result_count_ || results_[result_head_].id != id) {
            return false;
        }

        result_head_ = (result_head_ + 1) % capacity;
        --result_count_;
        return true;
    }

    Mode mode() const noexcept {
        return mode_;
    }

    static constexpr std::size_t capacity = 8;

private:

    Solver solver_;
    Mode mode_ = Mode::configuring;
    std::array<Request, capacity> requests_{};
    std::array<Result, capacity> results_{};
    std::size_t request_head_ = 0, request_count_ = 0, result_head_ = 0, result_count_ = 0;
};

} // namespace manipulator
