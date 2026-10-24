# Wiring (two Arduino Nanos + two ESP-01 modules)

## ESP-01 Wi-Fi module (same on both Nanos)

| ESP-01 | Connect to |
|---|---|
| VCC, CH_PD (EN) | 3.3 V from a **separate regulator** (e.g. AMS1117-3.3, 300 mA+). The Nano's 3.3 V pin cannot power it. |
| GND | Common ground |
| TX | Nano RX (D0) |
| RX | Nano TX (D1) **through a voltage divider** (1 kΩ in series, 2 kΩ to GND). The Nano is 5 V, the ESP-01 is 3.3 V only. |

- The module must run **standard AT firmware** at **9600 baud**. To set it once: `AT+UART_DEF=9600,8,1,0,0` (or `AT+IPR=9600` on older firmware).
- D0/D1 are shared with USB. **Unplug the ESP-01 TX/RX wires while uploading**, then plug them back in.

## Transmitter Nano (worn)

| Part | Pin |
|---|---|
| Flex sensors: thumb, index, middle, ring, pinky | A0, A1, A2, A3, A4 |
| EMG muscle sensor (signal) | A5 |
| AD8232 OUTPUT | A6 |
| AD8232 LO+ / LO- | D2 / D3 |
| AD8232 3.3V / GND | Nano 3.3V / GND |
| Status LED | D13 (onboard) |

Each flex sensor is a divider: 5 V -> flex sensor -> analog pin -> 10 kΩ -> GND.
Place the AD8232 electrodes on the chest/forearm (red, yellow, green pads per the module's guide).

## Receiver Nano (on the arm)

| Servo | Pin |
|---|---|
| Thumb, index, middle, ring, pinky | D2, D3, D4, D5, D6 |
| Elbow | D7 |
| Wrist | D8 |

Power the 7 servos from a separate 5-6 V supply (several amps), never from the Nano. Join all grounds together.
