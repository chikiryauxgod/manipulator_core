#!/usr/bin/env python3
"""Run the STM32 ARM firmware and existing serial tests in Renode (Linux)."""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

import serial

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.modbus_client import Client


def wait_ready(process, port, timeout=30):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f'Renode exited with code {process.returncode}')
        if port.exists():
            try:
                # PTYs carry bytes only; they cannot verify physical parity bits.
                with serial.Serial(str(port), 115200, timeout=.5, write_timeout=1) as stream:
                    if Client(stream, 1).status()['version'] == 1:
                        return
            except (OSError, ValueError, TimeoutError, serial.SerialException):
                pass
        time.sleep(.1)
    raise TimeoutError('STM32 firmware did not respond over USART2')


def stop(process):
    if process.poll() is None:
        os.killpg(process.pid, signal.SIGTERM)
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--renode', default='renode')
    parser.add_argument('--elf', type=Path, default=ROOT / 'build/stm32/manipulator_core.elf')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/renode')
    args = parser.parse_args()
    if not args.elf.is_file():
        parser.error(f'Firmware not found: {args.elf}')
    args.output.mkdir(parents=True, exist_ok=True)
    log_path = args.output / 'renode.log'
    try:
        with tempfile.TemporaryDirectory(prefix='blackpill-') as directory:
            work = Path(directory)
            port = work / 'usart2'
            script = work / 'black_pill.resc'
            platform = Path(__file__).with_name('black_pill.repl').resolve()
            script.write_text('\n'.join([
                'mach create "blackpill"',
                f'machine LoadPlatformDescription @{platform}',
                f'sysbus LoadELF @{args.elf.resolve()}',
                f'emulation CreateUartPtyTerminal "uart" {json.dumps(str(port))}',
                'connector Connect sysbus.usart2 uart',
                'start', '',
            ]))
            with log_path.open('w') as log:
                process = subprocess.Popen(
                    [args.renode, '--disable-gui', '--plain', '-P', '-1', str(script)],
                    stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
                try:
                    wait_ready(process, port)
                    print('STM32 firmware ready: Cortex-M4, FreeRTOS, USART2.', flush=True)
                    subprocess.run([
                        sys.executable, str(ROOT / 'tests/integration/hardware/test_black_pill.py'),
                        '--port', str(port), '--no-parity',
                        '--output', str(args.output.resolve() / 'tests.json'),
                    ], check=True, timeout=180)
                    result = subprocess.run([
                        sys.executable, str(ROOT / 'examples/black_pill_session.py'),
                        '--port', str(port), '--no-parity',
                    ], check=True, text=True, stdout=subprocess.PIPE, timeout=60)
                    (args.output / 'example.txt').write_text(result.stdout)
                    print(result.stdout, end='')
                    if process.poll() is not None:
                        raise RuntimeError('Renode stopped during the tests')
                finally:
                    stop(process)
    except (OSError, RuntimeError, TimeoutError, subprocess.SubprocessError) as error:
        print(f'STM32 test failed: {error}\nRenode log: {log_path}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
