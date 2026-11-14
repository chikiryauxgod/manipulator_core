# Тесты

`unit/` проверяет прикладную логику, хранение и Python-клиент; `integration/host/` — RTU и работу с FreeRTOS на ПК. В `integration/hardware/` находятся проверки обмена с Black Pill через COM; библиотека кинематики использует Google Test для unit-тестов и pytest для потокового интерфейса.

После [сборки](../config/README.md), из корня проекта:

```sh
ctest --test-dir build/stdio --output-on-failure
ctest --test-dir build/stdio -R kinematics_stdio_pytest --output-on-failure
.venv/bin/python tests/integration/hardware/test_black_pill.py --port /dev/ttyUSB0
```

Аппаратные тесты без выбранного порта пропускаются. Они сбрасывают модель и очереди в RAM; проверка записи Flash включается отдельно через `--flash --programmer /path/to/STM32_Programmer_CLI` и перезаписывает сохранённую конфигурацию. Во время запуска плата должна работать, а COM-порт — быть свободен.
