#!/usr/bin/env python3
"""Test a board running FIRMWARE_UART_ECHO=1 on USART2 at 115200 8N1."""
import argparse
import sys
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--count", type=int, default=5)
    parser.add_argument("--timeout", type=float, default=2.0)
    args = parser.parse_args()
    if args.count < 1 or args.timeout <= 0:
        parser.error("count and timeout must be positive")

    try:
        with serial.Serial(
            args.port,
            115200,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=args.timeout,
            write_timeout=args.timeout,
            xonxoff=False,
            rtscts=False,
            dsrdtr=False,
            exclusive=True,
        ) as port:
            port.reset_input_buffer()
            for index in range(args.count):
                sent = f"BOARD UART ECHO {index + 1:04d}\r\n".encode("ascii")
                written = port.write(sent)
                port.flush()
                received = port.read(len(sent))
                ok = written == len(sent) and received == sent
                print(
                    f"{index + 1}: TX={sent!r} RX={received!r} " f'{"OK" if ok else "FAIL"}',
                    flush=True,
                )
                if not ok:
                    print("Check board echo mode, Resume/F8 and PA2/PA3 wiring.", file=sys.stderr)
                    return 1
                time.sleep(0.2)
    except (serial.SerialException, OSError) as error:
        print(f"Cannot communicate on {args.port}: {error}", file=sys.stderr)
        return 1

    print("Board UART echo passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
