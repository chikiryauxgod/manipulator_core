#include "application/controller.hpp"
#include <manipulator/forward.h>
#include <cmath>
#include <cstdlib>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            std::abort();                                                                          \
    } while (false)
using namespace manipulator;

static Configuration config() {
    Configuration c;
    c.count = 3;
    c.geometry[0] = {K_REVOLUTE, 0, 1.57079632679f, 0.1f, 0};

    c.geometry[1] = {K_REVOLUTE, 0.3f, 0, 0, 0};

    c.geometry[2] = {K_REVOLUTE, 0.2f, 0, 0, 0};

    for (auto& limit : c.limits)
        limit = {K_BOUNDED, -3.14159265f, 3.14159265f};

    return c;
}

int main() {
    Controller controller;
    Request request{7, {0.3f, 0, 0.3f}, {0.1f, 0.2f, 0.3f}};

    CHECK(controller.submit(request) == Controller::Admission::not_ready);
    auto c = config();
    auto invalid = c;
    invalid.count = 7;
    CHECK(controller.activate(invalid) == K_SIZE_MISMATCH);
    CHECK(controller.activate(c) == K_OK);
    CHECK(controller.activate(c) == K_INVALID_STATE);
    for (unsigned i = 0; i < 8; ++i) {
        request.id = i;
        CHECK(controller.submit(request) == Controller::Admission::accepted);
    }

    CHECK(controller.submit(request) == Controller::Admission::full);
    for (unsigned i = 0; i < 8; ++i)
        CHECK(controller.process_one());
    request.id = 99;
    CHECK(controller.submit(request) == Controller::Admission::accepted);
    CHECK(!controller.process_one());
    // unread results never overwritten
    Result result;
    CHECK(controller.peek_result(result) && result.id == 0 && result.status == K_OK);
    CHECK(result.report.residual_valid && result.report.residual <= 2e-5f);
    // Independently verify returned joint coordinates through forward kinematics.
    k_model model{};
    k_joint_config storage[6]{};

    CHECK(k_model_begin(&model, storage, 6, 3) == K_OK);
    for (unsigned i = 0; i < 3; ++i) {
        CHECK(k_model_set_joint(&model, i, &c.geometry[i]) == K_OK);
        CHECK(k_model_set_limits(&model, i, &c.limits[i]) == K_OK);
    }

    CHECK(k_model_set_base(&model, &c.base) == K_OK);
    CHECK(k_model_set_tool(&model, &c.tool) == K_OK);
    CHECK(k_model_finalize(&model) == K_OK);
    k_transform pose;
    CHECK(k_forward(&model, result.angles.data(), 3, &pose) == K_OK);
    CHECK(std::hypot(pose.m[0][3] - 0.3f, pose.m[1][3], pose.m[2][3] - 0.3f) <= 2e-5f);
    CHECK(!controller.acknowledge(123));
    CHECK(controller.acknowledge(0));
    CHECK(controller.process_one());
    controller.begin_configuration();
    CHECK(!controller.peek_result(result) && !controller.process_one());
    CHECK(controller.activate(c) == K_OK);
    request.initial[0] = 10;
    CHECK(controller.submit(request) == Controller::Admission::accepted);
    CHECK(controller.process_one());
    CHECK(controller.peek_result(result));
    CHECK(result.status == K_JOINT_LIMIT);
    Solver solver;
    CHECK(solver.configure(c) == K_OK);
    invalid = c;
    invalid.geometry[0].a = NAN;
    CHECK(solver.configure(invalid) != K_OK);
    request.initial = {0.1f, 0.2f, 0.3f};

    CHECK(solver.solve(request).status == K_OK);
    // rejected config preserved model
}
