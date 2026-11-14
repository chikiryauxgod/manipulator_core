# Инструменты

`modbus_client.py` — клиент платы: чтение состояния, настройка модели и отправка расчётов. `uart_echo_test.py` проверяет эхо UART, `generate_kinematics_config.py` создаёт заголовок численного профиля, `model_3r.json` содержит пример модели.

```sh
.venv/bin/python tools/modbus_client.py --port /dev/ttyUSB0 status
.venv/bin/python tools/modbus_client.py --help
```

Команды выполняются из корня проекта. Для эхо-теста нужна прошивка с `FIRMWARE_UART_ECHO=1`; клиент Modbus работает со штатным режимом `0`.
