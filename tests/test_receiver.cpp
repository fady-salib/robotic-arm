#include <cassert>
#include <cmath>
#include <iostream>
// Compile and exercise the real sketch against a simulated serial/servo boundary.
#include "../firmware/receiver/receiver.ino"

static void send_packet(ArmPacket p, uint32_t now)
{
    arm_packet_seal(&p);
    Serial.inject("+IPD,0,16:");
    Serial.inject(std::string(reinterpret_cast<char *>(&p), sizeof(p)));
    fake_ms = now;
    loop();
}

int main()
{
    initialize_servos();
    for (uint8_t i=0; i<NUM_SERVOS; ++i) {
        assert(servos[i].attach_protected && servos[i].initial_write_protected);
        assert(servos[i].angle == static_cast<int>(REST_POSE[i]));
    }
    ArmPacket p = {};
    p.seq = 255; p.flex[0] = 1000;
    send_packet(p, 0);
    assert(have_rx && digital_values[LED_PIN]); // timestamp zero is valid
    assert(target_deg[0] == 170);
    assert(target_deg[IDX_WRIST] == 180); // known mechanical mapping limitation
    fake_ms = 10000; loop();
    assert(target_deg[0] == REST_POSE[0]);
    assert(std::fabs(current_deg[IDX_WRIST] - REST_POSE[IDX_WRIST]) <= 2.401f);
    send_packet(p, 10010); // new epoch after timeout
    assert(std::fabs(current_deg[0] - 10.0f) <= 2.401f);
    p.flex[0] = 0; send_packet(p, 10020); // duplicate must not apply or refresh
    assert(target_deg[0] == 170 && last_rx_ms == 10010);
    p.seq = 254; send_packet(p, 10030);
    assert(target_deg[0] == 170 && last_rx_ms == 10010);
    p.seq = 0; p.bpm = 150; send_packet(p, 10040);
    assert(target_deg[0] == 10 && speed_scale_percent == 50);
    p.seq = 1; p.flex[0] = 1001; send_packet(p, 10050);
    assert(last_rx_ms == 10040);
    fake_ms = 10541; loop(); assert(!digital_values[LED_PIN]);
    assert(target_deg[IDX_WRIST] == REST_POSE[IDX_WRIST]);
    p.flex[0] = 1000; p.seq = 5;
    send_packet(p, UINT32_MAX - 100);
    fake_ms = 20; loop(); assert(digital_values[LED_PIN]);
    fake_ms = 500; loop(); assert(!digital_values[LED_PIN]);
    std::cout << "PASS receiver sketch: timeout, bounded stall step, sequence, ranges, rollover\n";
}
