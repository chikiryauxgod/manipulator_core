#!/usr/bin/env python3
"""One real UART session: configure model, submit IK, read result, ACK.

Requires normal firmware (FIRMWARE_UART_ECHO=0). Replaces the RAM model/queues.
Does not write Flash, reflash the board or change its UART settings.
"""
import argparse
import json
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.modbus_client import Client


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True, help="/dev/ttyUSB0, /dev/serial/by-id/... or COM3")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--address", type=int, default=1)
    parser.add_argument("--no-parity", action="store_true", help="8N2 instead of 8E1")
    parser.add_argument("--model", type=Path, default=ROOT / "tools/model_3r.json")
    parser.add_argument(
        "--target", type=float, nargs=3, default=[0.3, 0.0, 0.3], metavar=("X", "Y", "Z")
    )
    parser.add_argument("--initial", type=float, nargs="+", default=[0.1, 0.2, 0.3])
    parser.add_argument("--id", type=int, default=1)
    args = parser.parse_args()
    if not 1 <= args.address <= 247 or not 0 <= args.id <= 0xFFFFFFFF:
        parser.error("Slave address must be 1..247; request ID must fit uint32")

    try:
        import serial

        with args.model.open(encoding="utf-8") as file:
            model = json.load(file)
        if len(args.initial) != len(model["joints"]):
            raise ValueError("Provide one initial angle per joint")
        with serial.Serial(
            args.port,
            args.baud,
            bytesize=8,
            parity="N" if args.no_parity else "E",
            stopbits=2 if args.no_parity else 1,
            timeout=5,
            write_timeout=5,
            exclusive=True if os.name == "posix" else None,
        ) as port:
            client = Client(port, args.address)
            print("1. Connected:", json.dumps(client.status()), flush=True)

            # BEGIN, DH/limits/base/tool register writes, APPLY and readiness polling.
            client.configure(model)
            print(f"2. Model active: {len(model['joints'])} joints", flush=True)

            # Metres and radians. SUBMIT copies the initial angles and TCP target.
            client.submit(args.id, args.target, args.initial)
            print(f"3. Queued IK #{args.id}: TCP={args.target} m", flush=True)

            # Waiting/reading does not remove the result from the board's FIFO.
            result = client.wait_result(args.id)
            print("4. Result:", json.dumps(result), flush=True)
            print("5. Before ACK:", json.dumps(client.status()), flush=True)

            # Only ACK releases the FIFO slot. No blind retry on transport timeout.
            client.acknowledge(args.id)
            print("6. After ACK:", json.dumps(client.status()), flush=True)
            if result["status"] != 0:
                print("IK failed; angles must not be used.", file=sys.stderr)
                return 1
    except ImportError:
        print("Install pyserial: python3 -m pip install -r tools/requirements.txt", file=sys.stderr)
        return 1
    except (OSError, ValueError, KeyError, RuntimeError, serial.SerialException) as error:
        print(f"Session failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
