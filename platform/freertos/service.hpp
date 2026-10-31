#pragma once
#include "application/engine.hpp"
#include <rtos_layer/mutex.hpp>
#include <rtos_layer/notification.hpp>
namespace manipulator {
// Static lifetime. Exactly one protocol task and one worker task.
// bind_worker() must run before requests or the scheduler are started.
class RtosService final : public Service {
public:
    void bind_worker(TaskHandle_t worker) { configASSERT(worker); worker_=worker; }
    ServiceStatus begin_configuration() override {
        auto status=locked([&]{return engine_.begin_configuration();}); wake(); return status;
    }
    ServiceStatus activate(const Configuration& c) override {
        auto status=locked([&]{return engine_.activate(c);}); wake(); return status;
    }
    ServiceStatus submit(const Request& r) override {
        auto status=locked([&]{return engine_.submit(r);}); wake(); return status;
    }
    ServiceStatus acknowledge(uint32_t id) override {
        auto status=locked([&]{return engine_.acknowledge(id);}); wake(); return status;
    }
    Snapshot snapshot() override { return locked([&]{return engine_.snapshot();}); }
    // Task entry argument points to this object. The caller creates static tasks.
    static void worker_entry(void* argument) {
        auto& self=*static_cast<RtosService*>(argument);
        for(;;) {
            Engine::Work work;
            while(self.locked([&]{return self.engine_.take_work(work);})) {
                Result result{};
                k_status status=K_NOT_READY;
                if(work.configuration) status=self.solver_.configure(work.model);
                else result=self.solver_.solve(work.request);
                self.locked([&]{self.engine_.complete(work,status,result); return true;});
            }
            // Notifications accumulate if a command arrives between take_work and take.
            rtos_layer::Notification::take(true,portMAX_DELAY);
        }
    }
private:
    template<class F> auto locked(F action) -> decltype(action()) {
        rtos_layer::MutexGuard guard(mutex_,portMAX_DELAY);
        configASSERT(guard.owns_lock());
        return action();
    }
    void wake() { configASSERT(worker_); rtos_layer::Notification(worker_).give(); }
    rtos_layer::StaticMutex mutex_;
    Engine engine_;
    Solver solver_; // owned exclusively by worker; IK never holds mutex
    TaskHandle_t worker_=nullptr;
};
}
