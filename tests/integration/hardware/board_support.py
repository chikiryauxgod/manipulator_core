"""Independent DH reference and bounded polling for physical-board tests."""

import json
import math
from pathlib import Path
import time

ROOT = Path(__file__).resolve().parents[3]


def model_for(count=3):
    model = json.loads((ROOT / "tools/model_3r.json").read_text())
    while len(model["joints"]) < count:
        model["joints"].append(
            {
                "a": 0.05,
                "alpha": math.pi / 2,
                "d_offset": 0,
                "theta_offset": 0,
                "min": -math.pi,
                "max": math.pi,
            }
        )
    if count == 6:
        model["base"] = [1, 0, 0, 0.1, 0, 1, 0, -0.05, 0, 0, 1, 0.02, 0, 0, 0, 1]
        model["tool"] = [1, 0, 0, 0.03, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    return model


def forward_position(model, angles):
    def matrix(values):
        return [values[i : i + 4] for i in range(0, 16, 4)]

    def multiply(a, b):
        return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]

    unit = [float(i == j) for i in range(4) for j in range(4)]
    pose = matrix(model.get("base", unit))
    for joint, angle in zip(model["joints"], angles):
        theta = angle + joint["theta_offset"]
        ct, st = math.cos(theta), math.sin(theta)
        ca, sa = math.cos(joint["alpha"]), math.sin(joint["alpha"])
        a, d = joint["a"], joint["d_offset"]
        pose = multiply(
            pose,
            [
                [ct, -st * ca, st * sa, a * ct],
                [st, ct * ca, -ct * sa, a * st],
                [0, sa, ca, d],
                [0, 0, 0, 1],
            ],
        )
    pose = multiply(pose, matrix(model.get("tool", unit)))
    return [pose[i][3] for i in range(3)]


def wait_status(client, predicate, timeout=10):
    deadline = time.monotonic() + timeout
    last = None
    while time.monotonic() < deadline:
        last = client.status()
        if predicate(last):
            return last
        time.sleep(0.01)
    raise AssertionError(f"Board did not reach expected state: {last}")
