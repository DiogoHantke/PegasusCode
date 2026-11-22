"""Reprodução Canvas do HTML: grid 7/5 e 8/4, gaps 16, sensores 64px."""

import tkinter as tk
from tkinter import ttk, messagebox, font
from collections.abc import Callable
from ..communication.serial_transport import available_ports
from ..simulation.mock_robot import MockRobot
from ..core.robot_controller import RobotController
from ..core.robot_state import State, STATE_LABELS, format_time
from ..core.config import BAUD_RATES, DEFAULT_BAUD, MAX_MANUAL_PWM
from . import theme as c
from .pid_panel import PIDPanel


class MainWindow(tk.Tk):
    def __init__(self, controller: RobotController, simulate: bool = False) -> None:
        super().__init__()
        self.controller = controller
        self.title("PEGASUS — Supervisório / Controlador — Robô Seguidor de Linha")
        self.geometry("1024x751")
        self.minsize(880, 705)
        self.configure(bg=c.FRAME)
        self.tk.call("tk", "scaling", 1.0)
        families = set(font.families(self))
        self.body_font = "Reddit Sans" if "Reddit Sans" in families else "DejaVu Sans"
        self.mono_font = (
            "Source Code Pro" if "Source Code Pro" in families else "DejaVu Sans Mono"
        )
        self.canvas = tk.Canvas(self, bg=c.FRAME, highlightthickness=0)
        self.canvas.pack(fill="both", expand=True)
        self.port = tk.StringVar(value="SIMULAÇÃO" if simulate else "")
        self.baud = tk.StringVar(value=str(DEFAULT_BAUD))
        self.pwm = tk.StringVar(value=str(controller.pwm))
        style = ttk.Style(self)
        style.theme_use("clam")
        style.configure("TCombobox", font=(self.body_font, 10))
        style.configure("TButton", font=(self.body_font, 10), padding=(5, 2))
        self.connection_bar = tk.Frame(self.canvas, bg=c.HEADER)
        tk.Label(
            self.connection_bar,
            text="Bluetooth",
            bg=c.HEADER,
            fg=c.MUTED,
            font=(self.body_font, 11),
        ).pack(side="left", padx=(0, 8))
        self.port_box = ttk.Combobox(
            self.connection_bar, textvariable=self.port, width=24
        )
        self.port_box.pack(side="left", padx=(0, 4))
        ttk.Button(
            self.connection_bar, text="↻", width=3, command=self.refresh_ports
        ).pack(side="left", padx=(0, 8))
        ttk.Combobox(
            self.connection_bar,
            textvariable=self.baud,
            values=BAUD_RATES,
            width=8,
            state="readonly",
        ).pack(side="left")
        tk.Label(
            self.connection_bar,
            text="bps",
            bg=c.HEADER,
            fg=c.MUTED,
            font=(self.body_font, 10),
        ).pack(side="left", padx=4)
        self.connect_button = ttk.Button(
            self.connection_bar, text="Conectar", command=self.toggle_connection
        )
        self.connect_button.pack(side="left", padx=4)
        self.identity_label = tk.Label(
            self.connection_bar,
            text="",
            bg=c.HEADER,
            fg=c.MUTED,
            font=(self.body_font, 10),
        )
        self.identity_label.pack(side="left", padx=8)
        self.sim_menu = sim_menu = ttk.Menubutton(
            self.connection_bar, text="Simulação ▾"
        )
        menu = tk.Menu(sim_menu, tearoff=False)
        menu.add_command(
            label="Perder comunicação", command=lambda: self.sim_fault("link")
        )
        menu.add_command(
            label="Falhar próxima calibração",
            command=lambda: self.sim_fault("calibration"),
        )
        sim_menu.configure(menu=menu)
        self.bar_item = self.canvas.create_window(
            14, 49, window=self.connection_bar, anchor="nw"
        )
        self.hits: list[
            tuple[tuple[float, float, float, float], str, Callable[[], None]]
        ] = []
        self.active_tab = "supervisao"
        self.content_shift = 0
        self.virtual_height = 751
        self.pid_panel = PIDPanel(self)
        self.manual_modal = False
        self.mode = "diag"
        self.pressed = ""
        self.mouse_key = ""
        self.was_running = False
        self.sim_robot = MockRobot()
        self.key_releases: dict[str, str] = {}
        self.canvas.bind("<ButtonPress-1>", self.mouse_press)
        self.canvas.bind("<ButtonRelease-1>", self.mouse_release)
        self.bind_all("<KeyPress>", self.key_press)
        self.bind_all("<KeyRelease>", self.key_release)
        self.bind("<FocusOut>", lambda e: self.after_idle(self.check_focus))
        self.protocol("WM_DELETE_WINDOW", self.shutdown)
        self.refresh_ports()
        self.after(30, self.update_ui)
        if simulate:
            self.after(100, self.toggle_connection)

    def sim_fault(self, kind: str) -> None:
        transport = self.controller.transport
        if not isinstance(transport, MockRobot):
            self.controller.event = "Injeção de falha disponível somente na SIMULAÇÃO"
            return
        if kind == "link":
            transport.drop_link = True
        else:
            transport.fail_calibration = True
        self.controller.event = "SIMULAÇÃO: falha injetada · " + kind

    def refresh_ports(self) -> None:
        self.port_box["values"] = ["SIMULAÇÃO", *available_ports()]

    def toggle_connection(self) -> None:
        if self.controller.transport:
            self.controller.disconnect()
            return
        port = self.port.get().strip()
        if not port:
            self.controller.event = "Selecione ou digite a porta serial"
            return
        self.controller.connect(
            port, int(self.baud.get()), self.sim_robot if port == "SIMULAÇÃO" else None
        )

    def calibrate(self) -> None:
        if not self.controller.connected:
            self.controller.command("CALIBRATE")
            return
        self.controller.focus_lost()
        self.manual_modal = True
        try:
            confirmed = messagebox.askokcancel(
                "Calibração com movimento",
                "A rotina embarcada movimenta os motores. Deixe as rodas e a pista livres. Iniciar calibração?",
                parent=self,
            )
        finally:
            self.manual_modal = False
        if confirmed:
            self.controller.command("CALIBRATE")

    def switch_tab(self, tab: str) -> None:
        if tab == self.active_tab:
            return
        self.controller.focus_lost()
        if tab == "pid" and self.controller.can_move:
            self.controller.command("MANUAL_DISABLE")
        ratio = self.winfo_height() / self.virtual_height
        self.active_tab = tab
        self.virtual_height = 626 if tab == "pid" else 751
        self.minsize(880, 580 if tab == "pid" else 705)
        self.geometry(f"{self.winfo_width()}x{round(self.virtual_height*ratio)}")
        self.update_idletasks()
        self.draw()

    def set_mode(self, mode: str) -> None:
        self.mode = mode

    def check_focus(self) -> None:
        if self.focus_displayof() is None:
            self.mouse_key = ""
            self.controller.focus_lost()

    def key_press(self, event: tk.Event) -> None:
        if self.manual_modal:
            return
        if self.active_tab != "supervisao":
            if event.keysym == "Escape":
                self.controller.command("STOP")
            return
        key = event.keysym.lower()
        if key in self.key_releases:
            self.after_cancel(self.key_releases.pop(key))
        if isinstance(
            self.focus_get(), (ttk.Entry, ttk.Combobox, tk.Entry, tk.Spinbox)
        ):
            return
        self.controller.set_key(key, True)
        if event.keysym == "Escape":
            self.controller.command("STOP")

    def key_release(self, event: tk.Event) -> None:
        key = event.keysym.lower()
        # X11 autorepeat is Release/Press; defer release so it does not pulse the motors.
        self.key_releases[key] = self.after(20, lambda: self.finish_release(key))

    def finish_release(self, key: str) -> None:
        self.key_releases.pop(key, None)
        self.controller.set_key(key, False)

    def mouse_press(self, event: tk.Event) -> None:
        x, y = event.x / self.sx, event.y / self.sy
        for (x0, y0, x1, y1), name, action in reversed(self.hits):
            if x0 <= x <= x1 and y0 <= y <= y1:
                self.pressed = name
                if name.startswith("key-"):
                    self.mouse_key = name[-1]
                    self.controller.set_key(self.mouse_key, True)
                return

    def mouse_release(self, event: tk.Event) -> None:
        if self.mouse_key:
            self.controller.set_key(self.mouse_key, False)
            self.mouse_key = ""
        x, y = event.x / self.sx, event.y / self.sy
        for (x0, y0, x1, y1), name, action in reversed(self.hits):
            if name == self.pressed and x0 <= x <= x1 and y0 <= y <= y1:
                if not name.startswith("key-"):
                    action()
                break
        self.pressed = ""

    def rect(
        self,
        x: float,
        y: float,
        w: float,
        h: float,
        fill: str,
        border: str = c.OUTLINE,
        width: int = 1,
    ) -> None:
        y += self.content_shift
        self.canvas.create_rectangle(
            x * self.sx,
            y * self.sy,
            (x + w) * self.sx,
            (y + h) * self.sy,
            fill=fill,
            outline=border,
            width=width,
            tags="drawing",
        )

    def text(
        self,
        x: float,
        y: float,
        text: str,
        size: int = 11,
        color: str = c.TEXT,
        bold: bool = False,
        mono: bool = False,
        anchor: str = "w",
    ) -> None:
        y += self.content_shift
        self.canvas.create_text(
            x * self.sx,
            y * self.sy,
            text=text,
            fill=color,
            font=(
                self.mono_font if mono else self.body_font,
                -max(9, round(size * min(self.sx, self.sy))),
                "bold" if bold else "normal",
            ),
            anchor=anchor,
            justify="center",
            tags="drawing",
        )

    def button(
        self,
        x: float,
        y: float,
        w: float,
        h: float,
        text: str,
        name: str,
        action: Callable[[], None],
        fill: str = c.FRAME,
        color: str = c.TEXT,
        size: int = 13,
    ) -> None:
        active = self.pressed == name or (
            name.startswith("key-") and name[-1] in self.controller.keys
        )
        self.rect(x, y, w, h, c.BLACK if active else fill)
        self.text(
            x + w / 2,
            y + h / 2,
            text,
            size,
            c.WHITE if active else color,
            True,
            anchor="center",
        )
        self.hits.append(
            (
                (x, y + self.content_shift, x + w, y + h + self.content_shift),
                name,
                action,
            )
        )

    def badge(
        self,
        x: float,
        y: float,
        w: float,
        text: str,
        fill: str = c.FRAME,
        color: str = c.TEXT,
    ) -> None:
        self.rect(x, y, w, 25, fill)
        self.text(x + w / 2, y + 12.5, text, 11, color, mono=True, anchor="center")

    def draw(self) -> None:
        r = self.controller.robot
        self.sx = max(1, self.canvas.winfo_width()) / 1024
        self.sy = max(1, self.canvas.winfo_height()) / self.virtual_height
        self.content_shift = 0
        self.canvas.delete("drawing")
        self.hits.clear()
        self.rect(1, 1, 1022, self.virtual_height - 2, c.FRAME, width=2)
        self.rect(2, 2, 1020, 42, c.HEADER)
        self.text(
            14,
            23,
            "♙  Supervisório / Controlador — Robô Seguidor de Linha",
            15,
            bold=True,
        )
        connected = self.controller.connected
        color = (
            c.SUCCESS
            if connected
            else (
                c.ERROR
                if self.controller.connection
                in ("Comunicação perdida", "Erro de conexão")
                else c.BLACK
            )
        )
        self.rect(782, 10, 228, 25, c.WHITE, c.BORDER)
        self.canvas.create_oval(
            791 * self.sx,
            18 * self.sy,
            801 * self.sx,
            28 * self.sy,
            fill=color,
            outline=color,
            tags="drawing",
        )
        self.text(
            809,
            23,
            ("SIMULADO · " if r.simulated else "") + self.controller.connection,
            11,
            color,
        )
        self.rect(2, 44, 1020, 34, c.HEADER, c.BORDER)
        self.canvas.coords(self.bar_item, 14 * self.sx, 49 * self.sy)
        self.canvas.itemconfigure(self.bar_item, width=996 * self.sx)
        self.canvas.tag_raise(self.bar_item)
        self.rect(2, 78, 1020, 45, c.HEADER)
        self.button(
            18,
            88,
            170,
            35,
            "Supervisão & Controle",
            "tab-supervisao",
            lambda: self.switch_tab("supervisao"),
            c.FRAME if self.active_tab == "supervisao" else "#DDD9D9",
            size=13,
        )
        self.button(
            192,
            88,
            200,
            35,
            "PID & Gráfico de Controle",
            "tab-pid",
            lambda: self.switch_tab("pid"),
            c.FRAME if self.active_tab == "pid" else "#DDD9D9",
            size=13,
        )
        if self.active_tab == "pid":
            self.pid_panel.draw()
            self.draw_footer()
            return
        self.pid_panel.hide()
        self.content_shift = 45
        # First row: original 7/12 and 5/12 columns.
        self.rect(18, 94, 570, 229, c.PANEL)
        self.rect(604, 94, 402, 229, c.PANEL)
        self.text(35, 123, "Tempo de Volta", 15, bold=True)
        self.badge(
            489, 111, 82, f"Volta #{r.laps:02d}" if r.laps_supported else "Voltas: —"
        )
        self.rect(35, 144, 536, 1, c.BORDER, c.BORDER)
        self.rect(35, 160, 536, 98, c.WHITE)
        self.text(303, 185, "Cronômetro atual", 11, c.MUTED, anchor="center")
        self.text(
            303,
            220,
            format_time(r.elapsed_ms) if r.race_supported else "--:--.---",
            40,
            bold=True,
            mono=True,
            anchor="center",
        )
        for x, title, value in (
            (35, "Última Volta", r.last_lap),
            (309, "Melhor Volta", r.best_lap),
        ):
            self.rect(x, 272, 262, 34, c.WHITE, c.BORDER)
            self.text(x + 12, 289, title, 11, c.MUTED)
            self.text(
                x + 250, 289, format_time(value), 12, bold=True, mono=True, anchor="e"
            )
        self.text(621, 123, "Comandos de Operação", 15, bold=True)
        state_label = "Emergência" if r.estop else STATE_LABELS[r.state]
        if r.manual:
            state_label = "Manual"
        elif r.state in (State.INIT, State.WAITING) and not r.calibrated:
            state_label = "Não calibrado"
        if not connected:
            state_label = "—"
        self.badge(
            862,
            111,
            127,
            state_label,
            (
                c.ERROR
                if r.estop
                else ("#DCFCE7" if connected and r.state == State.RUNNING else c.FRAME)
            ),
            c.WHITE if r.estop else c.TEXT,
        )
        self.rect(621, 144, 368, 1, c.BORDER, c.BORDER)
        self.button(
            621,
            158,
            368,
            44,
            "▷  Start de Corrida",
            "start",
            lambda: self.controller.command("START"),
            c.SUCCESS if r.race_supported else c.FRAME,
            c.WHITE if r.race_supported else c.MUTED,
        )
        self.button(621, 210, 368, 44, "☷  Calibração", "calibrate", self.calibrate)
        self.button(
            621,
            262,
            368,
            44,
            "◇  Parada de Emergência",
            "estop",
            lambda: self.controller.command("ESTOP"),
            c.ERROR,
            c.WHITE,
        )
        # Second row: original 8/12 and 4/12 columns.
        self.rect(18, 339, 653, 323, c.PANEL)
        self.rect(687, 339, 319, 323, c.PANEL)
        self.text(35, 366, "Diagnóstico de Sensores", 15, bold=True)
        self.text(35, 385, "Leituras em tempo real · 0–255", 11, c.MUTED)
        self.rect(439, 357, 215, 34, c.FRAME)
        self.button(
            444,
            362,
            86,
            24,
            "Diagnóstico",
            "diag",
            lambda: self.set_mode("diag"),
            c.BLACK if self.mode == "diag" else c.FRAME,
            c.WHITE if self.mode == "diag" else c.TEXT,
            11,
        )
        self.button(
            534,
            362,
            115,
            24,
            "Tempo de Corrida",
            "race",
            lambda: self.set_mode("race"),
            c.BLACK if self.mode == "race" else c.FRAME,
            c.WHITE if self.mode == "race" else c.TEXT,
            11,
        )
        self.rect(35, 400, 619, 1, c.BORDER, c.BORDER)
        for i, sensor in enumerate(r.sensors):
            x = 35 + i * 105.2
            self.rect(x, 414, 92, 153, c.WHITE, c.BORDER)
            self.text(
                x + 46,
                445,
                "---" if sensor.value is None else f"{sensor.value:03d}",
                20,
                bold=True,
                mono=True,
                anchor="center",
            )
            label = sensor.diagnostic(r.state, r.calibrated)
            fill = (
                c.SUCCESS if label == "OK" else c.ERROR if label == "FALHA" else c.BLACK
            )
            fg = c.WHITE
            if not connected:
                label = "—"
                fill = c.BLACK
            elif self.mode == "race":
                white = sensor.whiteness(r.white_high) if r.calibrated else None
                if white is None:
                    label = "—"
                    fill = c.BLACK
                elif white < 0.5:
                    label = "PRETO"
                    fill = c.BLACK
                else:
                    label = "BRANCO"
                    # Continuous green intensity, normalized with real calibration and polarity.
                    ratio = (white - 0.5) * 2
                    low = (21, 128, 61)
                    high = (134, 239, 172)
                    fill = "#" + "".join(
                        f"{round(a+(b-a)*ratio):02X}" for a, b in zip(low, high)
                    )
                    fg = "#0F2918" if white > 0.75 else c.WHITE
            if label in ("NÃO CAL.", "SEM DADOS", "INCONCL."):
                label = "—"
            self.rect(x + 14, 471, 64, 64, fill)
            self.text(
                x + 46,
                503,
                label,
                10 if len(label) > 6 else 12,
                fg,
                True,
                anchor="center",
            )
            self.text(x + 46, 551, f"S{i+1}", 12, bold=True, mono=True, anchor="center")
        self.rect(35, 580, 619, 65, c.WHITE, c.BORDER)
        legend = (
            ((c.SUCCESS, "Aprovado"), (c.ERROR, "Falha"), (c.BLACK, "Sem diagnóstico"))
            if self.mode == "diag"
            else ((c.SUCCESS, "Branco"), (c.BLACK, "Preto"))
        )
        for i, (fill, label) in enumerate(legend):
            x = 55 + i * 195
            self.rect(x, 606, 13, 13, fill)
            self.text(x + 21, 613, label, 11)
        self.text(704, 366, "Controle à Distância (WASD)", 15, bold=True)
        self.rect(704, 384, 285, 1, c.BORDER, c.BORDER)
        self.button(
            704,
            397,
            125,
            24,
            "Sair do manual" if r.manual else "Ativar manual",
            "manual",
            lambda: self.controller.command(
                "MANUAL_DISABLE" if r.manual else "MANUAL_ENABLE"
            ),
            size=11,
        )
        if r.manual or r.state in (State.RUNNING, State.CALIBRATION):
            self.button(
                837,
                397,
                65,
                24,
                "Parar",
                "stop",
                lambda: self.controller.command("STOP"),
                size=11,
            )
        if r.estop or r.state == State.STOP:
            self.button(
                837,
                397,
                90,
                24,
                "Rearmar",
                "rearm",
                lambda: self.controller.command("REARM"),
                size=11,
            )
        if r.manual:
            self.button(
                910,
                397,
                79,
                24,
                f"PWM {self.controller.pwm}",
                "pwm",
                self.configure_pwm,
                size=11,
            )
        for key, x, y, label in (
            ("w", 819, 439, "Frente"),
            ("a", 755, 503, "Esq"),
            ("s", 819, 503, "Ré"),
            ("d", 883, 503, "Dir"),
        ):
            self.button(x, y, 56, 56, "", "key-" + key, lambda: None)
            active = key in self.controller.keys
            self.text(
                x + 28,
                y + 21,
                key.upper(),
                20,
                c.WHITE if active else c.TEXT,
                True,
                True,
                anchor="center",
            )
            self.text(
                x + 28,
                y + 41,
                label,
                11,
                c.WHITE if active else c.TEXT,
                anchor="center",
            )
        self.rect(704, 612, 285, 33, c.WHITE, c.BORDER)
        self.text(716, 629, "Último comando", 11, c.MUTED)
        names = {
            "Parado": "Parado",
            "START": "Iniciar corrida",
            "CALIBRATE": "Calibrar",
            "ESTOP": "Emergência",
            "STOP": "Parar",
            "REARM": "Rearmar",
            "MANUAL_ENABLE": "Ativar manual",
            "MANUAL_DISABLE": "Sair do manual",
            "MANUAL_STOP": "Parado",
            "MANUAL_FORWARD": "Frente",
            "MANUAL_BACKWARD": "Ré",
            "MANUAL_LEFT": "Esquerda",
            "MANUAL_RIGHT": "Direita",
        }
        short = names.get(self.controller.last_command, "—")
        self.text(977, 629, short, 11, bold=True, anchor="e")
        self.content_shift = 0
        self.draw_footer()

    def draw_footer(self) -> None:
        self.content_shift = 0
        r = self.controller.robot
        y = self.virtual_height - 28
        self.rect(2, y, 1020, 26, c.HEADER)
        event = self.controller.event
        if self.controller.connection == "Desconectado":
            event = "Conecte para monitorar o robô."
        elif self.controller.connection == "Conectando":
            event = "Conectando…"
        elif self.controller.connection == "Erro de conexão":
            event = "Não foi possível conectar. Verifique a porta e o baud."
        elif self.controller.connection == "Comunicação perdida":
            event = "Sem telemetria. Reconecte para acompanhar o robô."
        elif r.state == State.CALIBRATION:
            event = "Calibrando sensores…"
        self.text(14, y + 13, event[:92], 11, c.MUTED)
        self.text(
            1010,
            y + 13,
            "Parada remota não substitui parada física.",
            10,
            c.MUTED,
            anchor="e",
        )

    def configure_pwm(self) -> None:
        from tkinter.simpledialog import askinteger

        self.controller.focus_lost()
        self.manual_modal = True
        try:
            value = askinteger(
                "Limite PWM",
                "PWM manual (0–160), máximo embarcado 160:",
                initialvalue=self.controller.pwm,
                minvalue=0,
                maxvalue=MAX_MANUAL_PWM,
                parent=self,
            )
        finally:
            self.manual_modal = False
        if value is not None:
            self.controller.pwm = value

    def update_ui(self) -> None:
        self.controller.tick()
        running = self.controller.robot.state == State.RUNNING
        if running and not self.was_running:
            self.mode = "race"
        self.was_running = running
        self.connect_button.configure(
            text="Desconectar" if self.controller.transport else "Conectar"
        )
        self.identity_label.configure(
            text="PEGASUS" if self.controller.connected else ""
        )
        if self.controller.robot.simulated:
            self.sim_menu.pack(side="right")
        else:
            self.sim_menu.pack_forget()
        self.draw()
        self.after(50, self.update_ui)

    def shutdown(self) -> None:
        self.controller.disconnect()
        self.destroy()
