"""Robô virtual explícito. Mesmo wire protocol; não substitui ensaios físicos."""

from queue import Queue, Empty
import math
import time
from collections.abc import Callable
from ..communication.protocol import StreamParser, encode
from ..core.robot_state import State


class MockRobot:
    def __init__(self, clock: Callable[[], float] = time.monotonic) -> None:
        self.clock = clock
        self.incoming: Queue[bytes | Exception] = Queue(maxsize=128)
        self.parser = StreamParser()
        self.state = State.INIT
        self.manual = False
        self.estop = False
        self.calibrated = False
        self.fault = 0
        self.left = 0
        self.right = 0
        self.white_high = False
        self.last_move = self.clock()
        self.last_tx = -100.0
        self.started = 0.0
        self.elapsed = 0
        self.laps = 0
        self.last_lap = 0
        self.calibration_started = 0.0
        self.phase = 0
        self.drop_link = False
        self.fail_calibration = False
        self.closed = False
        self.cache: dict[int, tuple[bytes, int]] = {}
        self.period = 0.2
        self.pid_gains = (145, 8, 32)

    def start(self) -> None:
        self.closed = False
        self.drop_link = False
        self.parser = StreamParser()
        self.last_tx = -100.0
        while True:
            try:
                self.incoming.get_nowait()
            except Empty:
                break

    def close(self) -> None:
        self.closed = True
        # Closing the serial endpoint does not power off the virtual robot.

    def emit(self, seq: int, kind: str, *fields: object) -> None:
        if not self.drop_link and not self.closed:
            self.incoming.put_nowait(encode(seq, kind, *fields))

    def halt(self) -> None:
        self.left = self.right = 0

    def send(self, data: bytes, priority: bool = False) -> None:
        if self.drop_link or self.closed:
            return
        for frame in self.parser.feed(data):
            if (
                frame.kind
                in ("ESTOP", "STOP", "PING", "GET_STATUS", "GET_SENSORS", "GET_PID")
                and not frame.fields
            ):
                self.execute(frame.kind, frame.fields)
                self.emit(frame.sequence, "ACK", 0)
                continue
            cached = self.cache.get(frame.sequence)
            if cached:
                self.emit(
                    frame.sequence,
                    "ACK" if cached[1] == 0 else "NACK",
                    cached[1] if cached[0] == data else 5,
                )
                continue
            result = self.execute(frame.kind, frame.fields)
            self.cache[frame.sequence] = (data, result)
            if len(self.cache) > 8:
                del self.cache[next(iter(self.cache))]
            self.emit(frame.sequence, "NACK" if result else "ACK", result)

    def execute(self, kind: str, fields: tuple[str, ...]) -> int:
        if kind == "SET_PID":
            if len(fields) != 3 or any(not x.isdigit() for x in fields):
                return 2
            values = tuple(int(x) for x in fields)
            if any(v > limit for v, limit in zip(values, (1000, 500, 500))):
                return 2
            if self.estop:
                return 4
            if self.manual or self.state not in (
                State.INIT,
                State.WAITING,
                State.STOP,
                State.FINISH,
            ):
                return 3
            self.pid_gains = values
            self.emit(0, "G", *values)
            return 0
        takes_value = kind in (
            "TELEM",
            "MANUAL_FORWARD",
            "MANUAL_BACKWARD",
            "MANUAL_LEFT",
            "MANUAL_RIGHT",
        )
        if takes_value:
            if len(fields) != 1 or not fields[0].isdigit():
                return 2
            value = int(fields[0])
        elif fields:
            return 2
        else:
            value = 0
        if kind in ("ESTOP", "STOP"):
            self.halt()
            self.manual = False
            if self.state == State.CALIBRATION:
                self.calibrated = False
            self.state = State.STOP
            if kind == "ESTOP":
                self.estop = True
        elif kind == "PING":
            self.emit(0, "ID", "PEGASUS", 1, 1)
        elif kind == "GET_PID":
            self.emit(0, "G", *self.pid_gains)
        elif kind == "GET_STATUS":
            self.status()
        elif kind == "GET_SENSORS":
            self.sensors()
        elif kind == "TELEM":
            if not 200 <= value <= 1000:
                return 2
            self.period = value / 1000
        elif kind == "REARM":
            if self.state != State.STOP:
                return 3
            self.halt()
            self.estop = False
            self.fault = 0
            self.manual = False
            self.state = State.WAITING
        elif self.estop:
            return 4
        elif kind == "START":
            if (
                not self.calibrated
                or self.manual
                or self.state not in (State.WAITING, State.STOP)
            ):
                return 3
            self.state = State.RUNNING
            self.started = self.clock()
            self.elapsed = 0
            self.laps = 0
            self.last_lap = 0
            self.fault = 0
            self.left = self.right = 80
        elif kind in ("CALIBRATE", "MANUAL_ENABLE"):
            if self.state in (State.RUNNING, State.CALIBRATION, State.EMERGENCY):
                return 3
            self.halt()
            self.fault = 0
            if kind == "CALIBRATE":
                self.state = State.CALIBRATION
                self.manual = False
                self.calibrated = False
                self.calibration_started = self.clock()
            else:
                self.manual = True
                self.state = State.WAITING
                self.last_move = self.clock()
        elif kind == "MANUAL_DISABLE":
            if self.state in (State.RUNNING, State.CALIBRATION):
                return 3
            if self.manual:
                self.halt()
                self.manual = False
                self.state = State.WAITING
        elif kind == "MANUAL_STOP":
            if not self.manual:
                return 3
            self.halt()
            self.last_move = self.clock()
        elif kind.startswith("MANUAL_"):
            if not self.manual or self.state != State.WAITING:
                return 3
            if not 0 <= value <= 160:
                return 2
            directions = {
                "MANUAL_FORWARD": (1, 1),
                "MANUAL_BACKWARD": (-1, -1),
                "MANUAL_LEFT": (-1, 1),
                "MANUAL_RIGHT": (1, -1),
            }
            if kind not in directions:
                return 1
            a, b = directions[kind]
            self.left = a * value
            self.right = b * value
            self.last_move = self.clock()
        else:
            return 1
        return 0

    def status(self) -> None:
        self.emit(
            0,
            "T",
            int(self.state),
            int(self.manual) | (int(self.estop) << 1) | (int(self.calibrated) << 2),
            self.fault,
            self.left,
            self.right,
            self.elapsed,
            self.phase,
            int(self.white_high),
        )

    def sensors(self) -> None:
        t = self.clock()
        values = [round(128 + 110 * math.sin(t * 2 + i * 0.9)) for i in range(6)]
        if self.state == State.RUNNING:
            # Illustrative virtual plant, never a prediction of the real robot.
            kp, ki, kd = (v / 100 for v in self.pid_gains)
            seconds = max(0, t - self.started)
            damping = max(0.25, min(2.2, 0.5 + kd * 1.4 - kp * 0.12))
            frequency = max(1.2, min(4.5, 1.5 + kp * 0.7))
            position = (
                2.4
                * math.exp(-damping * seconds * 0.55)
                * math.cos(frequency * seconds)
            )
            position += ki * 0.1 * math.exp(-0.4 * seconds)
            values = [
                round(18 + 220 * math.exp(-(((i - 2.5 - position) / 0.8) ** 2)))
                for i in range(6)
            ]
            if self.white_high:
                values = [255 - v for v in values]
        self.emit(0, "S", *values, 126)
        for i in range(1, 7):
            ok = self.calibrated or (self.state == State.EMERGENCY and i != 6)
            self.emit(0, "D", i, 18 if ok else 255, 238 if ok else 0, int(ok))

    def poll(self) -> None:
        now = self.clock()
        if self.manual and now - self.last_move > 0.3:
            self.halt()
            self.manual = False
            self.fault = 2
            self.state = State.STOP
        if self.state == State.CALIBRATION:
            self.phase = min(5, int((now - self.calibration_started) * 2))
            self.left = self.right = 51
            if now - self.calibration_started >= 3:
                self.halt()
                self.phase = 6 if self.fail_calibration else 5
                self.calibrated = not self.fail_calibration
                self.state = State.EMERGENCY if self.fail_calibration else State.WAITING
                if self.fail_calibration:
                    self.fault = 4
                self.fail_calibration = False
        if self.state == State.RUNNING:
            self.elapsed = int((now - self.started) * 1000)
            count = self.elapsed // 5000
            if count > self.laps:
                self.laps = count
                self.last_lap = 5000
                self.emit(0, "LAP", count, 5000)
                if count >= 3:
                    self.halt()
                    self.state = State.FINISH
        if now - self.last_tx >= self.period:
            self.last_tx = now
            self.status()
            self.sensors()
