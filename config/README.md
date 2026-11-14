# Конфигурация и сборка

Здесь лежат настройки FreeRTOS, численного профиля кинематики и режима прошивки. Проект STM32CubeIDE открывается из корня репозитория, где находятся `.project`, `.cproject` и `manipulator_core.ioc`; CMake используется для сборки на ПК и [ARM-прошивки](../tests/integration/stm32/README.md).

```sh
git submodule update --init --recursive
python3 -m venv .venv
.venv/bin/python -m pip install -r tests/requirements.txt
cmake -S . -B build/stdio -DPython3_EXECUTABLE="$PWD/.venv/bin/python" -DCMAKE_BUILD_TYPE=Debug
cmake --build build/stdio --parallel
```

Команды выполняются из корня проекта. Подключение платы описано в [platform](../platform/README.md), запуск проверок — в [tests](../tests/README.md).
