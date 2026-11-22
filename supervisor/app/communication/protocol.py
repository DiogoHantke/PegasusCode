"""Protocolo PEGASUS v1. Sem dependência de serial ou Tk."""

from dataclasses import dataclass
import re

MAX_FRAME = 47
TOKEN = re.compile(r"[A-Z][A-Z_0-9]*\Z")


class ProtocolError(ValueError):
    pass


def crc8(data: bytes) -> int:
    crc = 0
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = ((crc << 1) ^ (7 if crc & 128 else 0)) & 255
    return crc


@dataclass(frozen=True)
class Frame:
    sequence: int
    kind: str
    fields: tuple[str, ...] = ()


def encode(sequence: int, kind: str, *fields: object) -> bytes:
    if not 0 <= sequence <= 255 or not TOKEN.fullmatch(kind):
        raise ProtocolError("Sequência ou identificador inválido")
    values = tuple(str(x) for x in fields)
    if any(not re.fullmatch(r"[A-Za-z0-9_.-]+", x) for x in values):
        raise ProtocolError("Campo inválido")
    body = ",".join((str(sequence), kind, *values)).encode("ascii")
    frame = b"@" + body + f"*{crc8(body):02X}\n".encode()
    if len(frame) > MAX_FRAME:
        raise ProtocolError("Frame excede 47 bytes")
    return frame


def decode(data: bytes) -> Frame:
    if len(data) > MAX_FRAME or not re.fullmatch(
        rb"@[0-9]{1,3},[A-Z][A-Z_0-9]*(?:,[A-Za-z0-9_.-]+)*\*[0-9A-F]{2}\n", data
    ):
        raise ProtocolError("Formato inválido ou truncado")
    body = data[1:-4]
    if crc8(body) != int(data[-3:-1], 16):
        raise ProtocolError("CRC inválido")
    seq, kind, *fields = body.decode("ascii").split(",")
    if int(seq) > 255:
        raise ProtocolError("Sequência fora da faixa")
    return Frame(int(seq), kind, tuple(fields))


class StreamParser:
    """Ressincroniza em @; fragmentação arbitrária e memória limitada."""

    def __init__(self) -> None:
        self.buffer = bytearray()
        self.errors = 0

    def feed(self, data: bytes) -> list[Frame]:
        frames = []
        for byte in data:
            if byte == 64:
                self.buffer.clear()
            if not self.buffer and byte != 64:
                continue
            self.buffer.append(byte)
            if len(self.buffer) > MAX_FRAME:
                self.errors += 1
                self.buffer.clear()
            elif byte == 10:
                try:
                    frames.append(decode(bytes(self.buffer)))
                except ProtocolError:
                    self.errors += 1
                self.buffer.clear()
        return frames
