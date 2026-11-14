#include "solver.hpp"
#include "options.hpp"

namespace manipulator {

k_status Solver::configure(const Configuration& c) {
    if (c.count < 3 || c.count > max_joints) {
        return K_SIZE_MISMATCH;
    }

    k_model candidate{};
    std::array<k_joint_config, max_joints> temporary{};
    auto status = k_model_begin(&candidate, temporary.data(), temporary.size(), c.count);
    for (std::size_t i = 0; i < c.count && status == K_OK; ++i) {
        if (c.geometry[i].type != K_REVOLUTE) {
            return K_INVALID_MODEL;
        }

        status = k_model_set_joint(&candidate, i, &c.geometry[i]);
        if (status == K_OK) {
            status = k_model_set_limits(&candidate, i, &c.limits[i]);
        }
    }

    if (status == K_OK) {
        status = k_model_set_base(&candidate, &c.base);
    }

    if (status == K_OK) {
        status = k_model_set_tool(&candidate, &c.tool);
    }

    if (status == K_OK) {
        status = k_model_finalize(&candidate);
    }

    if (status != K_OK) {
        return status;
    }

    storage_ = temporary;
    model_ = candidate;
    model_.joints = storage_.data();
    ready_ = true;
    return K_OK;
}

Result Solver::solve(const Request& request) {
    Result result{};
    result.id = request.id;
    if (!ready_) {
        return result;
    }

    std::array<k_real, max_joints> scales{}, steps{};
    scales.fill(IkDefaults::joint_scale);
    steps.fill(IkDefaults::max_step);
    const k_ik_options options{IkDefaults::iterations, IkDefaults::backtracking,
                               IkDefaults::tolerance,  IkDefaults::length_scale,
                               IkDefaults::damping,    scales.data(),
                               steps.data(),           model_.joint_count};

    result.status =
        k_inverse_position(&model_, &request.target, request.initial.data(), model_.joint_count,
                           &options, workspace_.data(), workspace_.size(), result.angles.data(),
                           result.angles.size(), &result.report);
    return result;
}
} // namespace manipulator
