#pragma once

#include <cstdint>

namespace manipulator {

struct IkDefaults {
    static constexpr uint32_t iterations = 300;
    static constexpr uint32_t backtracking = 16;
    static constexpr float tolerance = 2e-5f;
    static constexpr float length_scale = 1.f;
    static constexpr float damping = 0.005f;
    static constexpr float joint_scale = 1.f;
    static constexpr float max_step = 0.2f;
};

} // namespace manipulator
