"""Toda E/S serial ocorre na worker. Nenhuma chamada Tk fora da main thread."""

from queue import Queue, Empty, Full
from threading import Event, Thread
from typing import Protocol
import glob
import logging
import serial
from serial.tools import list_ports


def available_ports() -> list[str]:
    return sorted(
        set([p.device for p in list_ports.comports()] + glob.glob("/dev/rfcomm*"))
    )


class Transport(Protocol):
    incoming: Queue[bytes | Exception]

    def start(self) -> None: ...
    def send(self, data: bytes, priority: bool = False) -> None: ...
    def close(self) -> None: ...


class SerialTransport:
    def __init__(self, port: str, baud: int) -> None:
        self.port, self.baud = port, baud
        self.incoming: Queue[bytes | Exception] = Queue(maxsize=128)
        self.outgoing: Queue[bytes] = Queue(maxsize=32)
        self.stop = Event()
        self.thread = Thread(target=self._run, daemon=True, name="Pegasus serial")

    def start(self) -> None:
        self.thread.start()

    def send(self, data: bytes, priority: bool = False) -> None:
        if priority:
            while True:
                try:
                    self.outgoing.get_nowait()
                except Empty:
                    break
        try:
            self.outgoing.put_nowait(data)
        except Full:
            raise OSError("Fila de transmissão congestionada")

    def _run(self) -> None:
        try:
            with serial.Serial(
                self.port, self.baud, timeout=0.02, write_timeout=0.15, exclusive=True
            ) as connection:
                connection.reset_input_buffer()
                while not self.stop.is_set():
                    try:
                        data = self.outgoing.get_nowait()
                    except Empty:
                        data = None
                    if data is not None:
                        if connection.write(data) != len(data):
                            raise OSError("Escrita serial incompleta")
                    data = connection.read(min(256, connection.in_waiting or 1))
                    if data:
                        self.incoming.put_nowait(data)
        except Exception as error:
            logging.exception("Falha no transporte serial")
            # Make room for the error, so overflow also causes a safe disconnect.
            try:
                self.incoming.put_nowait(error)
            except Full:
                self.incoming.get_nowait()
                self.incoming.put_nowait(error)

    def close(self) -> None:
        self.stop.set()
        if self.thread.is_alive():
            self.thread.join(timeout=0.4)
