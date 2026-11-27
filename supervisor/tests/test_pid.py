import os
import pytest
from app.core.pid import PIDSettings, ControlHistory, line_deviation
from app.core.robot_state import RobotState, State
from app.core.robot_controller import RobotController
from app.communication.protocol import Frame, ProtocolError
from app.simulation.mock_robot import MockRobot


@pytest.mark.parametrize(
    "texts",
    [
        ("nan", "0", "0"),
        ("1e9999999999", "0", "0"),
        ("inf", "0", "0"),
        ("-1", "0", "0"),
        ("10.01", "0", "0"),
        ("1", "5.01", "0"),
        ("1", "0", "5.01"),
        ("1.001", "0", "0"),
        ("abc", "0", "0"),
        ("", "0", "0"),
    ],
)
def test_invalid_gains(texts):
    with pytest.raises(ValueError):
        PIDSettings.parse(texts)


def test_decimal_gains_and_telemetry_validation():
    assert PIDSettings.parse(("1,45", "0.08", "0.32")).values == (145, 8, 32)
    assert PIDSettings.parse(("10", "5", "5")).values == (1000, 500, 500)
    state = RobotState()
    state.update(Frame(0, "G", ("145", "8", "32")))
    with pytest.raises(ProtocolError):
        state.update(Frame(0, "G", ("1001", "8", "32")))
    assert state.pid_gains == (145, 8, 32)


def test_readback_and_safe_application():
    c = RobotController()
    mock = MockRobot()
    c.connect("SIMULAÇÃO", 9600, mock)
    for _ in range(4):
        c.tick()
    assert c.robot.pid_gains == (145, 8, 32)
    assert c.apply_pid(PIDSettings(200, 10, 40))
    assert c.robot.pid_gains == (
        145,
        8,
        32,
    )  # Do not accept local edits as device state.
    c.tick()
    assert c.robot.pid_gains == (200, 10, 40)
    c.robot.manual = True
    assert not c.apply_pid(PIDSettings())
    c.robot.manual = False
    c.robot.state = State.RUNNING
    assert not c.apply_pid(PIDSettings())
    c.robot.state = State.STOP
    c.robot.estop = True
    assert not c.apply_pid(PIDSettings())
    c.disconnect()


def test_graph_real_samples_and_metrics():
    state = RobotState(calibrated=True)
    for sensor, value in zip(state.sensors, (0, 0, 255, 255, 0, 0)):
        sensor.value = value
        sensor.low = 0
        sensor.high = 255
        sensor.approved = True
    assert line_deviation(state) == 0
    state.sensors[2].value = 0
    assert line_deviation(state) == 20
    state.sensors[3].value = 0
    assert line_deviation(state) is None
    history = ControlHistory()
    assert history.metrics() == (None, None, None)
    for ms, value in ((0, 50), (1000, 10), (2000, 4), (3000, 2), (4000, 0)):
        history.add(ms, value)
    mean, peak, band = history.metrics()
    assert mean == 13.2 and peak == 50 and band == 2
    history.add(4000, 100)
    assert history.metrics()[1] == 50
    history.add(5000, None)
    assert history.metrics()[2] is None
    history.clear()
    assert not history.samples


@pytest.mark.skipif(not os.environ.get("DISPLAY"), reason="Requer X11")
def test_pid_tab_inputs_restore_and_switching():
    from app.ui.main_window import MainWindow

    c = RobotController()
    root = MainWindow(c)
    try:
        root.update()
        root.switch_tab("pid")
        root.update()
        root.draw()
        assert root.active_tab == "pid" and root.virtual_height == 626
        assert all(
            root.canvas.itemcget(item, "state") == "normal"
            for item in root.pid_panel.items
        )
        assert {name for _, name, _ in root.hits} >= {
            "tab-supervisao",
            "tab-pid",
            "pid-apply",
            "pid-reset",
        }
        root.pid_panel.variables[0].set("abc")
        root.pid_panel.apply()
        assert "válidos" in c.event
        root.pid_panel.restore()
        assert root.pid_panel.variables[0].get() == "1.45"
        c.connect("SIMULAÇÃO", 9600, MockRobot())
        for _ in range(4):
            c.tick()
        root.pid_panel.variables[0].set("2.00")
        root.pid_panel.apply()
        c.tick()
        root.draw()
        assert c.robot.pid_gains == (200, 8, 32)
        root.switch_tab("supervisao")
        root.update()
        root.draw()
        assert all(
            root.canvas.itemcget(item, "state") == "hidden"
            for item in root.pid_panel.items
        )
        assert root.virtual_height == 751
    finally:
        root.shutdown()


def test_pid_readback_is_requested_after_ack():
    c = RobotController()
    mock = MockRobot()
    c.connect("SIMULAÇÃO", 9600, mock)
    for _ in range(4):
        c.tick()
    emit = mock.emit
    skipped = []

    def drop_first_g(seq, kind, *fields):
        if kind == "G" and not skipped:
            skipped.append(True)
            return
        emit(seq, kind, *fields)

    mock.emit = drop_first_g
    c.apply_pid(PIDSettings(220, 5, 20))
    c.tick()
    assert skipped and c.robot.pid_gains == (220, 5, 20)
    c.disconnect()


def test_basic_firmware_reports_race_as_pending():
    from app.communication.protocol import encode

    c = RobotController()
    mock = MockRobot()
    c.connect("SIMULAÇÃO", 9600, mock)
    for _ in range(4):
        c.tick()
    c._receive(Frame(0, "ID", ("PEGASUS", "1", "0", "0")))
    c.robot.calibrated = True
    c.robot.state = State.WAITING
    sequence = c.sequence
    assert not c.command("START")
    assert c.sequence == sequence and "pendente" in c.event
    c.disconnect()
