"""Dados validados e diagnósticos; sem widgets ou medições inventadas."""

from dataclasses import dataclass, field
from enum import IntEnum
from ..communication.protocol import Frame, ProtocolError


class State(IntEnum):
    INIT = 0
    CALIBRATION = 1
    WAITING = 2
    RUNNING = 3
    FINISH = 4
    STOP = 5
    EMERGENCY = 6


STATE_LABELS = (
    "Pronto",
    "Calibrando",
    "Pronto",
    "Em Corrida",
    "Finalizado",
    "Parado",
    "Falha de calibração",
)


def integers(frame: Frame, count: int) -> list[int]:
    if len(frame.fields) != count:
        raise ProtocolError("Quantidade de campos inválida")
    try:
        return [int(x) for x in frame.fields]
    except ValueError as error:
        raise ProtocolError("Campo numérico inválido") from error


def bounded(value: int, low: int, high: int) -> int:
    if not low <= value <= high:
        raise ProtocolError("Valor de telemetria fora da faixa")
    return value


@dataclass
class Sensor:
    value: int | None = None
    low: int = 255
    high: int = 0
    approved: bool = False
    received: bool = False

    def diagnostic(self, state: State, calibrated: bool) -> str:
        if self.value is None:
            return "SEM DADOS"
        if not self.received:
            return "INCONCL."
        if state == State.CALIBRATION:
            return "CALIBR."
        if (
            (calibrated or state == State.EMERGENCY)
            and self.approved
            and self.high - self.low >= 40
        ):
            return "OK"
        if state == State.EMERGENCY and not self.approved:
            return "FALHA"
        return "NÃO CAL."

    def whiteness(self, white_high: bool) -> float | None:
        if self.value is None or not self.approved or self.high <= self.low:
            return None
        normalized = max(
            0.0, min(1.0, (self.value - self.low) / (self.high - self.low))
        )
        return normalized if white_high else 1 - normalized


@dataclass
class RobotState:
    state: State = State.INIT
    manual: bool = False
    estop: bool = False
    calibrated: bool = False
    fault: int = 0
    left_pwm: int = 0
    right_pwm: int = 0
    elapsed_ms: int = 0
    calibration_phase: int = 0
    white_high: bool = False
    laps: int = 0
    last_lap: int | None = None
    best_lap: int | None = None
    sensors: list[Sensor] = field(default_factory=lambda: [Sensor() for _ in range(6)])
    identity: str = ""
    simulated: bool = False
    laps_supported: bool = False
    race_supported: bool = False

    pid_gains: tuple[int, int, int] | None = None

    def update(self, frame: Frame) -> bool:
        if frame.kind == "ID":
            if (
                len(frame.fields) not in (3, 4)
                or frame.fields[0] != "PEGASUS"
                or frame.fields[1] != "1"
                or frame.fields[2] not in ("0", "1")
            ):
                raise ProtocolError("Firmware incompatível")
            if len(frame.fields) == 4 and frame.fields[3] not in ("0", "1"):
                raise ProtocolError("Capacidade de corrida inválida")
            self.race_supported = (
                frame.fields[3] == "1" if len(frame.fields) == 4 else True
            )
            self.identity = "PEGASUS · protocolo 1"
            self.laps_supported = frame.fields[2] == "1"
        elif frame.kind == "T":
            s, flags, fault, l, r, elapsed, phase, polarity = integers(frame, 8)
            bounded(s, 0, 6)
            bounded(flags, 0, 7)
            bounded(fault, 0, 5)
            bounded(l, -255, 255)
            bounded(r, -255, 255)
            bounded(elapsed, 0, 0xFFFFFFFF)
            bounded(phase, 0, 6)
            bounded(polarity, 0, 1)
            if State(s) == State.RUNNING and self.state != State.RUNNING:
                self.laps = 0
                self.last_lap = None
                self.best_lap = None
            if State(s) == State.CALIBRATION and self.state != State.CALIBRATION:
                for sensor in self.sensors:
                    sensor.received = False
                    sensor.approved = False
            self.state = State(s)
            self.manual = bool(flags & 1)
            self.estop = bool(flags & 2)
            self.calibrated = bool(flags & 4)
            self.fault = fault
            self.left_pwm = l
            self.right_pwm = r
            self.elapsed_ms = elapsed
            self.calibration_phase = phase
            self.white_high = bool(polarity)
        elif frame.kind == "S":
            *values, mask = integers(frame, 7)
            for value in values:
                bounded(value, 0, 255)
            bounded(mask, 0, 255)
            for i, (sensor, value) in enumerate(zip(self.sensors, values), 1):
                sensor.value = value if mask & (1 << i) else None
        elif frame.kind == "D":
            index, low, high, ok = integers(frame, 4)
            bounded(index, 1, 6)
            bounded(low, 0, 255)
            bounded(high, 0, 255)
            bounded(ok, 0, 1)
            if ok and high - low < 40:
                raise ProtocolError("Contraste aprovado inconsistente")
            sensor = self.sensors[index - 1]
            sensor.low = low
            sensor.high = high
            sensor.approved = bool(ok)
            sensor.received = True
        elif frame.kind == "G":
            kp, ki, kd = integers(frame, 3)
            bounded(kp, 0, 1000)
            bounded(ki, 0, 500)
            bounded(kd, 0, 500)
            self.pid_gains = (kp, ki, kd)
        elif frame.kind == "LAP":
            if not self.laps_supported:
                raise ProtocolError("Voltas não suportadas")
            count, ms = integers(frame, 2)
            bounded(count, 1, 65535)
            bounded(ms, 1, 0xFFFFFFFF)
            if count <= self.laps:
                return False
            self.laps = count
            self.last_lap = ms
            self.best_lap = ms if self.best_lap is None else min(self.best_lap, ms)
        else:
            return False
        return True


def format_time(ms: int | None) -> str:
    if ms is None:
        return "--:--.---"
    return f"{ms//60000:02d}:{ms//1000%60:02d}.{ms%1000:03d}"
