"""Real pyserial bytes through a Linux PTY into the C++ Modbus server."""
import os
from pathlib import Path
import pty
import subprocess
import sys
import time
import tty

import pytest
import serial

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / 'tests/integration/hardware'))

from tools.modbus_client import Client, ModbusException
from board_support import forward_position, model_for


@pytest.fixture
def client():
    master, slave = pty.openpty()
    process = None
    try:
        tty.setraw(slave)
        with serial.Serial(os.ttyname(slave), 115200, timeout=2, write_timeout=2) as port:
            process = subprocess.Popen([os.environ['PTY_SERVER'], str(master)],
                                       pass_fds=(master,))
            yield Client(port, 1)
            assert process.poll() is None, 'Virtual server stopped unexpectedly'
    finally:
        if process is not None:
            process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=3)
        os.close(master)
        os.close(slave)


@pytest.mark.parametrize('count', [3, 4, 5, 6])
def test_configuration_ik_and_ack(client, count):
    model = model_for(count)
    expected = [.3, .4, -.2] + [.1] * (count - 3)
    target = forward_position(model, expected)
    client.configure(model)
    client.submit(100 + count, target, [.15] * count)
    result = client.wait_result(100 + count, timeout=5)
    assert result['status'] == 0
    actual = forward_position(model, result['angles'][:count])
    assert actual == pytest.approx(target, abs=5e-5)
    assert client.get_result()['id'] == result['id']
    client.acknowledge(result['id'])
    assert client.get_result() is None


def test_modbus_exception_and_recovery(client):
    with pytest.raises(ModbusException) as error:
        client.read(0xffff, 1)
    assert error.value.code == 2
    assert client.status()['version'] == 1


def test_bad_crc_and_recovery(client):
    packet = bytes.fromhex('01 04 00 00 00 18 00 00')
    client.serial.write(packet)
    client.serial.flush()
    assert client.serial.read(1) == b''
    time.sleep(.02)
    assert client.status()['version'] == 1
