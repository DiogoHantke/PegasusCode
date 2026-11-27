#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
test_binary=$(mktemp /tmp/pegasus-controls-test.XXXXXX)
trap 'rm -f "$test_binary"' EXIT HUP INT TERM
for test_name in button buzzer status_monitor; do
    "${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -Itests/host -Iinclude -Ilib/timer -Ilib/adc \
        "tests/host/${test_name}_test.c" "src/${test_name}.c" -o "$test_binary"
    "$test_binary"
done
