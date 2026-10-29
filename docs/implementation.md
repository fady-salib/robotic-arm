# Implementation notes

## Sensor path

Flex calibration observes min/max during a sweep. A span below 30 ADC counts
returns zero. Other readings are linearly scaled/clamped to 0–1000, then filtered
with `filtered += 0.35 * (value - filtered)` at the send cadence. The first valid
sample primes the filter directly. Per-channel inversion occurs after filtering;
invalid calibration stays zero even for inverted channels. Min/max is sensitive
to outliers and cannot identify reversed wiring or every disconnected sensor.

EMG baseline is the relaxed sample mean. Runtime processing is
`envelope += 0.04 * (abs(raw - baseline) - envelope)`. Calibration records the
maximum envelope; a learned peak below 25 counts is unusable. Otherwise the output
is `clamp(1000 * envelope / peak, 0, 1000)`. This assumes a conditioned analog
front end and does not replace amplification, band-limiting or anti-aliasing.
The sample cadence is cooperative, so the effective filter time constant varies
with actual sampling intervals. Calibration is not persisted to EEPROM.

ECG uses adaptive high/low trackers, 60% rise / 40% fall thresholds, a 300 ms
refractory period and a four-value instantaneous-BPM average. The accepted
30–220 BPM check is further constrained by the refractory period to about 200
BPM. Lead-off resets immediately; more than 3 seconds without a beat clears the
reference/history. BPM zero means unavailable, not a normal pulse. Synthetic
tests do not establish performance on real ECG or EMG motion artifacts.

## Wire format

The existing 16-byte packed structure is retained for compatibility. Multi-byte
integers are little-endian on the intended AVR targets; this is not a portable
network-order serialization API.

| Byte offset | Content |
|---|---|
| 0 | Magic `0xA5` |
| 1 | Sequence modulo 256 |
| 2–11 | Five unsigned 16-bit flex values, 0–1000 |
| 12–13 | Unsigned 16-bit EMG, 0–1000 |
| 14 | BPM, zero unavailable |
| 15 | XOR of bytes 0–14 |

Receiver checks frame length, magic, checksum and sensor ranges before applying
commands. While live, sequence deltas 1–127 are newer; delta zero and 128–255
are rejected without refreshing the timeout. After more than 500 ms without
acceptance, any valid sequence starts a new epoch. Transmitter restart recovers
after timeout; no timestamps mean old/replayed datagrams can also be accepted.
XOR does not provide authentication or robust multi-bit integrity.

## ESP-01 transport

One hardware UART at 9600 baud serves each module. No Serial debug output or
SoftwareSerial is used. Setup is blocking and retries Wi-Fi indefinitely; the
receiver has already initialized its rest commands at this point.

Transmit transaction: issue CIPSEND only with available UART buffer space, wait
for `>`, write the copied payload, then wait for `SEND OK`. No next command is
started until completion. Prompt/ACK timeout after 1000 ms, `ERROR`, `FAIL`, or
`busy` latches a fault. Both MCU and module must be restarted to recover; blind
retry could send an AT command as payload to a module still expecting bytes.
The receiver then times out. No automatic reconnect/watchdog is claimed.

Receive parser supports `+IPD,<0..4>,<0..1500>:` with no remote metadata. Payloads
of other sizes are fully consumed/discarded; only complete expected-size frames
are published. Partial frames expire after a 100 ms gap when the next byte arrives.
Static payload storage is bounded to 32 bytes. Malformed text is rejected; this
is not protection against an attacker controlling a weak-checksummed network.

## Kinematics and servo commands

FK uses `x=l1*cos(t1)+l2*cos(t1+t2)` and the corresponding sine expression for y.
IK uses the cosine law, clamps its cosine for floating-point roundoff, and
projects unreachable targets onto `abs(l1-l2) <= r <= l1+l2`. Zero-radius unequal
links project along +x; equal links allow the folded singularity. Nonfinite or
nonpositive inputs return false and zero angles. False also indicates projection;
callers requiring separate reasons would need a future status API.

Default links are 0.25 m / 0.09 m. EMG chooses y from −0.15 to +0.15 m at fixed
x=0.20 m. Both servo angles use `clamp(90 + degrees(theta), 0, 180)`.
This assumes unmeasured offsets and directions. Along this path the wrist
relative angle is approximately 100–133°, so **every wrist command saturates to
180°**. Finger commands map 0–1000 to 10–170°. These are software commands,
not verified physical tracking or safe mounting limits.

Initialization protects attach/write from timer interrupts so the library cannot
emit its default pulse between those calls. It cannot prevent movement from an
unknown physical starting position. Runtime updates occur no faster than every
10 ms; each update advances by at most 2.4° (1.2° at high reported BPM). A long
stall does not accumulate a larger step. Integer degree output introduces
quantization; this limits commands, not measured velocity/acceleration or torque.
Communication loss sets targets to the configured rest pose. The previous slow
speed scale is retained during timeout; missing ECG does not disable motion.
