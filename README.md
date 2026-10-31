# Manipulator Core

Вычислительный контроллер кинематики: STM32F401CCU6, C++17, FreeRTOS,
Modbus RTU slave. Управление приводами не входит в проект.

## Получение зависимостей

```
git clone --recurse-submodules <URL-этого-репозитория>
# либо после обычного clone:
git submodule update --init --recursive
```

Точные версии зависимостей зафиксированы gitlinks, а не веткой master.
Основной репозиторий находится в этом каталоге рядом с `.project`/`.ioc`.
Remote основного репозитория пока не задан; коммит локальный.

## Проверка на Linux

```
cmake -S . -B build -DRTOS_LAYER_BUILD_TESTS=ON
cmake --build build -j 2
ctest --test-dir build --output-on-failure
```

По умолчанию включён интеграционный тест POSIX-порта FreeRTOS.
Для сборки без Linux-порта передайте `-DAPPLICATION_FREERTOS_PATH=`.
Тесты не требуют платы. CubeIDE импортируется из этого каталога; сгенерированная
прошивка пока не подключает новые модули приложения и UART.

- [Протокол](docs/modbus.md)
- [Разработка](docs/development.md)
- [Зависимости](docs/dependencies.md)
- [rtos_layer](rtos_layer/README.md)
