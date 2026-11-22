"""Ganhos em ponto fixo e histórico de desvio normalizado, sem ângulos fictícios."""

from collections import deque
from dataclasses import dataclass
from decimal import Decimal, DecimalException
from .robot_state import RobotState


@dataclass(frozen=True)
class PIDSettings:
    kp: int = 145
    ki: int = 8
    kd: int = 32

    def __post_init__(self) -> None:
        if any(
            type(v) is not int or not 0 <= v <= limit
            for v, limit in zip(self.values, (1000, 500, 500))
        ):
            raise ValueError("Use Kp entre 0 e 10; Ki e Kd entre 0 e 5.")

    @property
    def values(self) -> tuple[int, int, int]:
        return self.kp, self.ki, self.kd

    @classmethod
    def parse(cls, texts: tuple[str, ...]) -> "PIDSettings":
        if len(texts) != 3:
            raise ValueError("Informe os três ganhos.")
        try:
            raw = [Decimal(t.strip().replace(",", ".")) for t in texts]
            if any(
                not n.is_finite() or not 0 <= n <= limit
                for n, limit in zip(raw, (10, 5, 5))
            ):
                raise ValueError("Use Kp entre 0 e 10; Ki e Kd entre 0 e 5.")
            numbers = [n * 100 for n in raw]
            if any(not n.is_finite() or n != n.to_integral_value() for n in numbers):
                raise ValueError("Use até duas casas decimais.")
            return cls(*(int(n) for n in numbers))
        except (DecimalException, OverflowError) as error:
            raise ValueError("Informe ganhos numéricos válidos.") from error

    def formatted(self) -> tuple[str, ...]:
        return tuple(f"{v/100:.2f}" for v in self.values)


def line_deviation(robot: RobotState) -> float | None:
    if not robot.calibrated:
        return None
    whites = [s.whiteness(robot.white_high) for s in robot.sensors]
    if any(v is None for v in whites):
        return None
    blacks = [1 - float(v) for v in whites]
    if sum(blacks) < 100 / 255:
        return None
    return sum(b * w for b, w in zip(blacks, (-5, -3, -1, 1, 3, 5))) / sum(blacks) * 20


class ControlHistory:
    def __init__(self) -> None:
        self.samples: deque[tuple[int, float | None]] = deque(maxlen=256)

    def clear(self) -> None:
        self.samples.clear()

    def add(self, milliseconds: int, value: float | None) -> None:
        if self.samples and milliseconds <= self.samples[-1][0]:
            return
        self.samples.append((milliseconds, value))

    def window(self) -> list[tuple[int, float | None]]:
        if not self.samples:
            return []
        threshold = max(0, self.samples[-1][0] - 10000)
        return [(t, v) for t, v in self.samples if t >= threshold]

    def metrics(self) -> tuple[float | None, float | None, float | None]:
        samples = self.window()
        values = [abs(v) for _, v in samples if v is not None]
        if not values:
            return None, None, None
        in_band = None
        for t, value in reversed(samples):
            if value is None or abs(value) > 5:
                break
            in_band = t
        duration = None if in_band is None else (samples[-1][0] - in_band) / 1000
        return sum(values) / len(values), max(values), duration
