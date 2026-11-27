#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
test_binary=$(mktemp /tmp/pegasus-running-test.XXXXXX)
trap 'rm -f "$test_binary"' EXIT HUP INT TERM
for polarity in 0 1; do
    for test_name in line_follower ino_reference; do
        "${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -DCAL_WHITE_HIGH="$polarity" \
            -Itests/host -Iinclude -Ilib/adc -Ilib/pwm -Ilib/timer \
            "tests/host/${test_name}_test.c" src/line_follower.c -o "$test_binary"
        "$test_binary"
    done
    "${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -DCAL_WHITE_HIGH="$polarity" \
        -Itests/host -Iinclude -Ilib/adc -Ilib/pwm -Ilib/timer \
        tests/host/running_main_test.c src/line_follower.c -o "$test_binary"
    "$test_binary" button
    "$test_binary" loss
done
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -Itests/host -Ilib/adc \
    tests/host/adc_test.c lib/adc/adc_lib.c -o "$test_binary"
"$test_binary"
