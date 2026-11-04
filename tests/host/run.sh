#!/bin/sh
set -eu
cd "$(dirname "$0")/../.."
test_binary=$(mktemp /tmp/pegasus-test.XXXXXX)
trap 'rm -f "$test_binary"' EXIT HUP INT TERM
gcc -std=c11 -O2 -Wall -Wextra -Werror -Itests/host -Ilib/adc -Ilib/pwm \
    -Ilib/timer -Ilib/usart -Ilib/sensor tests/host/diagnostic_test.c \
    lib/adc/adc_lib.c lib/pwm/pwm_lib.c lib/usart/usart_lib.c lib/sensor/sensor_lib.c \
    -o "$test_binary"
"$test_binary" serial
"$test_binary" success
"$test_binary" timeout
