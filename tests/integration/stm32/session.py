"""Queue complete host writes while Renode is paused, then run UART timing."""
import argparse
import runpy
import socket
import sys
from unittest.mock import patch

import serial


class EmulatorSerial(serial.Serial):
    monitor_port = None
    prompt = b'(blackpill) '

    def __init__(self, *args, **kwargs):
        self.monitor = None
        super().__init__(*args, **kwargs)
        self.monitor = socket.create_connection(('127.0.0.1', self.monitor_port), timeout=5)
        self.monitor.settimeout(5)
        self._prompts(1)

    def _prompts(self, count):
        data = bytearray()
        while data.count(self.prompt) < count:
            block = self.monitor.recv(65536)
            if not block:
                raise ConnectionError('Renode monitor disconnected')
            data.extend(block)
        if b'There was an error' in data:
            raise RuntimeError(data.decode(errors='replace'))

    def write(self, packet):
        # PTY reader scheduling must not turn one host write into multiple RTU
        # frames. Queue all bytes at one virtual instant; STM32_UART then clocks
        # them into the peripheral, raising the firmware's actual RX interrupts.
        commands = ['pause']
        commands.extend(f'sysbus.usart2 WriteChar {byte}' for byte in packet)
        commands.append('start')
        self.monitor.sendall(('\n'.join(commands) + '\n').encode())
        self._prompts(len(commands))
        return len(packet)

    def close(self):
        if self.monitor is not None:
            self.monitor.close()
            self.monitor = None
        super().close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--monitor', type=int, required=True)
    parser.add_argument('script')
    parser.add_argument('arguments', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    EmulatorSerial.monitor_port = args.monitor
    sys.argv = [args.script, *args.arguments]
    # Only the emulator's byte injection differs. The existing test/client code
    # and PTY receive path are unchanged; every response comes from ARM firmware.
    with patch('serial.Serial', EmulatorSerial):
        runpy.run_path(args.script, run_name='__main__')


if __name__ == '__main__':
    main()
