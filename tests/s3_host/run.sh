#!/bin/sh
# Builds and runs the S3 logic tests with strict warnings and memory checks.
set -e
cd "$(dirname "$0")"
FW=../../zone_monitor
OUT="${TMPDIR:-/tmp}/s3_host_test"
clang -std=gnu17 -g -O1 -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion -Wno-unused-const-variable -Werror \
  -fsanitize=address,undefined -fno-sanitize-recover=undefined \
  -Istubs -I"$FW" \
  s3_test.c "$FW/motor.c" "$FW/buzzer.c" "$FW/espnow_link.c" "$FW/estop.c" "$FW/qr_clear.c" "$FW/zones.c" \
  -o "$OUT"
"$OUT"
