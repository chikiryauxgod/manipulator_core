# Зависимости

| Каталог | Источник | Фиксация |
|---|---|---|
| third-party/nanoMODBUS | https://github.com/debevv/nanoMODBUS | Git submodule |
| third-party/FreeRTOS-Kernel | https://github.com/FreeRTOS/FreeRTOS-Kernel | Git submodule, V11.1.0 |
| third-party/manipulator_kinematics | https://github.com/chikiryauxgod/manipulator_kinematics | Git submodule |
| rtos_layer | Собственный код этого проекта | Обычные исходники, отдельный CMake target |
| Drivers/STM32F4xx_HAL_Driver | STM32Cube FW_F4 V1.28.3, HAL 1.8.5 | Сгенерированная CubeMX копия |
| Drivers/CMSIS | STM32Cube FW_F4 V1.28.3 | Сгенерированная CubeMX копия |

HAL/CMSIS пока НЕ сабмодули: исходная генерация содержит отдельные выбранные файлы,
а не Git checkout; загрузка upstream недоступна из среды разработки по DNS.
Они сохранены без подмены версий. При доступе к upstream нужно выбрать совместимые
коммиты HAL, CMSIS Core и CMSIS Device F4, сравнить с копиями, сохранить пути
CubeIDE и повторно проверить прошивку. Не создавать фиктивные gitlinks по тегам.
Лицензии/заголовки исходных файлов сохранены; новые лицензии для своего кода не назначены.

Кинематика подключена с последнего локального коммита, без незакоммиченных
изменений исходного соседнего репозитория. Сам соседний репозиторий не изменён.
Скачанная отдельно копия FreeRTOS в родительском каталоге также сохранена.

В обновлениях сабмодулей фиксируйте новый SHA только после тестов.
CubeMX при повторной генерации не должен перезаписывать содержимое third-party.
