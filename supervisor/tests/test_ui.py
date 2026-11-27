import os
import time
from types import SimpleNamespace
import pytest

pytestmark = pytest.mark.skipif(
    not os.environ.get("DISPLAY"), reason="Requer servidor gráfico X11"
)


def test_reference_layout_clicks_keyboard_and_estop(monkeypatch):
    from app.ui.main_window import MainWindow
    from app.core.robot_controller import RobotController
    from app.core.robot_state import State
    from app.simulation.mock_robot import MockRobot
    from tkinter import messagebox

    monkeypatch.setattr(messagebox, "askokcancel", lambda *a, **kw: True)
    c = RobotController()
    root = MainWindow(c)
    try:
        root.update()
        root.draw()
        assert root.canvas.winfo_width() == 1024
        assert {name for _, name, _ in root.hits} >= {
            "start",
            "calibrate",
            "estop",
            "diag",
            "race",
            "manual",
            "key-w",
            "key-a",
            "key-s",
            "key-d",
        }
        c.connect("SIMULAÇÃO", 9600, MockRobot())
        for _ in range(3):
            c.tick()
        assert c.connected

        def click(name):
            root.draw()
            box = next(box for box, n, _ in root.hits if n == name)
            x0, y0, x1, y1 = box
            event = SimpleNamespace(
                x=(x0 + x1) / 2 * root.sx, y=(y0 + y1) / 2 * root.sy
            )
            root.mouse_press(event)
            root.mouse_release(event)

        click("race")
        assert root.mode == "race"
        click("diag")
        assert root.mode == "diag"
        click("manual")
        c.transport.last_tx = -100
        c.tick()
        assert c.can_move
        root.manual_modal = True
        root.key_press(SimpleNamespace(keysym="w"))
        assert not c.keys
        root.manual_modal = False
        root.key_press(SimpleNamespace(keysym="w"))
        assert "w" in c.keys
        root.key_release(SimpleNamespace(keysym="w"))
        root.finish_release("w")
        assert not c.keys
        click("estop")
        c.transport.last_tx = -100
        c.tick()
        assert c.robot.estop and c.robot.state == State.STOP
        click("rearm")
        c.transport.last_tx = -100
        c.tick()
        assert not c.robot.estop and not c.can_move
        click("calibrate")
        c.transport.last_tx = -100
        c.tick()
        assert c.robot.state == State.CALIBRATION
        root.geometry("880x680")
        root.update()
        root.draw()
        assert root.sx < 1 and root.sy < 1
    finally:
        root.shutdown()
