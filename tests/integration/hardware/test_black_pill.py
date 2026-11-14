#!/usr/bin/env python3
"""Real COM + Black Pill tests. Explicit --port enables access to the device.

RAM model/queues are reset before and after each test. Flash is untouched unless
--flash is passed. Missing configured hardware is an ERROR, not a silent skip.
"""
import argparse
import json
import math
import os
from pathlib import Path
import random
import struct
import subprocess
import sys
import time
import unittest

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(Path(__file__).resolve().parent))

from tools.modbus_client import Client, ModbusException, crc16
from board_support import forward_position, model_for, wait_status

OPTIONS = argparse.Namespace(
    port=os.environ.get("BLACKPILL_PORT"),
    baud=115200,
    address=1,
    no_parity=False,
    flash=False,
    programmer=None,
    stress_queries=100,
)


class BoardCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not OPTIONS.port:
            raise unittest.SkipTest("No board selected: pass --port or BLACKPILL_PORT")
        import serial

        cls.port = serial.Serial(
            OPTIONS.port,
            OPTIONS.baud,
            bytesize=8,
            parity="N" if OPTIONS.no_parity else "E",
            stopbits=2 if OPTIONS.no_parity else 1,
            timeout=3,
            write_timeout=3,
            exclusive=True if os.name == "posix" else None,
        )
        cls.addClassCleanup(cls.port.close)
        cls.client = Client(cls.port, OPTIONS.address)
        if cls.client.status()["version"] != 1:
            raise AssertionError("Board does not expose protocol version 1")

    def reset_ram(self):
        self.client.command(1)
        wait_status(self.client, lambda s: s["mode"] == 0 and not s["busy"])

    def setUp(self):
        self.reset_ram()
        self.addCleanup(self.reset_ram)

    def expect_exception(self, code, action):
        with self.assertRaises(ModbusException) as caught:
            action()
        self.assertEqual(caught.exception.code, code)


class BlackPillTests(BoardCase):
    def test_status_and_invalid_requests(self):
        state = self.client.status()
        self.assertEqual((state["mode"], state["queued"], state["results"]), (0, 0, 0))
        self.expect_exception(2, lambda: self.client.read(65535, 1))
        self.expect_exception(3, lambda: self.client.command(123))
        self.expect_exception(3, lambda: self.client.command(2))  # no model
        self.expect_exception(3, lambda: self.client.command(3))  # no inputs
        self.assertEqual(self.client.status()["version"], 1)

    def test_configuration_and_explicit_ack(self):
        self.client.configure(model_for())
        self.expect_exception(4, lambda: self.client.write(0x100, [3]))
        self.client.submit(101, [0.3, 0, 0.3], [0.1, 0.2, 0.3])
        result = self.client.wait_result(101)
        self.assertEqual(result["status"], 0)
        self.assertEqual(self.client.get_result(), result)  # reading is not ACK
        self.expect_exception(3, lambda: self.client.acknowledge(102))
        self.assertEqual(self.client.get_result(), result)
        self.client.acknowledge(101)
        self.assertIsNone(self.client.get_result())
        self.assertEqual(self.client.status()["results"], 0)

    def test_inverse_kinematics_3_to_6_joints(self):
        rng = random.Random(20261114)
        for count in range(3, 7):
            model = model_for(count)
            self.client.configure(model)
            for sample in range(5):
                with self.subTest(joints=count, sample=sample):
                    reference = [rng.uniform(-0.8, 0.8) for _ in range(count)]
                    target = forward_position(model, reference)
                    result = self.client.solve(
                        200 + count * 10 + sample, target, [value + 0.08 for value in reference]
                    )
                    self.assertEqual(result["status"], 0, result)
                    actual = forward_position(model, result["angles"][:count])
                    self.assertLessEqual(math.dist(actual, target), 2.2e-5)
                    for joint, angle in zip(model["joints"], result["angles"]):
                        self.assertTrue(joint["min"] <= angle <= joint["max"])

    def test_joint_limit_and_nonconvergence(self):
        self.client.configure(model_for())
        result = self.client.solve(301, [0.3, 0, 0.3], [10, 0, 0])
        self.assertEqual(result["status"], 11)
        result = self.client.solve(302, [10, 10, 10], [0.1, 0.2, 0.3])
        self.assertIn(result["status"], (13, 14))
        self.assertEqual(self.client.solve(303, [0.3, 0, 0.3], [0.1, 0.2, 0.3])["status"], 0)

    def test_queue_fifo_and_backpressure(self):
        self.client.configure(model_for())
        for index in range(8):
            self.client.submit(400 + index, [0.3, 0, 0.3], [0.1, 0.2, 0.3])
        wait_status(self.client, lambda s: s["results"] == 8)
        for index in range(8):
            self.client.submit(408 + index, [0.3, 0, 0.3], [0.1, 0.2, 0.3])
        wait_status(self.client, lambda s: s["queued"] == 8 and s["results"] == 8)
        self.expect_exception(4, lambda: self.client.submit(999, [0.3, 0, 0.3], [0.1, 0.2, 0.3]))
        # SAVE must be refused before flash access, even in the default no-flash suite.
        self.expect_exception(4, lambda: self.client.command(5))
        for index in range(16):
            result = self.client.wait_result(400 + index)
            self.assertEqual(result["status"], 0)
            self.client.acknowledge(result["id"])
        state = wait_status(self.client, lambda s: not s["busy"] and s["queued"] == 0)
        self.assertEqual(state["results"], 0)

    def test_cancel_and_reinitialize(self):
        self.client.configure(model_for(6))
        self.client.submit(501, [10, 10, 10], [0.1, 0.2, 0.3, 0.1, 0.2, 0.3])
        self.reset_ram()
        state = self.client.status()
        self.assertEqual((state["queued"], state["results"]), (0, 0))
        self.assertIsNone(self.client.get_result())
        self.client.configure(model_for())
        self.assertEqual(self.client.solve(502, [0.3, 0, 0.3], [0.1, 0.2, 0.3])["status"], 0)

    def test_bad_crc_foreign_slave_and_recovery(self):
        foreign = 2 if OPTIONS.address != 2 else 3
        valid = bytes([OPTIONS.address, 4, 0, 0, 0, 24])
        invalid_crc = struct.pack("<H", crc16(valid) ^ 1)
        foreign_packet = bytes([foreign, 4, 0, 0, 0, 24])
        packets = [valid + invalid_crc, foreign_packet + struct.pack("<H", crc16(foreign_packet))]
        for packet in packets:
            with self.subTest(packet=packet.hex()):
                old_timeout = self.port.timeout
                try:
                    self.port.timeout = 0.2
                    self.port.reset_input_buffer()
                    self.port.write(packet)
                    self.port.flush()
                    self.assertEqual(self.port.read(256), b"")
                finally:
                    self.port.timeout = old_timeout
                self.assertEqual(self.client.status()["version"], 1)

    def test_repeated_status(self):
        for index in range(OPTIONS.stress_queries):
            with self.subTest(query=index):
                state = self.client.status()
                self.assertEqual((state["version"], state["mode"]), (1, 0))


class FlashPersistenceTests(BoardCase):
    @classmethod
    def setUpClass(cls):
        if not OPTIONS.flash:
            raise unittest.SkipTest("Flash tests disabled: pass --flash --programmer PATH")
        super().setUpClass()

    def test_save_reset_load_apply(self):
        self.client.configure(model_for())
        # Exercise both copies, without changing UART settings or slave address.
        self.client.command(5)
        self.client.command(5)
        expected = self.client.read(0x100, 100, 3) + self.client.read(0x164, 61, 3)
        self.port.close()
        try:
            reset = subprocess.run(
                [OPTIONS.programmer, "-c", "port=SWD", "freq=400", "-rst"],
                capture_output=True,
                text=True,
                timeout=30,
                check=False,
            )
            self.assertEqual(reset.returncode, 0, reset.stdout + reset.stderr)
            time.sleep(0.3)
        finally:
            self.port.open()
        self.assertEqual(self.client.status()["mode"], 0)
        restored = self.client.read(0x100, 100, 3) + self.client.read(0x164, 61, 3)
        self.assertEqual(restored, expected)
        self.client.command(2)
        wait_status(self.client, lambda s: s["mode"] == 2)
        self.assertEqual(self.client.solve(601, [0.3, 0, 0.3], [0.1, 0.2, 0.3])["status"], 0)
        self.reset_ram()
        self.client.command(6)
        self.client.command(2)
        wait_status(self.client, lambda s: s["mode"] == 2)


def main():
    global OPTIONS
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default=os.environ.get("BLACKPILL_PORT"))
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--address", type=int, default=1)
    parser.add_argument("--no-parity", action="store_true", help="8N2 instead of 8E1")
    parser.add_argument("--stress-queries", type=int, default=100)
    parser.add_argument("--flash", action="store_true")
    parser.add_argument("--programmer")
    parser.add_argument("--output", type=Path, help="Optional JSON summary; no file by default")
    OPTIONS = parser.parse_args()
    if OPTIONS.flash and not OPTIONS.programmer:
        parser.error("--flash requires --programmer")
    if not 1 <= OPTIONS.address <= 247 or OPTIONS.stress_queries < 1:
        parser.error("address must be 1..247 and stress-queries must be positive")
    suite = unittest.TestSuite(
        [
            unittest.defaultTestLoader.loadTestsFromTestCase(BlackPillTests),
            unittest.defaultTestLoader.loadTestsFromTestCase(FlashPersistenceTests),
        ]
    )
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    status = "FAIL" if not result.wasSuccessful() else "SKIP" if result.testsRun == 0 else "PASS"
    if OPTIONS.output:
        OPTIONS.output.write_text(
            json.dumps(
                {
                    "run": result.testsRun,
                    "skipped": len(result.skipped),
                    "failures": len(result.failures),
                    "errors": len(result.errors),
                    "status": status,
                    "success": None if status == "SKIP" else result.wasSuccessful(),
                },
                indent=2,
            )
            + "\n"
        )
    if not result.wasSuccessful():
        return 1
    return 77 if result.testsRun == 0 else 0


if __name__ == "__main__":
    sys.exit(main())
