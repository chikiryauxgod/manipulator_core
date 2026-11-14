# Manipulator Core

[![CI](https://github.com/chikiryauxgod/manipulator_core/actions/workflows/host.yml/badge.svg)](https://github.com/chikiryauxgod/manipulator_core/actions/workflows/host.yml)

Прошивка для Black Pill на STM32F401CCU6: рассчитывает углы суставов по заданной точке TCP и
принимает команды по Modbus RTU. Приложение написано на C++17, работает под FreeRTOS и поддерживает
настройку модели из 3–6 суставов. Для проверки на ПК есть CMake-сборка, тесты и примеры.

## Оглавление

- [Приложение](app/README.md)
- [Запуск прошивки и диагностика](firmware/README.md)
- [Драйверы и FreeRTOS](platform/README.md)
- [Конфигурация и сборка](config/README.md)
- [Обёртки FreeRTOS](rtos_layer/README.md)
- [Примеры](examples/README.md)
- [Клиент и инструменты](tools/README.md)
- [Тесты](tests/README.md)
- [Протокол и устройство проекта](docs/README.md)
- [Зависимости](third-party/README.md)

[Библиотека кинематики](https://github.com/chikiryauxgod/manipulator_kinematics)
