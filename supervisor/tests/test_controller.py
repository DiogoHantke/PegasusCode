from queue import Queue
import pytest
from app.core.robot_controller import RobotController, resolve_keys
from app.core.robot_state import RobotState, State, format_time
from app.simulation.mock_robot import MockRobot
from app.communication.protocol import Frame, ProtocolError, encode


class Clock:
    def __init__(self):
        self.now = 10.0

    def __call__(self):
        return self.now

    def advance(self, seconds):
        self.now += seconds


@pytest.fixture
def rig():
    clock = Clock()
    controller = RobotController(clock)
    robot = MockRobot(clock)
    controller.connect("SIMULAÇÃO", 9600, robot)
    for _ in range(3):
        controller.tick()
    assert controller.connected
    return clock, controller, robot


def advance(rig, seconds):
    clock, c, r = rig
    for _ in range(round(seconds / 0.05)):
        clock.advance(0.05)
        c.tick()


def manual(rig):
    _, c, r = rig
    c.command("MANUAL_ENABLE")
    advance(rig, 0.25)
    assert c.can_move


def calibrate(rig):
    _, c, r = rig
    c.command("CALIBRATE")
    advance(rig, 3.5)
    assert c.robot.calibrated and c.robot.state == State.WAITING


def test_validated_handshake_only():
    class Transport:
        def __init__(self):
            self.incoming = Queue()

        def start(self):
            pass

        def send(self, *a, **kw):
            pass

        def close(self):
            pass

    clock = Clock()
    c = RobotController(clock)
    t = Transport()
    c.connect("/dev/fake", 9600, t)
    c.tick()
    t.incoming.put(encode(0, "ID", "PEGASUS", 1, 0))
    c.tick()
    assert c.connection == "Conectando"
    clock.advance(1.1)
    c.tick()
    assert c.connection == "Erro de conexão"


def test_start_confirmation_and_timing(rig):
    _, c, r = rig
    assert not c.command("START")
    calibrate(rig)
    assert c.command("START") and c.robot.state == State.WAITING
    advance(rig, 0.5)
    assert c.robot.state == State.RUNNING and 300 <= c.robot.elapsed_ms <= 550
    assert not c.command("MANUAL_FORWARD", 80)
    advance(rig, 10)
    assert c.robot.laps == 2 and c.robot.last_lap == 5000 and c.robot.best_lap == 5000
    advance(rig, 5)
    assert (
        c.robot.state == State.FINISH and c.robot.laps == 3 and r.left == r.right == 0
    )


def test_wasd_key_release_simultaneous_and_focus(rig):
    _, c, r = rig
    manual(rig)
    c.set_key("W", True)
    advance(rig, 0.3)
    assert r.left == r.right == c.pwm
    c.set_key("s", True)
    advance(rig, 0.15)
    assert r.left == r.right == 0
    c.set_key("w", False)
    advance(rig, 0.15)
    assert r.left == r.right == -c.pwm
    c.focus_lost()
    advance(rig, 0.15)
    assert r.left == r.right == 0 and not c.keys
    for key, expected in [("a", (-80, 80)), ("d", (80, -80))]:
        c.set_key(key, True)
        advance(rig, 0.15)
        assert (r.left, r.right) == expected
        c.set_key(key, False)
        advance(rig, 0.15)
        assert r.left == r.right == 0


def test_deadman_without_controller_poll(rig):
    clock, c, r = rig
    manual(rig)
    c.set_key("w", True)
    advance(rig, 0.2)
    assert r.left
    clock.advance(0.31)
    r.poll()
    assert (
        r.state == State.STOP
        and r.fault == 2
        and not r.manual
        and r.left == r.right == 0
    )


def test_estop_latched_rearm_no_automatic_motion(rig):
    _, c, r = rig
    manual(rig)
    c.set_key("w", True)
    advance(rig, 0.2)
    c.command("ESTOP")
    advance(rig, 0.3)
    assert r.estop and c.robot.estop and not c.can_move and not c.keys
    assert not c.command("MANUAL_FORWARD", 80)
    c.command("MANUAL_ENABLE")
    advance(rig, 0.2)
    assert not r.manual
    c.command("REARM")
    advance(rig, 0.2)
    assert not r.estop and not r.manual and r.left == r.right == 0


def test_stop_cancels_calibration(rig):
    _, c, r = rig
    c.command("CALIBRATE")
    advance(rig, 0.3)
    assert c.robot.state == State.CALIBRATION and not c.can_move
    c.command("STOP")
    advance(rig, 0.3)
    assert r.state == State.STOP and not r.calibrated and r.left == r.right == 0


def test_calibration_failure_diagnostics(rig):
    _, c, r = rig
    r.fail_calibration = True
    c.command("CALIBRATE")
    advance(rig, 3.6)
    assert c.robot.state == State.EMERGENCY and not c.can_move
    assert c.robot.sensors[5].diagnostic(c.robot.state, False) == "FALHA"


def test_link_loss_and_explicit_reconnect(rig):
    clock, c, r = rig
    calibrate(rig)
    c.command("START")
    advance(rig, 0.4)
    started = r.started
    r.drop_link = True
    advance(rig, 2)
    r.poll()
    assert c.connection == "Comunicação perdida" and c.transport is None
    assert r.state == State.RUNNING and r.left == r.right == 80
    c.connect("SIMULAÇÃO", 9600, r)
    for _ in range(4):
        c.tick()
    assert c.connected and c.robot.state == State.RUNNING
    assert r.started == started and c.robot.elapsed_ms >= 2000


def test_duplicate_does_not_restart_or_refresh_movement():
    clock = Clock()
    r = MockRobot(clock)
    r.send(encode(1, "MANUAL_ENABLE"))
    r.send(encode(2, "MANUAL_FORWARD", 80))
    clock.advance(0.2)
    r.send(encode(2, "MANUAL_FORWARD", 80))
    clock.advance(0.11)
    r.poll()
    assert r.state == State.STOP and r.left == 0
    r.send(encode(3, "CALIBRATE"))
    started = r.calibration_started
    clock.advance(0.2)
    r.send(encode(3, "CALIBRATE"))
    assert r.calibration_started == started


def test_sequence_wrap_and_nack_ranges(rig):
    _, c, r = rig
    while c.sequence != 254:
        c.command("GET_STATUS")
        c.tick()
    assert c.sequence == 254
    c.command("GET_STATUS")
    c.tick()
    c.command("GET_STATUS")
    c.tick()
    assert c.sequence == 0
    manual(rig)
    c.command("MANUAL_FORWARD", 256)
    advance(rig, 0.1)
    assert r.left == 0


@pytest.mark.parametrize(
    "keys,expected",
    [
        (set(), "MANUAL_STOP"),
        ({"w"}, "MANUAL_FORWARD"),
        ({"s"}, "MANUAL_BACKWARD"),
        ({"a", "d"}, "MANUAL_STOP"),
        ({"w", "a"}, "MANUAL_STOP"),
    ],
)
def test_key_resolver(keys, expected):
    assert resolve_keys(keys) == expected


@pytest.mark.parametrize(
    "frame",
    [
        Frame(0, "S", ("256", "0", "0", "0", "0", "0", "126")),
        Frame(0, "T", ("99", "0", "0", "0", "0", "0", "0", "0")),
        Frame(0, "D", ("1", "50", "60", "1")),
        Frame(0, "T", ("2", "0", "0", "0", "0", "-1", "0", "0")),
        Frame(0, "S", ("1",)),
    ],
)
def test_invalid_telemetry_transactional(frame):
    state = RobotState()
    with pytest.raises(ProtocolError):
        state.update(frame)
    assert state.state == State.INIT and state.sensors[0].value is None


def test_zero_is_not_failure_and_polarity():
    state = RobotState()
    state.update(Frame(0, "S", ("0", "0", "0", "0", "0", "0", "126")))
    assert state.sensors[0].diagnostic(State.WAITING, False) == "INCONCL."
    state.update(Frame(0, "D", ("1", "0", "240", "1")))
    assert state.sensors[0].diagnostic(State.WAITING, True) == "OK"
    assert (
        state.sensors[0].whiteness(False) == 1 and state.sensors[0].whiteness(True) == 0
    )
    assert format_time(14382) == "00:14.382" and format_time(None) == "--:--.---"


def test_lap_duplicate_and_physical_capability():
    state = RobotState()
    with pytest.raises(ProtocolError):
        state.update(Frame(0, "LAP", ("1", "1000")))
    state.update(Frame(0, "ID", ("PEGASUS", "1", "1")))
    state.update(Frame(0, "LAP", ("1", "1000")))
    state.update(Frame(0, "LAP", ("1", "900")))
    assert state.best_lap == 1000


@pytest.mark.parametrize("task", ["race", "calibration"])
def test_disconnect_does_not_issue_stop_to_autonomous_task(rig, task):
    clock, c, r = rig
    from app.communication.protocol import decode

    if task == "race":
        calibrate(rig)
        c.command("START")
        advance(rig, 0.3)
    else:
        c.command("CALIBRATE")
        advance(rig, 0.3)
    sent = []
    send = r.send

    def record(data, priority=False):
        sent.append(decode(data).kind)
        send(data, priority)

    r.send = record
    c.disconnect()
    assert not sent
    clock.advance(2)
    r.poll()
    if task == "race":
        assert r.state == State.RUNNING and r.left == r.right == 80
    else:
        clock.advance(1)
        r.poll()
        assert r.calibrated and r.state == State.WAITING


def test_disconnect_with_stale_manual_state_does_not_stop_race(rig):
    _, c, r = rig
    calibrate(rig)
    c.command("START")
    advance(rig, 0.3)
    c.robot.manual = True
    c.disconnect()
    assert r.state == State.RUNNING and r.left == r.right == 80


def test_manual_disconnect_stops_and_loss_still_has_deadman(rig):
    clock, c, r = rig
    manual(rig)
    c.set_key("w", True)
    advance(rig, 0.2)
    c.disconnect()
    assert not r.manual and r.left == r.right == 0
    c.connect("SIMULAÇÃO", 9600, r)
    for _ in range(4):
        c.tick()
    manual(rig)
    c.set_key("w", True)
    advance(rig, 0.2)
    r.drop_link = True
    clock.advance(0.31)
    r.poll()
    assert r.state == State.STOP and not r.manual and r.left == r.right == 0


def test_calibration_completes_after_link_loss(rig):
    clock, c, r = rig
    c.command("CALIBRATE")
    advance(rig, 0.4)
    r.drop_link = True
    advance(rig, 2)
    clock.advance(1)
    r.poll()
    assert c.connection == "Comunicação perdida"
    assert r.calibrated and r.state == State.WAITING
