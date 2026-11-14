"""Client protocol tests only: these do not claim to test a physical board."""

import math
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.modbus_client import Client, ModbusException, crc16, float_words


def response(function, payload, address=1):
    body = bytes([address, function]) + payload
    return body + struct.pack("<H", crc16(body))


def result_payload(request_id=0x12345678, present=True):
    words = [0] * 24
    words[0], words[1], words[6] = 1, 2, int(present)
    words[7:9] = [request_id >> 16, request_id & 65535]
    for index, angle in enumerate([0.1, 0.2, 0.3, 0, 0, 0]):
        words[10 + index * 2 : 12 + index * 2] = float_words(angle)
    words[22:24] = float_words(1e-6)
    return bytes([48]) + struct.pack(">24H", *words)


class ScriptedPort:
    def __init__(self, responses):
        self.responses = list(responses)
        self.received = b""
        self.writes = []

    def reset_input_buffer(self):
        self.received = b""

    def write(self, packet):
        if crc16(packet) != 0:
            raise AssertionError("Invalid request CRC")
        self.writes.append(packet)
        self.received = self.responses.pop(0)
        return len(packet)

    def flush(self):
        pass

    def read(self, count):
        result = self.received[:count]
        self.received = self.received[count:]
        return result


class ClientTests(unittest.TestCase):
    def setUp(self):
        self.delay = patch("tools.modbus_client.time.sleep")
        self.delay.start()
        self.addCleanup(self.delay.stop)

    def test_known_crc_and_read_frame(self):
        port = ScriptedPort([response(4, b"\x02\x00\x01")])
        self.assertEqual(Client(port, 1).read(0, 1), [1])
        self.assertEqual(port.writes[0], bytes.fromhex("01 04 00 00 00 01 31 ca"))
        self.assertEqual(crc16(bytes.fromhex("01 03 00 00 00 0a")), 0xCDC5)

    def test_complete_session_preserves_explicit_ack(self):
        port = ScriptedPort(
            [
                response(16, struct.pack(">HH", 0x200, 20)),
                response(6, struct.pack(">HH", 0, 3)),
                response(4, result_payload(present=False)),
                response(4, result_payload()),
                response(4, result_payload()),
                response(16, struct.pack(">HH", 0x200, 2)),
                response(6, struct.pack(">HH", 0, 4)),
            ]
        )
        client = Client(port, 1)
        client.submit(0x12345678, [0.3, 0, 0.3], [0.1, 0.2, 0.3])
        result = client.wait_result(0x12345678)
        self.assertEqual(result["id"], 0x12345678)
        self.assertEqual(client.get_result(), result)
        self.assertFalse(
            any(packet[1:6] == bytes.fromhex("06 00 00 00 04") for packet in port.writes)
        )
        client.acknowledge(result["id"])
        self.assertEqual(port.writes[-1][:-2], bytes.fromhex("01 06 00 00 00 04"))
        self.assertEqual(len(port.responses), 0)

    def test_older_fifo_result_is_not_acknowledged(self):
        port = ScriptedPort([response(4, result_payload(request_id=2))])
        with self.assertRaisesRegex(RuntimeError, "Unread older result 2"):
            Client(port, 1).wait_result(3)
        self.assertEqual(len(port.writes), 1)

    def test_exception_crc_truncation_and_wrong_address(self):
        client = Client(ScriptedPort([response(0x84, b"\x02")]), 1)
        with self.assertRaises(ModbusException) as caught:
            client.read(0xFFFF, 1)
        self.assertEqual(caught.exception.code, 2)
        corrupt = response(4, b"\x02\x00\x01")[:-1] + b"\x00"
        with self.assertRaisesRegex(ValueError, "CRC"):
            Client(ScriptedPort([corrupt]), 1).read(0, 1)
        with self.assertRaises(TimeoutError):
            Client(ScriptedPort([b"\x01"]), 1).read(0, 1)
        with self.assertRaisesRegex(ValueError, "slave address"):
            Client(ScriptedPort([response(4, b"\x02\x00\x01", address=2)]), 1).read(0, 1)

    def test_invalid_submit_never_writes(self):
        port = ScriptedPort([])
        client = Client(port, 1)
        for request_id, target, initial in [
            (-1, [0, 0, 0], [0]),
            (2**32, [0, 0, 0], [0]),
            (1, [0, 0], [0]),
            (1, [math.nan, 0, 0], [0]),
            (1, [0, 0, 0], [0] * 7),
        ]:
            with self.subTest(request_id=request_id, target=target, initial=initial):
                with self.assertRaises(ValueError):
                    client.submit(request_id, target, initial)
        self.assertEqual(port.writes, [])


if __name__ == "__main__":
    unittest.main()
