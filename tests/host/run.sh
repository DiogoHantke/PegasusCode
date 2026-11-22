#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
test_binary=$(mktemp /tmp/pegasus-calibration-test.XXXXXX)
trap 'rm -f "$test_binary"' EXIT HUP INT TERM
for polarity in 0 1; do
    "${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -DCAL_WHITE_HIGH="$polarity" -Itests/host -Iinclude -Ilib/adc -Ilib/pwm -Ilib/timer tests/host/calibration_test.c src/calibration/*.c -o "$test_binary"
    for scenario in approved front lateral front-recovery lateral-recovery both-recovery noise contrast-failure front-timeout no-adc no-front-reference no-lateral-reference delayed-recovery all-lateral-recovery repeat-success repeat-fault initial-white initial-black initial-no-adc initial-lateral-no-adc; do
        "$test_binary" "$scenario"
    done
done
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -DPEGASUS_SUPERVISOR -Itests/host -Iinclude -Ilib/adc -Ilib/pwm -Ilib/timer tests/host/main_test.c src/communication/system_init.c -o "$test_binary"
for scenario in short long bounce wrap-short wrap-long retry-success retry-fault long-retry busy manual estop emergency boot-held; do
    "$test_binary" "$scenario"
done
