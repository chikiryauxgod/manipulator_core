#include "firmware.h"
#include "firmware_mode.h"
#include "uart_echo.hpp"

#include "freertos/service.hpp"
#include "protocol/modbus/server.hpp"
#include "freertos/rtu_transport.hpp"
#include "stm32/drivers/flash.hpp"
#include "stm32/drivers/board_io.hpp"
#include "storage/persistent_store.hpp"
#include "diagnostics.hpp"

#include <new>

namespace {

constexpr uint32_t worker_stack_words = 2048;
constexpr uint32_t protocol_stack_words = 2048;

StaticTask_t worker_control;
StaticTask_t protocol_control;
StackType_t worker_stack[worker_stack_words];
StackType_t protocol_stack[protocol_stack_words];
TaskHandle_t worker_handle = nullptr;

// Construct after HAL_Init(), without heap or global RTOS constructors.
alignas(manipulator::RtosService) unsigned char service_storage[sizeof(manipulator::RtosService)];

void protocol_entry(void* argument) {
    auto& service = *static_cast<manipulator::RtosService*>(argument);
    // Static storage keeps frame queues and the flash codec off the task stack.
    static manipulator::stm32::drivers::ConfigurationFlash flash;
    static manipulator::PersistentConfigurationStore store(flash);
    static manipulator::FreeRtosRtuTransport uart;
    static manipulator::modbus::Registers registers(service, &store);
    static manipulator::modbus::Server server(registers);

    firmware_diagnostics.configuration_loaded = registers.restore();
    const auto settings = registers.settings();
    if (!uart.initialize(settings.baud, settings.even_parity) ||
        server.initialize(settings.slave, uart) != NMBS_ERROR_NONE) {
        firmware_assert_failed(__FILE__, __LINE__);
    }
    server.timeouts(0, 0); // reads consume one complete, hardware-framed ADU

    for (;;) {
        uart.acquire_frame();
        const auto error = server.poll();
        if (error != NMBS_ERROR_NONE) {
            ++firmware_diagnostics.modbus_errors;
        }
        firmware_diagnostics.last_modbus_error = error;
        const auto diagnostics = uart.diagnostics();
        publish_uart_statistics(diagnostics.uart);
        firmware_diagnostics.rtu_timer_count = diagnostics.timer_interrupts;
        firmware_diagnostics.rtu_frames = diagnostics.frames;
        firmware_diagnostics.rtu_last_size = diagnostics.last_size;
        firmware_diagnostics.rtu_crc_errors = diagnostics.crc_errors;
        firmware_diagnostics.dropped_frames = diagnostics.dropped_frames;
        ++firmware_diagnostics.protocol_heartbeat;
        firmware_diagnostics.scheduler_tick = xTaskGetTickCount();
        firmware_diagnostics.hal_tick = manipulator::stm32::drivers::milliseconds();
        firmware_diagnostics.worker_stack_free_min = uxTaskGetStackHighWaterMark(worker_handle);
        firmware_diagnostics.protocol_stack_free_min = uxTaskGetStackHighWaterMark(nullptr);
    }
}

} // namespace

extern "C" {

volatile FirmwareDiagnostics firmware_diagnostics = {};

void firmware_assert_failed(const char* file, uint32_t line) {
    manipulator::stm32::drivers::disable_interrupts();
    firmware_diagnostics.fault_file = file;
    firmware_diagnostics.fault_line = line;

    for (;;) {
        manipulator::stm32::drivers::idle_instruction();
    }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char* task_name) {
    (void)task;
    firmware_assert_failed(task_name, 0);
}

void vApplicationGetIdleTaskMemory(StaticTask_t** task, StackType_t** stack,
                                   configSTACK_DEPTH_TYPE* depth) {
    static StaticTask_t idle_control;
    static StackType_t idle_stack[configMINIMAL_STACK_SIZE];

    *task = &idle_control;
    *stack = idle_stack;
    *depth = configMINIMAL_STACK_SIZE;
}

void firmware_start(void) {
    if (FIRMWARE_UART_ECHO) {
        uart_echo_run();
    }

    auto* service = new (service_storage) manipulator::RtosService;

    worker_handle =
        xTaskCreateStatic(manipulator::RtosService::worker_entry, "kinematics", worker_stack_words,
                          service, 1, worker_stack, &worker_control);
    configASSERT(worker_handle != nullptr);
    service->bind_worker(worker_handle);

    const auto protocol_handle = xTaskCreateStatic(protocol_entry, "protocol", protocol_stack_words,
                                                   service, 2, protocol_stack, &protocol_control);
    configASSERT(protocol_handle != nullptr);

    vTaskStartScheduler();
    firmware_assert_failed(__FILE__, __LINE__);
}
}
