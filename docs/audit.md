# Technical audit — 2026-10-29

Baseline: `7a1f176d32068209958d9117e5a530044744002e` on `master`.
All 19 tracked files were read before modification. There were no tests,
dependency lock/configuration, host build scripts, or GitHub workflows. Existing
pure C modules passed GCC C11 syntax checks with warnings as errors. Both
sketches compiled with AVR core 1.8.6 / Servo 1.2.2. No hardware was connected.

## Evidence and uncertainty

**Confirmed source:** two sketches, shared C/C++ library, five flex inputs,
EMG envelope processing, AD8232-style ECG/lead-off handling, ESP-01 AT commands,
UDP, seven hobby-servo outputs, planar FK/IK, speed limiting and timeout-to-rest.
No Python application, machine learning, joint feedback, Step/Dir drivers,
mechanical CAD, 3D DH solver, or pick-and-place implementation exists.

**Owner-reported:** a September–December 2024 two-Uno prototype using EMG for
the arm and a flex glove for fingers. Physical motor types, communication and
the presence of ECG remain unconfirmed. Source uploaded in October 2026 cannot
establish historical hardware functionality or feature authorship dates.

The Uno's core headers define `A6` even though its board does not expose it.
Baseline Uno compilation therefore succeeds but does not validate wiring. The
classic Nano has exposed ADC6/7; it supports all seven current analog signals.
This review adds an analog-count guard and explicit optional ECG disable for Uno,
without moving pins or pretending to reproduce the original prototype.

## File-by-file baseline findings

| Original file | Role and finding | Treatment |
|---|---|---|
| `firmware/transmitter/transmitter.ino` | Sensor sampling, two-phase calibration, packet send; A6 ambiguity, no flex polarity option, approximate timing | ECG build guard/disable option, polarity configuration, aligned active-calibration delay, scheduler initialization |
| `firmware/receiver/receiver.ino` | Seven servos, EMG target to IK, timeout; sequence ignored, timestamp-zero sentinel, first dt includes setup, saturated wrist mapping | Sequence/session checks, explicit receive flag, bounded control step, atomic attach/rest initialization; retain/document unmeasured mapping |
| `firmware/transmitter/secrets.example.h` | Wi-Fi and broadcast destination template; no actual credential | Use a documentation IP and recommend unicast/isolated network |
| `firmware/receiver/secrets.example.h` | Wi-Fi credential template | Preserve |
| `libraries/ArmCommon/src/ArmProtocol.h` | Packed 16-byte binary struct; misleading sequence/drop comment; no explicit endian statement | Clarify checks/session ordering; document AVR little-endian format |
| `libraries/ArmCommon/src/ArmProtocol.c` | Magic/XOR only; unchecked normalized values | Add flex/EMG bounds and modulo sequence helper; retain wire compatibility |
| `libraries/ArmCommon/src/Calibration.h` | Flex/EMG channel state and thresholds | Preserve public structures/API |
| `libraries/ArmCommon/src/Calibration.c` | Min/max smoothing, rectified EMG; tiny calibration still amplifies noise via denominator floor | Suppress EMG output below minimum learned peak |
| `libraries/ArmCommon/src/Esp01.h` | Hardware UART interface; nonblocking claim depends on available serial buffer | Clarify timing and expose latched send fault |
| `libraries/ArmCommon/src/Esp01.cpp` | AT setup, send state, incremental IPD; idle immediately after payload, ignores SEND OK/errors; loose parser can remain stuck on partial frames | Await ACK, check serial backpressure, latch ambiguous faults, configure metadata format, extract strict tested parser |
| `libraries/ArmCommon/src/HeartMonitor.h` | ECG state/API; claims average intervals but stores BPM | Correct documentation to instantaneous-BPM average |
| `libraries/ArmCommon/src/HeartMonitor.c` | Adaptive threshold/hysteresis/refractory; stale beat reference survives timeout | Clear reference/ring index on timeout; synthetic tests, not clinical validation |
| `libraries/ArmCommon/src/Kinematics.h` | Planar FK/IK API | Document invalid-input behavior and missing mechanical limits |
| `libraries/ArmCommon/src/Kinematics.c` | Geometric solver; absolute 1 mm margin misclassifies exact boundaries and fails at tiny link scales; invalid inputs may propagate NaN | Validate lengths/finite inputs, use true annulus boundaries and `hypotf`, retain cosine clamp |
| `libraries/ArmCommon/library.properties` | Metadata declares every architecture | Restrict advertised architecture to AVR; bump review version |
| `docs/wiring.md` | Nano/ESP/ADC/servo wiring; no provenance distinction or measured hardware limits | Rewrite as source-derived wiring, add diagram, power and calibration constraints |
| `README.md` | Truncated architecture section; unsupported Python/build badges, 3D solver, feedback, stepper and planning claims | Replace with source-backed overview, reproducible commands and limitations |
| `.gitignore` | Ignores local secrets and some binaries | Add object/HEX exclusions; allow explicit test-only secrets fixture |
| `LICENSE` | MIT text with original 2023 notice and no named holder | Preserve; owner can clarify attribution without inventing dates |

## Prioritized defects and disposition

| Priority | Defect/risk | Disposition |
|---|---|---|
| P1 | Large setup/stall dt can cause a large commanded servo step | Fixed: start control clock after setup; fixed maximum step per update |
| P1 | Valid-checksum values above 1000 drive out-of-contract targets | Fixed: reject before control/freshness update |
| P1 | Duplicate/reordered datagrams can refresh freshness or reverse commands | Fixed within a live epoch; timestamp-free replay across timeout remains |
| P1 | Default wrist mapping saturates throughout target range; mechanical limits unknown | Confirmed and prominently documented; offset/sign changes deferred to measured hardware calibration |
| P1 | Startup/default pulses and unknown shaft position | Attach/rest write protected from ISR; immediate physical rest movement remains and is documented |
| P1 | CIPSEND can overlap its prior transaction and obscure errors | Fixed ACK sequencing/backpressure; ambiguous failure latches instead of blind retry |
| P2 | Partial/malformed IPD framing can block or misparse subsequent data | Strict bounded parser with 100 ms inter-byte timeout; malformed lengths rejected |
| P2 | Failed EMG calibration still scales small noise | Fixed: unusable learned peak returns zero |
| P2 | IK invalid lengths/nonfinite input and artificial boundary exclusion | Fixed finite/positive guards and exact annulus; mechanics still separate |
| P2 | Heart timeout retains stale timing/history | Fixed; no claim of ECG accuracy or medical safety |
| P2 | Flex min/max does not establish direction | Explicit per-channel inversion option; actual polarity still requires measurement |
| P2 | Uno can compile an inaccessible A6 read | Fixed explicit build guard and ECG-disabled variant |
| P3 | Unsupported README claims/no reproducible tests | Rewritten docs, host suite, pinned tool versions and CI |

## Sampling, resources and residual risks

ATmega328P provides 2 KiB SRAM and 32 KiB flash before bootloader allocation.
Static use fits comfortably in the tested builds, but remaining SRAM is not a
measured stack margin. Arduino's serial rings, seven Servo instances and testable
state are included in linker-reported globals; no application `String`, heap
allocation, or variable-size packet buffer is introduced.

Five four-read flex measurements cost roughly 2 ms at typical AVR ADC settings.
Together with UART and interrupt work this makes 4 ms sensor scheduling soft.
Missed periods are not replayed. Active calibration delay is now nominally 4 ms
plus acquisition overhead; no hard-real-time guarantee is made. Raw EMG bandwidth,
front-end model, ECG noise robustness, flex source impedance and mux settling
remain unverified. Software normalization cannot detect every disconnection.

The packet remains unauthenticated native AVR little-endian with a weak XOR.
Sequence ordering is half-range (1–127 forward) and resets after 500 ms without
acceptance; it cannot establish packet age, ownership, or clock synchronization.
No packet-loss statistics, acknowledgments over UDP, retransmission, Wi-Fi
reconnection, or safety-rated stop is implemented. The sender AT ACK only confirms
module transmission, not receiver delivery. Bench-test module/MCU recovery together.

See [verification](verification.md), [implementation](implementation.md) and
[media evidence requirements](media.md). Hardware-affecting calibration must be
measured on an isolated unloaded platform before claims of physical operation.
