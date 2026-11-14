cmake_minimum_required(VERSION 3.20)
project(manipulator_firmware LANGUAGES C CXX ASM)
get_filename_component(root "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(hal "${root}/Drivers/STM32F4xx_HAL_Driver")
set(kernel "${root}/third-party/FreeRTOS-Kernel")
set(kinematics "${root}/third-party/manipulator_kinematics")
set(K_REAL_TYPE
    float
    CACHE STRING "Arithmetic profile" FORCE)
add_subdirectory("${kinematics}" "${CMAKE_CURRENT_BINARY_DIR}/kinematics")

add_executable(
    manipulator_core
    ${root}/Core/Src/main.c
    ${root}/Core/Src/stm32f4xx_hal_msp.c
    ${root}/Core/Src/stm32f4xx_it.c
    ${root}/Core/Src/syscalls.c
    ${root}/Core/Src/sysmem.c
    ${root}/Core/Src/system_stm32f4xx.c
    ${root}/Core/Startup/startup_stm32f401ccux.s
    ${root}/app/kinematics/solver.cpp
    ${root}/app/protocol/modbus/registers.cpp
    ${root}/app/protocol/modbus/server.cpp
    ${root}/app/storage/persistent_store.cpp
    firmware.cpp
    uart_echo.cpp
    ${root}/platform/freertos/rtu_transport.cpp
    ${root}/platform/stm32/drivers/board_io.cpp
    ${root}/platform/stm32/drivers/flash.cpp
    ${root}/platform/stm32/drivers/hal_timebase.c
    ${root}/platform/stm32/drivers/timer2.cpp
    ${root}/platform/stm32/drivers/usart2.cpp
    ${root}/third-party/nanoMODBUS/nanomodbus.c
    ${kernel}/list.c
    ${kernel}/queue.c
    ${kernel}/tasks.c
    ${kernel}/portable/GCC/ARM_CM4F/port.c)
foreach(
    module IN
    ITEMS hal
          hal_cortex
          hal_dma
          hal_dma_ex
          hal_exti
          hal_flash
          hal_flash_ex
          hal_flash_ramfunc
          hal_gpio
          hal_pwr
          hal_pwr_ex
          hal_rcc
          hal_rcc_ex)
    target_sources(manipulator_core PRIVATE ${hal}/Src/stm32f4xx_${module}.c)
endforeach()
set_target_properties(
    manipulator_core
    PROPERTIES SUFFIX .elf
               C_STANDARD 17
               CXX_STANDARD 17)
target_include_directories(
    manipulator_core
    PRIVATE ${root}/Core/Inc
            ${hal}/Inc
            ${hal}/Inc/Legacy
            ${root}/Drivers/CMSIS/Device/ST/STM32F4xx/Include
            ${root}/Drivers/CMSIS/Include
            ${root}/app
            ${root}/config
            ${root}/firmware
            ${root}/platform
            ${root}/rtos_layer/include
            ${root}/config/freertos
            ${kernel}/include
            ${kernel}/portable/GCC/ARM_CM4F
            ${root}/third-party/nanoMODBUS)
target_compile_definitions(
    manipulator_core PRIVATE USE_HAL_DRIVER STM32F401xC NMBS_CLIENT_DISABLED
                             NMBS_SERVER_READ_WRITE_REGISTERS_DISABLED FIRMWARE_UART_ECHO=0)
target_compile_options(
    manipulator_core
    PRIVATE
        -Wall
        -ffunction-sections
        -fdata-sections
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti;-fno-use-cxa-atexit;-fno-threadsafe-statics>
)
target_link_libraries(manipulator_core PRIVATE manipulator::kinematics)
target_link_options(
    manipulator_core
    PRIVATE
    -T${root}/STM32F401CCUX_FLASH.ld
    --specs=nano.specs
    --specs=nosys.specs
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/manipulator_core.map)
set_property(
    TARGET manipulator_core
    APPEND
    PROPERTY LINK_DEPENDS ${root}/STM32F401CCUX_FLASH.ld)
