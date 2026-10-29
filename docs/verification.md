# Verification record — 2026-10-29

This record covers local compilation and simulated host checks only. No MCU,
ESP-01, sensors, human electrodes, or motors were connected. No physical motion,
latency, sensor accuracy, clinical accuracy, or safety validation was performed.

## Baseline before edits

Revision `7a1f176d32068209958d9117e5a530044744002e` had no tests or CI.
GCC C11 syntax-only checks passed for all four original pure-C source files
with `-Wall -Wextra -Wpedantic -Werror`. Using example credentials, Arduino CLI
1.3.1, AVR core 1.8.6 and Servo 1.2.2:

| Original build | Flash bytes | Static SRAM bytes | Result |
|---|---:|---:|---|
| Nano transmitter | 6,864 | 400 | Pass |
| Nano receiver | 7,570 | 402 | Pass |
| Uno transmitter, ECG enabled | 6,864 | 400 | Compiles, but physical A6 is unavailable |

Compilation alone was insufficient evidence of Uno hardware compatibility.

## Reviewed software

Commands run:

```bash
ASAN_OPTIONS=detect_leaks=0 make test
ARDUINO_CLI=/workspace/tooling/arduino-cli bash tools/build_firmware.sh --config-file /workspace/tooling/arduino-cli.yaml
bash -n tools/build_firmware.sh
git diff --check
```

The absolute tooling/config paths are local to the review environment; normal
use is `make test` and `bash tools/build_firmware.sh`. Tool versions are pinned
in the README/workflow. Builds use staged example credentials; real local secrets
are not read by the matrix script.

**Host:** four executables passed, with the sender executed for prompt-timeout,
ACK-timeout, and explicit-error cases. C and C++ warnings are errors. AddressSanitizer
and UndefinedBehaviorSanitizer were enabled. The first run failed because
LeakSanitizer cannot operate under this runner's ptrace restrictions; rerunning
with `detect_leaks=0` passed. Leak detection is explicitly **not** claimed.

| Executable | Evidence |
|---|---|
| `test_logic` | All 128 single-bit packet corruptions rejected; normalized ranges and modulo ordering; flex smoothing/calibration; failed EMG calibration; synthetic 60 BPM, timeout/refractory/rollover; 1,681 FK/IK round trips across both branches; invalid/boundary IK; fragmented/malformed/wrong-length/truncated IPD recovery |
| `test_receiver` | Real sketch with mock UART/Servo: startup attach/write interrupt protection, packet at timestamp zero, stale/duplicate rejection, sequence rollover/new epoch, invalid ranges, 500 ms timeout and millis rollover, bounded step after long stall; confirms default wrist saturation |
| `test_transport` | Real ESP driver: buffer backpressure, fragmented SEND OK, no overlapping sends, prompt timeout, ACK timeout, ERROR latch, late prompt cannot restart a faulted send |
| `test_transmitter` | Real sketch: calibrated packet construction, lead-off BPM reset, no send while prior transaction remains busy |

**AVR:** six configurations passed, plus the expected compile failure for ECG
on Uno, checked for the specific analog-input diagnostic.

| Target / sketch | Flash bytes | Static SRAM bytes | SRAM remaining* |
|---|---:|---:|---:|
| Nano ATmega328 / transmitter | 7,328 | 439 | 1,609 |
| Nano ATmega328 / receiver | 8,360 | 442 | 1,606 |
| Nano old bootloader / transmitter | 7,328 | 439 | 1,609 |
| Nano old bootloader / receiver | 8,360 | 442 | 1,606 |
| Uno / transmitter, ECG disabled | 6,218 | 439 | 1,609 |
| Uno / receiver | 8,360 | 442 | 1,606 |

*Linker figures leave space for stack/local variables; no runtime stack high-water
measurement was made. Bootloader-specific upload capacities are 30,720 bytes in
these Nano build definitions and 32,256 bytes for Uno. Different credentials,
core/library/compiler versions or configuration can change resource figures.

Clean AVR builds emitted four unused-parameter warnings in the upstream Arduino
core's `new.cpp`; no project-source warnings were observed. Cached rebuilds do
not repeat core warnings. Shell syntax and whitespace checks passed. Documentation
links were checked locally. Added CI definitions have not been treated as passing
remote runs merely because the local commands pass.

## Physical verification still required

Resolve Uno/Nano history and actual motors/link/sensors; identify AT firmware
and test reset/reconnect behavior; verify ADC signals/conditioning, calibration
polarity, sampling jitter and ECG artifacts; measure servo pulse limits, mounting
signs/offsets, rest pose and actual geometry; test startup, link loss and power
cutoff unloaded on an isolated platform. The default wrist mapping saturates and
must not be presented as working Cartesian tracking.
