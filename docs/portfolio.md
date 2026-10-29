# Portfolio wording — draft pending hardware provenance

## Academic CV (compact)

**EMG and flex-sensor robotic arm — Mechatronics project, Sep–Dec 2024.**
Built a wearable-input robotic arm prototype using muscle-signal input for arm
control and a flex-sensor glove for finger control (original two-Uno configuration
reported by the owner). The maintained C/C++ repository documents calibration,
packet transport, planar kinematics and command limiting, with host regression
tests and AVR compile checks. Current Nano/ESP-01/ECG software features remain
separate from the original prototype pending hardware verification.

For a two-page CV, the first sentence is the historical project description;
include the maintenance/testing sentence only when discussing the current
repository and your ability to explain the assisted changes. Do not attribute
later IK/ECG/Wi-Fi features to 2024 without original evidence. No accuracy or
hardware-test metric is available.

## LinkedIn project description

Developed an EMG-input robotic arm with a flex-sensor glove for finger control
(Sep–Dec 2024). The repository now documents the original prototype separately
from the current Arduino C/C++ firmware, including calibration, communication,
planar kinematics and host-tested control logic. Current software revisions
require physical validation and mechanical calibration.

Suggested skills: Embedded C/C++, Arduino, sensor interfacing, robotics,
calibration, UART, debugging. Add UDP/ESP8266, inverse kinematics and software
testing as current repository skills you can explain; do not imply every feature
was physically demonstrated. Do not list machine learning for this project.

## Contribution record

Owner-provided history: Fady Salib's 2024 prototype and intended functionality.
Source evidence: firmware present at `7a1f176` (uploaded October 2026).
This review: AI-assisted audit, defensive fixes, host tests, reproducible build
checks, and documentation. No new physical build or experimental results.
Confirm individual/team responsibilities before adding a claim of sole authorship.
