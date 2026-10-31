#include "freertos/service.hpp"
#include <cstdlib>
#define CHECK(x) do { if(!(x)) std::abort(); } while(false)
using namespace manipulator;
static RtosService service;
static void client(void*) {
    Configuration c; c.count=3;
    for(unsigned i=0;i<3;++i) {
        c.geometry[i]={K_REVOLUTE,0.1f,0,0,0};
        c.limits[i]={K_BOUNDED,-3.14f,3.14f};
    }
    CHECK(service.activate(c)==ServiceStatus::pending);
    for(unsigned i=0;!service.snapshot().runtime;++i) {CHECK(i<1000); vTaskDelay(1);}
    Request r{12,{0.3f,0,0},{}};
    CHECK(service.submit(r)==ServiceStatus::ok);
    for(unsigned i=0;!service.snapshot().has_result;++i) {CHECK(i<1000); vTaskDelay(1);}
    auto s=service.snapshot(); CHECK(s.result.id==12 && s.result.status==K_OK);
    CHECK(service.acknowledge(12)==ServiceStatus::ok);
    CHECK(service.begin_configuration()==ServiceStatus::ok);
    CHECK(!service.snapshot().runtime && !service.snapshot().has_result);
    vTaskEndScheduler();
}
extern "C" void vApplicationGetIdleTaskMemory(StaticTask_t** t,StackType_t** s,StackType_t* n) {
    static StaticTask_t task;
    static StackType_t stack[configMINIMAL_STACK_SIZE];
    *t=&task; *s=stack; *n=configMINIMAL_STACK_SIZE;
}
int main() {
    static StaticTask_t worker, protocol;
    static StackType_t ws[configMINIMAL_STACK_SIZE],ps[configMINIMAL_STACK_SIZE];
    auto handle=xTaskCreateStatic(RtosService::worker_entry,"IK",configMINIMAL_STACK_SIZE,
        &service,1,ws,&worker);
    CHECK(handle); service.bind_worker(handle);
    CHECK(xTaskCreateStatic(client,"protocol",configMINIMAL_STACK_SIZE,nullptr,2,ps,&protocol));
    vTaskStartScheduler();
}
