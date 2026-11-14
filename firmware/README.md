# Прошивка

`firmware.cpp` связывает приложение, транспорт и задачи FreeRTOS. В `diagnostics.hpp` находятся счётчики для отладчика, в `uart_echo.cpp` — режим проверки UART без Modbus.

Выбор режима задаётся в [firmware_mode.h](../config/firmware_mode.h): `FIRMWARE_UART_ECHO=0` — штатная работа, `1` — эхо UART.
