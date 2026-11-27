"""Aba PID fiel à grade 4/8 do HTML, com histórico real de sensores."""

import tkinter as tk
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from .main_window import MainWindow
from ..core.pid import PIDSettings
from ..core.robot_state import State
from . import theme as c


class PIDPanel:
    def __init__(self, window: "MainWindow") -> None:
        self.window = window
        self.variables = [tk.StringVar(value=v) for v in PIDSettings().formatted()]
        self.inputs = []
        self.items = []
        self.dirty = False
        self.last_gains = None
        self.requested = None
        for i, variable in enumerate(self.variables):
            widget = tk.Spinbox(
                window.canvas,
                from_=0,
                to=10 if i == 0 else 5,
                increment=(0.05, 0.01, 0.02)[i],
                format="%.2f",
                textvariable=variable,
                bg="#F7F3F2",
                fg=c.TEXT,
                buttonbackground=c.FRAME,
                relief="solid",
                bd=1,
                highlightthickness=0,
                font=(window.mono_font, -12),
            )
            self.inputs.append(widget)
            self.items.append(
                window.canvas.create_window(
                    0, 0, window=widget, anchor="nw", state="hidden"
                )
            )
            variable.trace_add("write", self._edited)

    def _edited(self, *args: str) -> None:
        self.dirty = True

    def hide(self) -> None:
        for item in self.items:
            self.window.canvas.itemconfigure(item, state="hidden")

    def restore(self) -> None:
        for variable, value in zip(self.variables, PIDSettings().formatted()):
            variable.set(value)
        self.window.controller.event = "Padrão restaurado. Aplique para enviar."

    def apply(self) -> None:
        try:
            settings = PIDSettings.parse(tuple(v.get() for v in self.variables))
        except ValueError as error:
            self.window.controller.event = str(error)
            return
        if self.window.controller.apply_pid(settings):
            self.requested = settings.values

    def draw(self) -> None:
        w = self.window
        r = w.controller.robot
        actual = r.pid_gains
        if actual != self.last_gains:
            if actual is not None and (not self.dirty or actual == self.requested):
                for variable, value in zip(
                    self.variables, PIDSettings(*actual).formatted()
                ):
                    variable.set(value)
                self.dirty = False
                self.requested = None
            self.last_gains = actual
        w.rect(18, 139, 319, 444, c.PANEL)
        w.rect(353, 139, 653, 444, c.PANEL)
        w.text(35, 165, "Ganhos do Controlador PID", 15, bold=True)
        w.rect(35, 184, 285, 1, c.BORDER, c.BORDER)
        names = ("Kp (Proporcional)", "Ki (Integral)", "Kd (Derivativo)")
        for i, (name, item) in enumerate(zip(names, self.items)):
            y = 211 + i * 86
            w.rect(35, y, 285, 74, c.WHITE, c.BORDER)
            w.text(48, y + 20, name, 13, bold=True)
            w.text(
                307,
                y + 20,
                "—" if actual is None else f"{actual[i]/100:.2f}",
                12,
                mono=True,
                anchor="e",
            )
            w.canvas.coords(item, 48 * w.sx, (y + 35) * w.sy)
            w.canvas.itemconfigure(
                item, state="normal", width=259 * w.sx, height=26 * w.sy
            )
            w.canvas.tag_raise(item)
        allowed = (
            w.controller.connected
            and not r.manual
            and not r.estop
            and r.state in (State.INIT, State.WAITING, State.STOP, State.FINISH)
        )
        w.button(
            35,
            487,
            285,
            36,
            "✓  Aplicar Ganhos PID",
            "pid-apply",
            self.apply,
            c.SUCCESS if allowed else c.FRAME,
            c.WHITE if allowed else c.MUTED,
        )
        w.button(35, 531, 285, 36, "Restaurar Padrão", "pid-reset", self.restore)
        w.text(370, 165, "Diagnóstico de Controle — Desvio × Tempo", 15, bold=True)
        w.text(
            370,
            185,
            "Posição estimada pelos seis sensores em relação ao centro",
            11,
            c.MUTED,
        )
        label = "Sem dados"
        if not w.controller.connected:
            label = "Sem conexão"
        elif r.state == State.RUNNING:
            label = "Em corrida"
        elif w.controller.history.samples:
            label = "Parado"
        w.badge(
            900,
            153,
            89,
            label,
            (
                "#DCFCE7"
                if r.state == State.RUNNING and w.controller.connected
                else c.FRAME
            ),
        )
        w.rect(370, 200, 619, 1, c.BORDER, c.BORDER)
        w.rect(370, 213, 619, 292, c.WHITE)
        w.text(382, 234, "Desvio (%)", 11, c.MUTED)
        self.line([740, 234, 756, 234], c.SUCCESS, 2)
        w.text(761, 234, "Sensores", 11)
        self.line([859, 234, 875, 234], c.ERROR, 2)
        w.text(880, 234, "Referência (0)", 11)
        x0, x1, y0, y1 = 427, 975, 268, 472
        for i, value in enumerate((100, 50, 0, -50, -100)):
            y = y0 + i * (y1 - y0) / 4
            self.line(
                [x0, y, x1, y],
                c.ERROR if value == 0 else c.BORDER,
                1,
                dash=(6, 4) if value == 0 else None,
            )
            w.text(
                x0 - 8,
                y,
                f"{value:+d}%" if value else "0",
                11,
                c.ERROR if value == 0 else c.MUTED,
                mono=True,
                anchor="e",
            )
        self.line([x0, y0, x0, y1, x1, y1], c.OUTLINE, 1)
        history = w.controller.history.window()
        end = max(10000, history[-1][0] if history else 10000)
        start = end - 10000
        for i in range(5):
            w.text(
                x0 + (x1 - x0) * i / 4,
                487,
                f"{(start+2500*i)/1000:g}s",
                11,
                c.MUTED,
                mono=True,
                anchor="center",
            )
        segment = []
        for milliseconds, value in history:
            if value is None:
                if len(segment) >= 4:
                    self.line(segment, c.SUCCESS, 2)
                segment = []
                continue
            segment.extend(
                (
                    x0 + (milliseconds - start) / 10000 * (x1 - x0),
                    (y0 + y1) / 2 - value / 100 * (y1 - y0) / 2,
                )
            )
        if len(segment) >= 4:
            self.line(segment, c.SUCCESS, 2)
        if not history:
            w.text(
                (x0 + x1) / 2,
                (y0 + y1) / 2 + 27,
                "Aguardando dados da corrida",
                12,
                c.MUTED,
                anchor="center",
            )
        mean, peak, in_band = w.controller.history.metrics()
        values = (
            "—" if mean is None else f"{mean:.1f}%",
            "—" if peak is None else f"{peak:.1f}%",
            "—" if in_band is None else f"{in_band:.1f} s",
        )
        for i, (title, value) in enumerate(
            zip(("Erro Médio", "Desvio Máximo", "Tempo em Faixa"), values)
        ):
            x = 370 + i * 210
            w.rect(x, 519, 199, 49, c.WHITE, c.BORDER)
            w.text(x + 12, 534, title, 11, c.MUTED)
            w.text(x + 12, 552, value, 12, mono=True, bold=True)

    def line(self, coordinates: list[float], color: str, width: int, dash=None) -> None:
        w = self.window
        options = {"fill": color, "width": width, "tags": "drawing"}
        if dash:
            options["dash"] = dash
        w.canvas.create_line(
            *[v * (w.sx if i % 2 == 0 else w.sy) for i, v in enumerate(coordinates)],
            **options,
        )
