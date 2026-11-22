"""E/S real PySerial sobre PTY Linux, sem módulo Bluetooth físico."""

import os
import select
import threading
import time
import pytest
from app.communication.serial_transport import SerialTransport
from app.core.robot_controller import RobotController
from app.simulation.mock_robot import MockRobot


@pytest.mark.skipif(os.name != "posix", reason="PTY Linux")
def test_pyserial_pty_handshake_reconnection_and_disconnect():
    master, slave = os.openpty()
    port = os.ttyname(slave)
    running = threading.Event()
    running.set()

    def bridge():
        robot = MockRobot()
        try:
            while running.is_set():
                if select.select([master], [], [], 0.01)[0]:
                    robot.send(os.read(master, 256))
                robot.poll()
                while not robot.incoming.empty():
                    os.write(master, robot.incoming.get_nowait())
        except OSError:
            pass

    worker = threading.Thread(target=bridge, daemon=True)
    worker.start()
    controller = RobotController()
    try:
        for _ in range(2):
            controller.connect(port, 9600)
            deadline = time.monotonic() + 3
            while not controller.connected and time.monotonic() < deadline:
                controller.tick()
                time.sleep(0.01)
            assert controller.connected and controller.robot.identity
            controller.disconnect()
        controller.connect(port, 9600)
        deadline = time.monotonic() + 3
        while not controller.connected and time.monotonic() < deadline:
            controller.tick()
            time.sleep(0.01)
        assert controller.connected
        running.clear()
        worker.join(0.5)
        os.close(master)
        master = -1
        deadline = time.monotonic() + 2
        while controller.transport and time.monotonic() < deadline:
            controller.tick()
            time.sleep(0.01)
        assert controller.connection == "Comunicação perdida"
    finally:
        running.clear()
        controller.disconnect()
        worker.join(0.5)
        if master >= 0:
            os.close(master)
        os.close(slave)


def test_invalid_serial_port_reports_error():
    controller = RobotController()
    controller.connect("/dev/pegasus-nonexistent", 9600)
    deadline = time.monotonic() + 2
    while controller.transport and time.monotonic() < deadline:
        controller.tick()
        time.sleep(0.01)
    assert controller.connection == "Erro de conexão"
