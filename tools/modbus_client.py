#!/usr/bin/env python3
"""USB-UART client. Requires pyserial. No firmware or platform dependencies."""
import argparse
import json
import math
import os
import struct
import time


def crc16(data):
    value = 0xffff
    for byte in data:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ (0xa001 if value & 1 else 0)
    return value


def float_words(value):
    return list(struct.unpack('>HH', struct.pack('>f', value)))


def words_float(words):
    return struct.unpack('>f', struct.pack('>HH', *words))[0]


class ModbusException(RuntimeError):
    """A valid Modbus exception response, distinct from a transport failure."""

    def __init__(self, code):
        self.code = code
        super().__init__(f'Modbus exception {code}')


class Client:
    def __init__(self, serial, address):
        self.serial = serial
        self.address = address

    def _read(self, count):
        data = self.serial.read(count)
        if len(data) != count:
            raise TimeoutError('Incomplete Modbus response')
        return data

    def transaction(self, function, payload):
        time.sleep(0.005)  # guaranteed RTU inter-frame silence
        self.serial.reset_input_buffer()
        data = bytes([self.address, function]) + payload
        self.serial.write(data + struct.pack('<H', crc16(data)))
        self.serial.flush()
        header = self._read(2)
        if header[0] != self.address:
            raise ValueError('Unexpected slave address')
        if header[1] == function | 0x80:
            response = header + self._read(3)
        elif header[1] != function:
            raise ValueError('Unexpected response function')
        elif function in (3, 4):
            count = self._read(1)
            response = header + count + self._read(count[0] + 2)
        else:
            response = header + self._read(6)
        if crc16(response):
            raise ValueError('Response CRC mismatch')
        if header[1] & 0x80:
            raise ModbusException(response[2])
        return response[2:-2]

    def read(self, address, count, function=4):
        payload = self.transaction(function, struct.pack('>HH', address, count))
        if payload[0] != count * 2:
            raise ValueError('Unexpected register count')
        return list(struct.unpack('>' + 'H' * count, payload[1:]))

    def write(self, address, words):
        for offset in range(0, len(words), 100):
            block = words[offset:offset + 100]
            payload = struct.pack('>HHB', address + offset, len(block), len(block) * 2)
            payload += struct.pack('>' + 'H' * len(block), *block)
            echo = self.transaction(16, payload)
            if echo != struct.pack('>HH', address + offset, len(block)):
                raise ValueError('Incorrect write acknowledgement')

    def command(self, command):
        payload = struct.pack('>HH', 0, command)
        if self.transaction(6, payload) != payload:
            raise ValueError('Incorrect command acknowledgement')

    def configure(self, model):
        joints = model['joints']
        if not 3 <= len(joints) <= 6:
            raise ValueError('Expected 3 to 6 joints')
        words = [0] * 161
        words[0] = len(joints)
        for index, joint in enumerate(joints):
            for field, name in enumerate(('a', 'alpha', 'd_offset', 'theta_offset', 'min', 'max')):
                start = 1 + index * 16 + field * 2
                words[start:start + 2] = float_words(joint[name])
        identity = [float(row == col) for row in range(4) for col in range(4)]
        for start, name in ((97, 'base'), (129, 'tool')):
            matrix = model.get(name, identity)
            if len(matrix) != 16:
                raise ValueError('Transform must contain 16 row-major values')
            for index, value in enumerate(matrix):
                words[start + index * 2:start + index * 2 + 2] = float_words(value)
        self.command(1)
        self.write(0x100, words)
        self.command(2)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            status = self.read(0, 7)
            if status[1] == 2:
                return
            if status[1] == 0:
                raise RuntimeError(f'Model rejected: k_status={status[5]}')
            time.sleep(0.02)
        raise TimeoutError('Model activation timed out')

    @staticmethod
    def _request_id(request_id):
        if not isinstance(request_id, int) or not 0 <= request_id <= 0xffffffff:
            raise ValueError('Request ID must fit uint32')
        return [request_id >> 16, request_id & 0xffff]

    def status(self):
        data = self.read(0, 7)
        return {'version': data[0], 'mode': data[1], 'busy': bool(data[2]),
                'queued': data[3], 'results': data[4],
                'configuration_status': data[5], 'has_result': bool(data[6])}

    def submit(self, request_id, target, initial):
        """Snapshot inputs and enqueue IK. Does not wait or resend on timeout."""
        target, initial = list(target), list(initial)
        if len(target) != 3 or len(initial) > 6:
            raise ValueError('Expected three target coordinates and at most six initial angles')
        if not all(math.isfinite(value) for value in target + initial):
            raise ValueError('Target and initial angles must be finite')
        words = self._request_id(request_id)
        for value in target + initial + [0.] * (6 - len(initial)):
            words += float_words(value)
        self.write(0x200, words)
        self.command(3)

    def get_result(self):
        """Read the FIFO head without removing it. None means no result yet."""
        data = self.read(0, 24)
        if not data[6]:
            return None
        return {'id': (data[7] << 16) | data[8], 'status': data[9],
                'angles': [words_float(data[10 + 2*i:12 + 2*i]) for i in range(6)],
                'residual': words_float(data[22:24])}

    def wait_result(self, request_id, timeout=30):
        self._request_id(request_id)
        if timeout <= 0:
            raise ValueError('Result timeout must be positive')
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            result = self.get_result()
            if result is not None:
                if result['id'] != request_id:
                    raise RuntimeError(f"Unread older result {result['id']}; ACK it first")
                return result
            time.sleep(0.02)
        raise TimeoutError('IK timed out; result may still arrive, do not resend blindly')

    def acknowledge(self, request_id):
        """Remove only the FIFO head with the matching ID."""
        self.write(0x200, self._request_id(request_id))
        self.command(4)

    def solve(self, request_id, target, initial):
        """Convenience operation: submit, wait, acknowledge (including IK errors)."""
        self.submit(request_id, target, initial)
        result = self.wait_result(request_id)
        self.acknowledge(request_id)
        return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--baud', type=int, default=115200)
    parser.add_argument('--address', type=int, default=1)
    parser.add_argument('--no-parity', action='store_true', help='8N2 instead of 8E1')
    commands = parser.add_subparsers(dest='command', required=True)
    commands.add_parser('status')
    init = commands.add_parser('init')
    init.add_argument('model', help='JSON model file')
    ik = commands.add_parser('ik')
    ik.add_argument('target', type=float, nargs=3)
    ik.add_argument('--initial', type=float, nargs='+', required=True)
    ik.add_argument('--id', type=int, default=1)
    ack = commands.add_parser('ack')
    ack.add_argument('id', type=int)
    commands.add_parser('save')
    commands.add_parser('load')
    commands.add_parser('apply')
    args = parser.parse_args()
    import serial
    with serial.Serial(args.port, args.baud, timeout=5, write_timeout=5,
                       exclusive=True if os.name == 'posix' else None,
                       parity='N' if args.no_parity else 'E',
                       stopbits=2 if args.no_parity else 1) as port:
        client = Client(port, args.address)
        if args.command == 'status':
            print(client.read(0, 24))
        elif args.command == 'init':
            with open(args.model, encoding='utf-8') as file:
                client.configure(json.load(file))
            print('Model active')
        elif args.command == 'ik':
            print(json.dumps(client.solve(args.id, args.target, args.initial)))
        elif args.command == 'ack':
            client.write(0x200, [args.id >> 16, args.id & 0xffff])
            client.command(4)
        else:
            client.command({'save': 5, 'load': 6, 'apply': 2}[args.command])


if __name__ == '__main__':
    main()
