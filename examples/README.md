# Примеры

`black_pill_session.py` настраивает 3R-модель через Modbus, отправляет цель TCP, получает углы и подтверждает чтение результата. `kinematics_stdio.stdin` содержит запросы FK и IK для host-бинарника, работающего без платы.

Из корня проекта после [сборки](../config/README.md):

```sh
.venv/bin/python examples/black_pill_session.py --port /dev/ttyUSB0
./build/stdio/kinematics_stdio_example < examples/kinematics_stdio.stdin
```

Бинарник принимает по одному запросу на строку и сразу пишет ответ в stdout; завершение — EOF. Он использует потоковый драйвер библиотеки напрямую, без прикладного слоя и Modbus.

```text
FK n [R/P a alpha d_offset theta_offset q]×n [base]×16 [tool]×16
IK n id [R/P a alpha d_offset theta_offset initial B/U min max joint_scale max_step]×n [base]×16 [tool]×16 x y z tolerance length_scale damping iterations backtracking_trials
```

Матрицы передаются построчно, длины — в метрах, углы — в радианах; `B` — сустав с ограничениями, `U` — без них. Ответ FK — `OK` и матрица; IK — `OK IK id n q1 … qn residual iteration_attempts fk_evaluations`. Ошибки возвращаются как `ERR status` или `ERR IK id status`, после них можно продолжать обмен. Драйвер допускает 1–64 сустава, прошивка — 3–6.
