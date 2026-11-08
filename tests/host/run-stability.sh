#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
test_dir=$(mktemp -d /tmp/pegasus-stability.XXXXXX)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
for options in normal motors_off white_high real_flash; do
    flags=''
    flash=''
    case "$options" in
        motors_off) flags='-DMOTORS_KEY=0';;
        white_high) flags='-DCAL_WHITE_HIGH=1';;
        real_flash) flags='-DTEST_REAL_FLASH'; flash='lib/flash/flash_lib.c';;
    esac
    gcc -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=undefined $flags \
        -Itests/host -Iinclude -Ilib/calibration -Ilib/sensor -Ilib/adc -Ilib/pwm -Ilib/timer -Ilib/flash \
        tests/host/calibration_stability_test.c lib/calibration/calibration.c \
        lib/calibration/calibration_store.c lib/sensor/sensor_lib.c lib/adc/adc_lib.c \
        lib/pwm/pwm_lib.c $flash -o "$test_dir/test"
    "$test_dir/test"
done

gcc -std=c11 -O2 -Wall -Wextra -Werror -Itests/host -Ilib/timer \
    tests/host/timer_test.c lib/timer/timer_lib.c -o "$test_dir/timer"
"$test_dir/timer"
python3 tests/host/check_preservation.py
