#!/usr/bin/env bash
# Reproducible compile-only checks. Always stage placeholder credentials so a
# local secrets.h cannot be embedded into CI artifacts or measured binaries.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
arduino_cmd=("${ARDUINO_CLI:-arduino-cli}" "$@")
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
for sketch in transmitter receiver; do
    mkdir -p "$stage/$sketch"
    cp "$root/firmware/$sketch/$sketch.ino" "$stage/$sketch/"
    cp "$root/firmware/$sketch/secrets.example.h" "$stage/$sketch/secrets.h"
done
for cpu in atmega328 atmega328old; do
    for sketch in transmitter receiver; do
        "${arduino_cmd[@]}" compile --fqbn "arduino:avr:nano:cpu=$cpu" \
            --libraries "$root/libraries" --warnings all \
            --build-path "$root/build/avr/nano-$cpu-$sketch" "$stage/$sketch"
    done
done
"${arduino_cmd[@]}" compile --fqbn arduino:avr:uno --libraries "$root/libraries" \
    --warnings all --build-property compiler.cpp.extra_flags=-DARM_ENABLE_ECG=0 \
    --build-path "$root/build/avr/uno-transmitter" "$stage/transmitter"
"${arduino_cmd[@]}" compile --fqbn arduino:avr:uno --libraries "$root/libraries" \
    --warnings all --build-path "$root/build/avr/uno-receiver" "$stage/receiver"
if "${arduino_cmd[@]}" compile --fqbn arduino:avr:uno --libraries "$root/libraries" \
    --build-path "$root/build/avr/uno-ecg-guard" "$stage/transmitter" \
    > "$stage/guard.log" 2>&1; then
    echo 'FAIL: Uno with ECG should be rejected' >&2
    exit 1
fi
if [[ "$(<"$stage/guard.log")" != *'ECG on A6 requires an exposed seventh ADC input'* ]]; then
    cat "$stage/guard.log" >&2
    exit 1
fi
echo 'PASS: six AVR builds and expected Uno ECG rejection'
