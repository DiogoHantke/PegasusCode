"""Captura reprodução Tk em sessão X11. Dependência opcional: Pillow.

Executar da raiz: python supervisor/tools/capture_visuals.py
Nunca conecta a hardware. Gera imagens explicitamente simuladas.
"""

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from PIL import ImageGrab
from app.core.robot_controller import RobotController
from app.simulation.mock_robot import MockRobot
from app.ui.main_window import MainWindow


class Clock:
    now = 100.0

    def __call__(self):
        return self.now


clock = Clock()
controller = RobotController(clock)
root = MainWindow(controller)
root.geometry("1024x751+100+100")
output = Path(__file__).resolve().parents[2] / "docs/validation"
output.mkdir(exist_ok=True)


def capture(name):
    root.connect_button.configure(
        text="Desconectar" if controller.transport else "Conectar"
    )
    root.identity_label.configure(text="PEGASUS" if controller.connected else "")
    if controller.robot.simulated:
        root.sim_menu.pack(side="right")
    root.update_idletasks()
    root.draw()
    root.update()
    root.update_idletasks()
    # Allow the X11 compositor to present the frame before grabbing pixels.
    time.sleep(0.12)
    x, y = root.winfo_rootx(), root.winfo_rooty()
    ImageGrab.grab(bbox=(x, y, x + root.winfo_width(), y + root.winfo_height())).save(
        output / name
    )


def advance(seconds):
    for _ in range(round(seconds / 0.05)):
        clock.now += 0.05
        controller.tick()


def run():
    capture("tk-desconectado.png")
    root.port.set("SIMULAÇÃO")
    controller.connect("SIMULAÇÃO", 9600, MockRobot(clock))
    for _ in range(3):
        controller.tick()
    controller.command("CALIBRATE")
    advance(3.5)
    capture("tk-diagnostico-simulado.png")
    controller.command("START")
    advance(7.3)
    root.mode = "race"
    capture("tk-corrida-simulada.png")
    root.switch_tab('pid')
    capture('tk-pid-simulado.png')
    root.switch_tab('supervisao')
    controller.command("ESTOP")
    advance(0.3)
    capture("tk-estop-simulado.png")
    root.shutdown()


root.after(200, run)
root.mainloop()
