import ctypes
import random
import subprocess
from pathlib import Path
import pytest
from app.communication.protocol import (
    Frame,
    ProtocolError,
    StreamParser,
    crc8,
    decode,
    encode,
    MAX_FRAME,
)

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(scope="session")
def c_protocol(tmp_path_factory):
    output = tmp_path_factory.mktemp("cprotocol") / "protocol.so"
    subprocess.run(
        [
            "cc",
            "-shared",
            "-fPIC",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-I" + str(ROOT / "include"),
            str(ROOT / "src/communication/protocol.c"),
            "-o",
            str(output),
        ],
        check=True,
    )
    lib = ctypes.CDLL(str(output))
    lib.protocol_decode.argtypes = [
        ctypes.c_char_p,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.POINTER(ctypes.c_char_p),
    ]
    lib.protocol_decode.restype = ctypes.c_uint8
    return lib


def c_decode(lib, data):
    buf = ctypes.create_string_buffer(data)
    seq = ctypes.c_uint8()
    payload = ctypes.c_char_p()
    valid = lib.protocol_decode(buf, ctypes.byref(seq), ctypes.byref(payload))
    return valid, seq.value, payload.value


def test_known_crc_and_parity(c_protocol):
    assert crc8(b"123456789") == 0xF4
    rng = random.Random(921)
    for _ in range(500):
        seq = rng.randrange(256)
        value = rng.randrange(161)
        data = encode(seq, "MANUAL_FORWARD", value)
        assert decode(data) == Frame(seq, "MANUAL_FORWARD", (str(value),))
        ok, c_seq, payload = c_decode(c_protocol, data)
        assert ok and c_seq == seq and payload == f"MANUAL_FORWARD,{value}".encode()


@pytest.mark.parametrize(
    "data",
    [
        b"",
        b"@1,PING*00\n",
        b"@1,PING",
        b"@1,PING*00\r\n",
        b"@a,PING*FF\n",
        b"@" + b"x" * 180 + b"\n",
        b"garbage",
        b"@1,\xff*00\n",
    ],
)
def test_reject_corruption_and_truncation(data, c_protocol):
    with pytest.raises(ProtocolError):
        decode(data)
    assert not c_decode(c_protocol, data)[0]


@pytest.mark.parametrize("seq", [-1, 256, 300])
def test_sequence_range(seq):
    with pytest.raises(ProtocolError):
        encode(seq, "PING")


def test_fragmentation_recovery_and_memory():
    parser = StreamParser()
    frame = encode(255, "START")
    assert parser.feed(frame[:5]) == []
    assert parser.feed(frame[5:]) == [Frame(255, "START")]
    result = parser.feed(
        b"junk@broken"
        + encode(2, "STOP")
        + b"@"
        + b"x" * 9000
        + b"\n"
        + encode(3, "ESTOP")
    )
    assert result == [Frame(2, "STOP"), Frame(3, "ESTOP")]
    assert len(parser.buffer) <= MAX_FRAME and parser.errors
