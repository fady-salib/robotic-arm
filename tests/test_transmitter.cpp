#include <cassert>
#include <iostream>
#include "../firmware/transmitter/transmitter.ino"

int main()
{
    for (unsigned i=0; i<ARM_NUM_FINGERS; ++i) {
        flex_init(&flex[i], 1);
        flex_learn(&flex[i], 100); flex_learn(&flex[i], 900);
        analog_values[FLEX_PINS[i]] = 500;
    }
    emg_init(&emg, 1); emg_set_baseline(&emg, 500);
    emg.peak = 100;
    analog_values[EMG_PIN] = 600;
    hr_reset(&heart); heart.bpm = 180;
    digital_values[LO_PLUS] = 1;
    fake_ms = 50; loop();
    assert(packet.flex[0] == 500 && packet.emg == 1000 && packet.bpm == 0);
    assert(arm_packet_valid(&packet));
    fake_ms = 100; loop(); // sender is still awaiting prompt
    assert(packet.seq == 0);
    Serial.inject(">"); loop();
    Serial.inject("SEND OK\r\n"); loop();
    assert(packet.seq == 1);
    std::cout << "PASS transmitter sketch: scaling, lead-off reset, send backpressure\n";
}
