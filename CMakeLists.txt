cmake_minimum_required(VERSION 3.20)
project(manipulator_core_host LANGUAGES C CXX)
# Host validation build; CubeIDE remains responsible for firmware generation.
set(KINEMATICS_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/third-party/manipulator_kinematics"
    CACHE PATH "Kinematics sources")
set(K_REAL_TYPE
    float
    CACHE STRING "Arithmetic profile" FORCE)
add_subdirectory(${KINEMATICS_PATH} ${CMAKE_CURRENT_BINARY_DIR}/kinematics)
add_library(manipulator_application app/kinematics/solver.cpp app/protocol/modbus/registers.cpp
                                    app/storage/persistent_store.cpp)
target_compile_features(manipulator_application PUBLIC cxx_std_17)
target_include_directories(manipulator_application PUBLIC app)
target_link_libraries(manipulator_application PUBLIC manipulator::kinematics)
target_compile_options(manipulator_application PRIVATE -Wall -Wextra -Werror -fno-exceptions
                                                       -fno-rtti)

# Reuse the library's tested line protocol rather than maintain a second parser. This host example
# calls the C API directly; it does not require a board or RTOS.
add_executable(kinematics_stdio_example ${KINEMATICS_PATH}/tests/driver/forward_driver.c)
set_target_properties(
    kinematics_stdio_example
    PROPERTIES C_STANDARD 11
               C_STANDARD_REQUIRED YES
               C_EXTENSIONS NO)
target_link_libraries(kinematics_stdio_example PRIVATE manipulator::kinematics)
target_compile_options(kinematics_stdio_example PRIVATE -Wall -Wextra -Werror)

enable_testing()
add_executable(application_tests tests/unit/application.cpp)
target_link_libraries(application_tests PRIVATE manipulator_application)
add_test(NAME application COMMAND application_tests)

add_executable(protocol_tests tests/unit/protocol.cpp)
target_link_libraries(protocol_tests PRIVATE manipulator_application)
add_test(NAME protocol COMMAND protocol_tests)

# Explicit optional targets: no downloads and no UART assumptions.
set(NANOMODBUS_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/third-party/nanoMODBUS"
    CACHE PATH "nanoMODBUS sources")
if(EXISTS "${NANOMODBUS_PATH}/nanomodbus.c")
    add_library(manipulator_modbus STATIC ${NANOMODBUS_PATH}/nanomodbus.c
                                          app/protocol/modbus/server.cpp)
    target_include_directories(manipulator_modbus PUBLIC ${NANOMODBUS_PATH})
    target_link_libraries(manipulator_modbus PUBLIC manipulator_application)
    target_compile_definitions(manipulator_modbus PUBLIC NMBS_CLIENT_DISABLED
                                                         NMBS_SERVER_READ_WRITE_REGISTERS_DISABLED)
    add_executable(rtu_tests tests/integration/host/rtu.cpp)
    target_link_libraries(rtu_tests PRIVATE manipulator_modbus)
    add_test(NAME modbus_rtu COMMAND rtu_tests)
endif()
set(APPLICATION_FREERTOS_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/third-party/FreeRTOS-Kernel"
    CACHE PATH "FreeRTOS source for application Linux integration")
if(APPLICATION_FREERTOS_PATH)
    set(RTOS_LAYER_FREERTOS_PATH
        "${APPLICATION_FREERTOS_PATH}"
        CACHE PATH "" FORCE)
    add_subdirectory(rtos_layer ${CMAKE_CURRENT_BINARY_DIR}/rtos_layer)
    add_executable(application_rtos_tests tests/integration/host/rtos.cpp)
    target_include_directories(application_rtos_tests PRIVATE platform)
    target_link_libraries(
        application_rtos_tests PRIVATE manipulator_application rtos_layer::rtos_layer
                                       rtos_layer_test_kernel)
    target_compile_options(application_rtos_tests PRIVATE -Wall -Wextra -Werror -fno-exceptions
                                                          -fno-rtti)
    add_test(NAME application_rtos COMMAND application_rtos_tests)
    set_tests_properties(application_rtos PROPERTIES TIMEOUT 10)
endif()

add_executable(transport_storage_tests tests/unit/transport_storage.cpp)
target_link_libraries(transport_storage_tests PRIVATE manipulator_application)
add_test(NAME transport_storage COMMAND transport_storage_tests)

add_executable(storage_protocol_tests tests/unit/storage_protocol.cpp)
target_link_libraries(storage_protocol_tests PRIVATE manipulator_application)
add_test(NAME storage_protocol COMMAND storage_protocol_tests)

if(TARGET manipulator_modbus)
    add_executable(session_tests tests/integration/host/session.cpp)
    target_link_libraries(session_tests PRIVATE manipulator_modbus)
    add_test(NAME modbus_session COMMAND session_tests)
endif()

add_executable(persistent_store_tests tests/unit/persistent_store.cpp)
target_link_libraries(persistent_store_tests PRIVATE manipulator_application)
target_compile_options(persistent_store_tests PRIVATE -Wall -Wextra -Werror)
add_test(NAME persistent_store COMMAND persistent_store_tests)

# Host client tests need only Python's standard library. Hardware access is explicit; an unselected
# board is a CTest SKIP, not a successful hardware run.
find_package(
    Python3
    COMPONENTS Interpreter
    QUIET)
if(Python3_Interpreter_FOUND)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} -c "import pytest"
        RESULT_VARIABLE pytest_unavailable
        OUTPUT_QUIET ERROR_QUIET)
    if(pytest_unavailable EQUAL 0)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET manipulator_modbus)
            add_executable(virtual_modbus_server tests/integration/virtual/server.cpp)
            target_link_libraries(virtual_modbus_server PRIVATE manipulator_modbus)
            target_compile_options(virtual_modbus_server PRIVATE -Wall -Wextra -Werror)
            add_test(
                NAME virtual_serial_pytest
                COMMAND
                    ${CMAKE_COMMAND} -E env PTY_SERVER=$<TARGET_FILE:virtual_modbus_server>
                    ${Python3_EXECUTABLE} -m pytest
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/virtual -q)
            set_tests_properties(virtual_serial_pytest PROPERTIES TIMEOUT 60)
        endif()

        add_test(
            NAME kinematics_stdio_pytest
            COMMAND
                ${Python3_EXECUTABLE} -m pytest
                ${KINEMATICS_PATH}/tests/integration/test_forward_stream.py
                ${KINEMATICS_PATH}/tests/integration/test_inverse_stream.py
                --driver-path=$<TARGET_FILE:kinematics_stdio_example> --profile=float -q)
        set_tests_properties(kinematics_stdio_pytest PROPERTIES TIMEOUT 120)
    else()
        message(
            STATUS
                "stdio pytest tests disabled: install tests/requirements.txt and select Python3_EXECUTABLE"
        )
    endif()

    add_test(NAME modbus_client_python
             COMMAND ${Python3_EXECUTABLE} -m unittest discover -s
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/unit -p test_*.py -v)

    set(BLACKPILL_PORT
        ""
        CACHE STRING "Explicit serial port for physical Black Pill tests")
    set(board_arguments)
    if(BLACKPILL_PORT)
        list(APPEND board_arguments --port "${BLACKPILL_PORT}")
    endif()
    add_test(
        NAME black_pill_hardware
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/hardware/test_black_pill.py
            ${board_arguments})
    set_tests_properties(
        black_pill_hardware
        PROPERTIES LABELS
                   hardware
                   RUN_SERIAL
                   TRUE
                   SKIP_RETURN_CODE
                   77
                   TIMEOUT
                   300)
endif()
