#include "ArmProtocol.h"
#include "Calibration.h"
#include "HeartMonitor.h"
#include "Kinematics.h"
#include "EspIpd.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void protocol(void)
{
    assert(sizeof(ArmPacket) == 16);
    ArmPacket p = {0};
    p.flex[0] = 1000;
    p.emg = 1000;
    arm_packet_seal(&p);
    assert(arm_packet_valid(&p));
    uint8_t *bytes = (uint8_t *)&p;
    assert(bytes[2] == 0xe8 && bytes[3] == 0x03); /* AVR little-endian */
    for (unsigned i = 0; i < sizeof(p); ++i) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            bytes[i] ^= (uint8_t)(1U << bit);
            assert(!arm_packet_valid(&p));
            bytes[i] ^= (uint8_t)(1U << bit);
        }
    }
    p.flex[4] = 1001; arm_packet_seal(&p); assert(!arm_packet_valid(&p));
    p.flex[4] = 0; p.emg = 65535; arm_packet_seal(&p);
    assert(!arm_packet_valid(&p));
    assert(!arm_packet_valid(NULL));
    for (unsigned i = 0; i < 256; ++i) {
        assert(!arm_sequence_newer((uint8_t)i, (uint8_t)i));
        assert(arm_sequence_newer((uint8_t)(i + 1), (uint8_t)i));
        assert(!arm_sequence_newer((uint8_t)(i - 1), (uint8_t)i));
        assert(!arm_sequence_newer((uint8_t)(i + 128), (uint8_t)i));
    }
}

static void calibration(void)
{
    FlexChannel c;
    flex_init(&c, 0.5f);
    assert(flex_scale(&c, 500) == 0);
    flex_learn(&c, 400); flex_learn(&c, 420);
    assert(flex_scale(&c, 410) == 0);
    flex_learn(&c, 800);
    assert(flex_scale(&c, 600) == 500);
    assert(flex_scale(&c, 1023) == 750);
    assert(flex_scale(&c, 0) == 375);
    EmgChannel e;
    emg_init(&e, 1.0f); emg_set_baseline(&e, 500);
    emg_feed(&e, 510); emg_learn_peak(&e);
    assert(emg_output(&e) == 0); /* inadequate calibration stays inactive */
    emg_feed(&e, 600); emg_learn_peak(&e);
    assert(emg_output(&e) == 1000);
    emg_feed(&e, 450); assert(emg_output(&e) == 500);
    emg_feed(&e, 1023); assert(emg_output(&e) == 1000);
    emg_feed(&e, 500); assert(emg_output(&e) == 0);
}

static void heart_rate(void)
{
    HeartMonitor h;
    hr_reset(&h);
    for (uint32_t t = 0; t < 5000; t += 4) {
        hr_feed(&h, t % 1000 < 20 ? 800 : 200, t);
    }
    assert(hr_bpm(&h) == 60);
    /* A gap invalidates history; first fresh beat only establishes a reference. */
    hr_feed(&h, 200, 9000); assert(hr_bpm(&h) == 0);
    hr_feed(&h, 800, 9004); assert(hr_bpm(&h) == 0);
    hr_feed(&h, 200, 9500); hr_feed(&h, 800, 10004);
    assert(hr_bpm(&h) == 60);
    hr_reset(&h); assert(hr_bpm(&h) == 0);
    /* Unsigned subtraction must preserve intervals across millis rollover. */
    hr_feed(&h, 200, UINT32_MAX - 1200);
    hr_feed(&h, 800, UINT32_MAX - 1000);
    hr_feed(&h, 200, UINT32_MAX - 500);
    hr_feed(&h, 800, 0);
    assert(hr_bpm(&h) == 59); /* 1001 ms, integer BPM */
    hr_feed(&h, 200, 20); hr_feed(&h, 800, 100);
    assert(hr_bpm(&h) == 59); /* refractory rejection */
}

static void kinematics(void)
{
    float t1, t2, x, y;
    for (int i = -20; i <= 20; ++i) {
        for (int j = -20; j <= 20; ++j) {
            float a = i * 0.1f, b = j * 0.1f;
            kin_fk_2link(0.25f, 0.09f, a, b, &x, &y);
            bool valid = kin_ik_2link(0.25f, 0.09f, x, y, j >= 0, &t1, &t2);
            (void)valid; /* boundary roundoff may trigger projection */
            float xx, yy;
            kin_fk_2link(0.25f, 0.09f, t1, t2, &xx, &yy);
            assert(fabsf(xx - x) < 1e-5f && fabsf(yy - y) < 1e-5f);
        }
    }
    assert(!kin_ik_2link(0.25f, 0.09f, 1, 0, true, &t1, &t2));
    kin_fk_2link(0.25f, 0.09f, t1, t2, &x, &y);
    assert(fabsf(x - 0.34f) < 1e-5f);
    assert(!kin_ik_2link(0.25f, 0.09f, 0, 0, true, &t1, &t2));
    kin_fk_2link(0.25f, 0.09f, t1, t2, &x, &y);
    assert(fabsf(x - 0.16f) < 1e-5f);
    assert(kin_ik_2link(1, 1, 0, 0, true, &t1, &t2));
    assert(isfinite(t1) && isfinite(t2));
    assert(!kin_ik_2link(0, 1, 0, 0, true, &t1, &t2));
    assert(t1 == 0 && t2 == 0);
    assert(!kin_ik_2link(1, 1, NAN, 0, true, &t1, &t2));
    assert(!kin_ik_2link(INFINITY, 1, 0, 0, true, &t1, &t2));
    assert(!kin_ik_2link(1, 1, 0, 0, true, NULL, &t2));
}

static bool feed_text(EspIpdParser *p, const char *s, uint32_t now, uint8_t *out)
{
    bool complete = false;
    for (; *s; ++s) complete |= esp_ipd_feed(p, (uint8_t)*s, now, out, 3);
    return complete;
}

static void parser(void)
{
    EspIpdParser p = {0};
    uint8_t out[3] = {0};
    assert(!feed_text(&p, "noise+IPD,0,3:a", 0, out));
    assert(out[0] == 0); /* incomplete packets cannot publish partial data */
    assert(feed_text(&p, "bc", 4, out));
    assert(memcmp(out, "abc", 3) == 0);
    assert(!feed_text(&p, "+IPD,0,4:abcd", 8, out));
    assert(!feed_text(&p, "+IPD,0,1501:", 12, out));
    assert(!feed_text(&p, "+IPD,0,x:", 16, out));
    assert(!feed_text(&p, "+IPD,9,3:abc", 20, out));
    assert(!feed_text(&p, "+IPD,0,3:a", 24, out));
    assert(feed_text(&p, "+IPD,0,3:def", 125, out));
    assert(memcmp(out, "def", 3) == 0);
    assert(!feed_text(&p, "+IP", 130, out));
    assert(!feed_text(&p, "D,0,3:bad", 240, out));
    assert(feed_text(&p, "+IPD,4,3:ghi", 244, out));
    assert(memcmp(out, "ghi", 3) == 0);
}

int main(void)
{
    protocol(); puts("PASS protocol: corruption, ranges, sequence wrap");
    calibration(); puts("PASS calibration: span, smoothing, saturation, no signal");
    heart_rate(); puts("PASS heart rate: synthetic pulse, timeout, refractory, wrap");
    kinematics(); puts("PASS kinematics: 1681 round trips, boundaries, invalid input");
    parser(); puts("PASS IPD parser: fragmentation, malformed input, recovery");
    return 0;
}
