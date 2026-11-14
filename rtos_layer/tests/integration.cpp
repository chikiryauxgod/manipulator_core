#include <rtos_layer/queue.hpp>
#include <rtos_layer/mutex.hpp>
#include <rtos_layer/notification.hpp>
#include <cstdlib>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x))                                                                                  \
            std::abort();                                                                          \
    } while (false)
static rtos_layer::StaticQueue<int, 2> messages;
static rtos_layer::StaticMutex mutex;
static TaskHandle_t consumer_handle;

static void consumer(void*) {
    int value = 0;
    CHECK(messages.receive(value, portMAX_DELAY) && value == 42);
    CHECK(rtos_layer::Notification::take(true, portMAX_DELAY) == 1);
    {
        rtos_layer::MutexGuard guard(mutex, portMAX_DELAY);
        CHECK(guard.owns_lock());
    }

    CHECK(!messages.receive(value, 2));
    // real scheduler timeout
    vTaskEndScheduler();
}

static void producer(void*) {
    CHECK(messages.send(42, portMAX_DELAY));
    rtos_layer::Notification(consumer_handle).give();
    vTaskSuspend(nullptr);
}

extern "C" void vApplicationGetIdleTaskMemory(StaticTask_t** task, StackType_t** stack,
                                              StackType_t* size) {
    static StaticTask_t control;
    static StackType_t storage[configMINIMAL_STACK_SIZE];
    *task = &control;
    *stack = storage;
    *size = configMINIMAL_STACK_SIZE;
}

int main() {
    static StaticTask_t consumer_control, producer_control;
    static StackType_t consumer_stack[configMINIMAL_STACK_SIZE];
    static StackType_t producer_stack[configMINIMAL_STACK_SIZE];
    consumer_handle = xTaskCreateStatic(consumer, "consumer", configMINIMAL_STACK_SIZE, nullptr, 2,
                                        consumer_stack, &consumer_control);
    CHECK(consumer_handle);
    CHECK(xTaskCreateStatic(producer, "producer", configMINIMAL_STACK_SIZE, nullptr, 1,
                            producer_stack, &producer_control));
    vTaskStartScheduler();
}
