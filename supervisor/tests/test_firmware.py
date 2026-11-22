import subprocess
from pathlib import Path
import pytest

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(scope="session")
def firmware_binary(tmp_path_factory):
    output = tmp_path_factory.mktemp("firmware") / "supervisor-test"
    command = [
        "cc",
        "-std=c11",
        "-O2",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-DF_CPU=16000000UL",
    ]
    command += [
        "-I" + str(ROOT / p)
        for p in ("tests/communication", "include", "lib/adc", "lib/pwm", "lib/timer")
    ]
    command += [
        str(ROOT / "tests/communication/supervisor_test.c"),
        str(ROOT / "src/communication/protocol.c"),
        "-o",
        str(output),
    ]
    subprocess.run(command, check=True, capture_output=True, text=True)
    return output


@pytest.mark.parametrize(
    "scenario",
    [
        "estop",
        "deadman",
        "wrap",
        "interlocks",
        "overflow",
        "link",
        "autonomous-uart",
        "manual-heartbeat",
        "calibration",
        "duplicates",
        "local",
        "pid",
        "congestion",
    ],
)
def test_actual_firmware_command_engine(firmware_binary, scenario):
    subprocess.run([str(firmware_binary), scenario], check=True)


def test_actual_avr_encoder_matches_python(firmware_binary):
    from app.communication.protocol import decode
    from app.core.robot_state import RobotState

    result = subprocess.run(
        [str(firmware_binary), "wire"], check=True, capture_output=True
    )
    frames = [decode(line + b"\n") for line in result.stdout.splitlines()]
    assert {f.kind for f in frames} >= {"ID", "ACK", "T", "S", "G"}
    state = RobotState()
    for frame in frames:
        if frame.kind != "ACK":
            state.update(frame)
    assert not state.race_supported
    assert state.pid_gains == (1000, 500, 500)
    assert [s.value for s in state.sensors] == [0, 128, 255, 42, 84, 252]


def test_emergency_led_pattern(tmp_path):
    output = tmp_path / "emergency-led"
    command = ["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror"]
    command += [
        "-I" + str(ROOT / p)
        for p in ("tests/host", "include", "lib/adc", "lib/pwm", "lib/timer")
    ]
    command += [str(ROOT / "tests/host/emergency_led_test.c"), "-o", str(output)]
    subprocess.run(command, check=True)
    subprocess.run([str(output)], check=True)
