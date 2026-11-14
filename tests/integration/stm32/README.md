# STM32 в Renode

Запускается ARM-прошивка с FreeRTOS, HAL, драйверами USART2 и таймеров.
Профиль задаёт 64 КБ RAM, 256 КБ Flash и частоту 84 МГц на моделях STM32F4 из Renode 1.17.0.
UART выведен в PTY; через него запускаются существующие 8 тестов обмена и пример расчёта.

Из корня проекта, с Arm GNU Toolchain в `PATH` и установленным Renode:

```sh
cmake -S firmware -B build/stm32 -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/arm-none-eabi.cmake" -DCMAKE_BUILD_TYPE=Release
cmake --build build/stm32 --parallel
.venv/bin/python tests/integration/stm32/run.py --renode /path/to/renode
```

Результаты и лог сохраняются в `build/renode/`. В Actions запуск находится
в задании `STM32 firmware in Renode`, результат примера доступен в Summary.
PTY не проверяет биты чётности; Flash со сбросом через ST-Link в этот запуск не входит.
Модели периферии не воспроизводят все особенности STM32F401.
