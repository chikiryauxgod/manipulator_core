#pragma once
#include <array>
#include <cstdint>
#include <manipulator/inverse.h>
#include <manipulator/transform.h>
namespace manipulator {
constexpr std::size_t max_joints=6;
struct Configuration {
    std::size_t count=0;
    std::array<k_dh_joint,max_joints> geometry{};
    std::array<k_joint_limits,max_joints> limits{};
    k_transform base{}, tool{};
    Configuration() { k_transform_identity(&base); k_transform_identity(&tool); }
};
struct Request {
    uint32_t id=0;
    k_vec3 target{};
    std::array<k_real,max_joints> initial{};
};
struct Result {
    uint32_t id=0;
    k_status status=K_NOT_READY;
    std::array<k_real,max_joints> angles{};
    k_ik_report report{};
};
class Solver final {
public:
    Solver()=default;
    Solver(const Solver&)=delete;
    Solver& operator=(const Solver&)=delete;
    // Invalid configuration never replaces the active model.
    k_status configure(const Configuration& configuration);
    Result solve(const Request& request);
private:
    k_model model_{};
    std::array<k_joint_config,max_joints> storage_{};
    std::array<k_real,6*max_joints> workspace_{};
    bool ready_=false;
};
}
