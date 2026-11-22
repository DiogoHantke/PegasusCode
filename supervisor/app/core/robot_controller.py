"""Máquina de conexão e comandos, executada exclusivamente na thread da UI."""

from collections.abc import Callable
from queue import Empty
import logging
import time
from ..communication.protocol import StreamParser, Frame, ProtocolError, encode
from ..communication.serial_transport import SerialTransport, Transport
from .robot_state import RobotState, State, STATE_LABELS
from .pid import PIDSettings, ControlHistory, line_deviation
from .config import (
    ACK_TIMEOUT_SECONDS,
    DEFAULT_BAUD,
    DEFAULT_MANUAL_PWM,
    HEARTBEAT_SECONDS,
    LINK_TIMEOUT_SECONDS,
    MANUAL_PERIOD_SECONDS,
)

KEY_COMMANDS = {
    "w": "MANUAL_FORWARD",
    "s": "MANUAL_BACKWARD",
    "a": "MANUAL_LEFT",
    "d": "MANUAL_RIGHT",
}


def resolve_keys(keys: set[str]) -> str:
    # Multiple directions are deliberately neutral; no implicit diagonal/reversal.
    return KEY_COMMANDS[next(iter(keys))] if len(keys) == 1 else "MANUAL_STOP"


class RobotController:
    def __init__(self, clock: Callable[[], float] = time.monotonic) -> None:
        self.history = ControlHistory()
        self.pid_queried = False
        self.clock = clock
        self.robot = RobotState()
        self.transport: Transport | None = None
        self.connection = "Desconectado"
        self.port = ""
        self.baud = DEFAULT_BAUD
        self.event = "Pronto · aguardando conexão"
        self.failures = 0
        self.parser = StreamParser()
        self.sequence = 0
        self.pending: dict[int, tuple[str, float]] = {}
        self.last_rx = 0.0
        self.last_ping = -100.0
        self.last_manual = -100.0
        self.started = 0.0
        self.ping_confirmed = False
        self.status_received = False
        self.keys: set[str] = set()
        self.pwm = DEFAULT_MANUAL_PWM
        self.last_command = "Parado"

    @property
    def connected(self) -> bool:
        return self.connection == "Conectado"

    @property
    def can_move(self) -> bool:
        return (
            self.connected
            and self.robot.manual
            and not self.robot.estop
            and self.robot.state == State.WAITING
        )

    def connect(self, port: str, baud: int, transport: Transport | None = None) -> None:
        self.disconnect()
        self.port = port
        self.baud = baud
        self.robot = RobotState(simulated=port == "SIMULAÇÃO")
        self.parser = StreamParser()
        self.history.clear()
        self.pid_queried = False
        self.pending.clear()
        self.ping_confirmed = False
        self.status_received = False
        self.transport = transport or SerialTransport(port, baud)
        self.connection = "Conectando"
        self.started = self.clock()
        self.last_rx = self.started
        self.last_ping = -100
        self.last_manual = -100
        self.transport.start()
        self.event = "Aguardando identificação e resposta válida do firmware"

    def disconnect(self) -> None:
        self.keys.clear()
        if self.transport:
            if self.robot.manual:
                try:
                    self.transport.send(
                        encode(self._next_sequence(), "MANUAL_DISABLE"), priority=True
                    )
                except (OSError, ValueError):
                    pass
            # Closing the monitor never issues STOP to an autonomous task.
            # Manual motion remains protected by the firmware's 300 ms lease.
            self.transport.close()
        self.transport = None
        self.pending.clear()
        self.connection = "Desconectado"
        self.robot.manual = False
        self.last_command = "—"

    def _next_sequence(self) -> int:
        for _ in range(256):
            self.sequence = (self.sequence + 1) & 255
            if self.sequence not in self.pending:
                return self.sequence
        raise OSError("Sem sequências disponíveis")

    def apply_pid(self, settings: PIDSettings) -> bool:
        if not self.connected:
            self.event = "Conecte o robô para aplicar os ganhos"
            return False
        if (
            self.robot.estop
            or self.robot.manual
            or self.robot.state
            not in (State.INIT, State.WAITING, State.STOP, State.FINISH)
        ):
            self.event = "Pare o robô e encerre o manual para aplicar os ganhos"
            return False
        return self.command("SET_PID", *settings.values)

    def command(self, kind: str, *fields: object) -> bool:
        if kind == "START" and self.connected and not self.robot.race_supported:
            self.event = "Corrida pendente de implementação no firmware"
            return False
        if not self.transport or (
            not self.connected and kind not in ("PING", "GET_STATUS")
        ):
            self.event = "Comando bloqueado: sem comunicação confirmada"
            return False
        if (
            kind.startswith("MANUAL_")
            and kind not in ("MANUAL_ENABLE", "MANUAL_DISABLE")
            and not self.can_move
        ):
            self.event = "Controle manual bloqueado"
            return False
        if kind == "START" and (
            self.robot.estop
            or self.robot.manual
            or not self.robot.calibrated
            or self.robot.state not in (State.WAITING, State.STOP)
        ):
            self.event = "Partida bloqueada: calibre, encerre o modo manual e rearme"
            return False
        if kind in ("STOP", "ESTOP", "MANUAL_DISABLE", "CALIBRATE", "START", "REARM"):
            self.keys.clear()
        seq = self._next_sequence()
        try:
            if kind in ("ESTOP", "STOP", "MANUAL_STOP", "MANUAL_DISABLE"):
                self.pending.clear()
            self.transport.send(
                encode(seq, kind, *fields),
                priority=kind in ("ESTOP", "STOP", "MANUAL_STOP", "MANUAL_DISABLE"),
            )
        except OSError as error:
            self._lost(str(error))
            return False
        logging.debug("TX seq=%s comando=%s campos=%s", seq, kind, fields)
        self.pending[seq] = (kind, self.clock())
        if kind not in ("PING", "GET_STATUS", "GET_SENSORS", "GET_PID"):
            self.last_command = kind
            self.event = "Aguardando confirmação…"
        return True

    def set_key(self, key: str, pressed: bool) -> None:
        key = key.lower()
        if key not in KEY_COMMANDS:
            return
        if pressed and self.can_move:
            self.keys.add(key)
        else:
            self.keys.discard(key)

    def focus_lost(self) -> None:
        self.keys.clear()
        if self.can_move:
            self.command("MANUAL_STOP")

    def _lost(self, reason: str) -> None:
        connecting = self.connection == "Conectando"
        self.disconnect()
        self.failures += 1
        self.connection = "Erro de conexão" if connecting else "Comunicação perdida"
        self.event = reason
        logging.warning("%s: %s", self.connection, reason)

    def _receive(self, frame: Frame) -> bool:
        logging.debug(
            "RX seq=%s tipo=%s campos=%s", frame.sequence, frame.kind, frame.fields
        )
        if frame.kind in ("ACK", "NACK"):
            if (
                len(frame.fields) != 1
                or not frame.fields[0].isdigit()
                or not 0 <= int(frame.fields[0]) <= 6
            ):
                raise ProtocolError("ACK/NACK inválido")
            pending = self.pending.pop(frame.sequence, None)
            if not pending:
                return False
            kind, _ = pending
            if frame.kind == "ACK" and frame.fields == ("0",):
                if kind == "PING":
                    self.ping_confirmed = True
                if kind == "SET_PID":
                    self.command("GET_PID")
                if kind not in ("PING", "GET_STATUS", "GET_SENSORS", "GET_PID"):
                    self.event = "Comando confirmado"
            else:
                logging.info("%s recusado: NACK %s", kind, frame.fields[0])
                self.event = (
                    "Função ainda não implementada no firmware"
                    if frame.fields == ("6",)
                    else "Comando indisponível no estado atual"
                )
            return True
        previous_state, previous_fault = self.robot.state, self.robot.fault
        accepted = self.robot.update(frame)
        if (
            frame.kind == "T"
            and accepted
            and previous_state != State.RUNNING
            and self.robot.state == State.RUNNING
        ):
            self.history.clear()
        if frame.kind == "S" and accepted and self.robot.state == State.RUNNING:
            self.history.add(self.robot.elapsed_ms, line_deviation(self.robot))
        if frame.kind == "G" and accepted:
            self.event = "Ganhos PID recebidos"
        if frame.kind == "T" and accepted:
            self.status_received = True
            if self.robot.estop:
                self.event = "Emergência acionada. Rearme necessário."
            elif (
                previous_state == State.CALIBRATION
                and self.robot.state == State.WAITING
                and self.robot.calibrated
            ):
                self.event = "Calibração concluída com sucesso"
            elif previous_state == State.CALIBRATION and self.robot.state == State.STOP:
                self.event = "Calibração cancelada · motores comandados em zero"
            elif self.robot.fault and self.robot.fault != previous_fault:
                reasons = {
                    1: "Erro ou overflow UART",
                    2: "Timeout manual",
                    3: "Perda do enlace",
                    4: "Falha de sensores ou controle",
                    5: "Linha perdida",
                }
                self.event = reasons[self.robot.fault]
            elif previous_state != self.robot.state:
                self.event = STATE_LABELS[self.robot.state]
            if not self.can_move:
                self.keys.clear()
        return accepted

    def tick(self) -> None:
        if not self.transport:
            return
        now = self.clock()
        if hasattr(self.transport, "poll"):
            self.transport.poll()
        try:
            for _ in range(128):
                packet = self.transport.incoming.get_nowait()
                if isinstance(packet, Exception):
                    self._lost(str(packet))
                    return
                errors = self.parser.errors
                for frame in self.parser.feed(packet):
                    try:
                        if self._receive(frame):
                            self.last_rx = now
                    except ProtocolError as error:
                        self.failures += 1
                        self.event = str(error)
                self.failures += self.parser.errors - errors
        except Empty:
            pass
        if self.robot.identity and self.ping_confirmed and self.status_received:
            if self.connection == "Conectando":
                self.event = "Pronto"
            self.connection = "Conectado"
        if self.connected and not self.pid_queried:
            self.command("GET_PID")
            self.pid_queried = True
        if now - self.last_rx > LINK_TIMEOUT_SECONDS or (
            self.connection == "Conectando" and now - self.started > 3
        ):
            self._lost("Timeout: firmware sem resposta válida")
            return
        expired = [
            kind
            for kind, sent in self.pending.values()
            if now - sent > ACK_TIMEOUT_SECONDS
        ]
        if expired:
            self._lost(
                f"Sem confirmação de {expired[0]}; estado incerto, movimento bloqueado"
            )
            return
        if now - self.last_ping >= HEARTBEAT_SECONDS:
            self.command("PING")
            if not self.status_received:
                self.command("GET_STATUS")
            self.last_ping = now
        if self.can_move and now - self.last_manual >= MANUAL_PERIOD_SECONDS:
            kind = resolve_keys(self.keys)
            self.command(kind, *((self.pwm,) if kind != "MANUAL_STOP" else ()))
            self.last_manual = now
