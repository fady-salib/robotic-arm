/*
 * Robotic Arm - Receiver (on the arm)
 *
 * Board : Arduino Nano
 * Link  : ESP-01 (AT firmware) on hardware Serial D0/D1
 *         (disconnect the ESP-01 TX/RX wires while uploading from USB)
 *
 * Drives 7 servos:
 *   5 fingers   <- flex sensors, direct mapping
 *   elbow+wrist <- EMG envelope sets a fingertip target; inverse kinematics
 *                  solves both joint angles
 *
 * Safety:
 *   - Failsafe: no valid packet for 500 ms -> arm eases to its rest pose.
 *   - Slew limiting: servos never jump, they move at a capped speed.
 *   - Heart rate above 140 bpm halves the speed limit.
 *
 * LED (D13): on while the link is alive, fast blink = Wi-Fi setup failed.
 */
#include <Arduino.h>
#include <Servo.h>
#include "ArmProtocol.h"
#include "Kinematics.h"
#include "Esp01.h"
#include "secrets.h"

// ---- Servo layout: 0-4 fingers (thumb..pinky), 5 elbow, 6 wrist ----
#define NUM_SERVOS 7
#define IDX_ELBOW  5
#define IDX_WRIST  6
static const uint8_t SERVO_PINS[NUM_SERVOS] = {2, 3, 4, 5, 6, 7, 8};
static const uint8_t LED_PIN = 13;

// ---- Tuning ----
static const float FINGER_OPEN_DEG   = 10.0f;
static const float FINGER_CLOSED_DEG = 170.0f;
static const float REST_POSE[NUM_SERVOS] = {10, 10, 10, 10, 10, 90, 90};

static const float L1_FOREARM_M = 0.25f;   // elbow -> wrist
static const float L2_HAND_M    = 0.09f;   // wrist -> fingertip
static const float TARGET_X_M   = 0.20f;   // fixed reach distance
static const float TARGET_Y_MIN = -0.15f;  // EMG 0    -> arm low
static const float TARGET_Y_MAX =  0.15f;  // EMG 1000 -> arm raised

static const uint32_t ESP_BAUD           = 9600;
static const float    MAX_SPEED_DPS      = 240.0f;
static const uint8_t  BPM_SLOW_LIMIT     = 140;
static const uint32_t FAILSAFE_MS        = 500;
static const uint32_t CONTROL_PERIOD_MS  = 10;

// ---- State (all static) ----
static Servo    servos[NUM_SERVOS];
static float    current_deg[NUM_SERVOS];
static float    target_deg[NUM_SERVOS];
static uint8_t  rx_buf[sizeof(ArmPacket)];
static uint8_t  speed_scale_percent = 100;
static uint32_t last_rx_ms = 0;
static uint32_t last_ctrl_ms = 0;

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static void apply_packet(const ArmPacket *p)
{
    // Fingers: 0..1000 -> open..closed
    for (uint8_t i = 0; i < ARM_NUM_FINGERS; i++) {
        float t = (float)p->flex[i] / 1000.0f;
        target_deg[i] = FINGER_OPEN_DEG + t * (FINGER_CLOSED_DEG - FINGER_OPEN_DEG);
    }

    // Elbow + wrist: EMG picks fingertip height, IK solves the joint angles
    float y = TARGET_Y_MIN + ((float)p->emg / 1000.0f) * (TARGET_Y_MAX - TARGET_Y_MIN);
    float t1, t2;
    kin_ik_2link(L1_FOREARM_M, L2_HAND_M, TARGET_X_M, y, true, &t1, &t2);
    target_deg[IDX_ELBOW] = clampf(90.0f + kin_rad_to_deg(t1), 0.0f, 180.0f);
    target_deg[IDX_WRIST] = clampf(90.0f + kin_rad_to_deg(t2), 0.0f, 180.0f);

    // Heart-rate safety: slow down if the wearer's pulse is very high
    speed_scale_percent = (p->bpm > BPM_SLOW_LIMIT) ? 50 : 100;
}

static void step_servos(float dt_s)
{
    float max_step = MAX_SPEED_DPS * (speed_scale_percent / 100.0f) * dt_s;
    for (uint8_t i = 0; i < NUM_SERVOS; i++) {
        float diff = target_deg[i] - current_deg[i];
        current_deg[i] += clampf(diff, -max_step, max_step);
        servos[i].write((int)(current_deg[i] + 0.5f));
    }
}

void setup(void)
{
    pinMode(LED_PIN, OUTPUT);

    for (uint8_t i = 0; i < NUM_SERVOS; i++) {
        current_deg[i] = REST_POSE[i];
        target_deg[i]  = REST_POSE[i];
        servos[i].attach(SERVO_PINS[i], 500, 2400);
        servos[i].write((int)REST_POSE[i]);
    }

    esp_begin(ESP_BAUD);
    while (!(esp_join(WIFI_SSID, WIFI_PASS) && esp_open_udp_rx(ARM_UDP_PORT))) {
        for (uint8_t i = 0; i < 20; i++) {
            digitalWrite(LED_PIN, i & 1);
            delay(80);
        }
    }
}

void loop(void)
{
    // ---- Receive ----
    if (esp_poll(rx_buf, sizeof(rx_buf))) {
        ArmPacket p;
        memcpy(&p, rx_buf, sizeof(p));
        if (arm_packet_valid(&p)) {
            apply_packet(&p);
            last_rx_ms = millis();
        }
    }

    // ---- Failsafe ----
    bool link_ok = (last_rx_ms != 0) && (millis() - last_rx_ms <= FAILSAFE_MS);
    digitalWrite(LED_PIN, link_ok);
    if (!link_ok) {
        for (uint8_t i = 0; i < NUM_SERVOS; i++) target_deg[i] = REST_POSE[i];
    }

    // ---- Fixed-rate control step ----
    uint32_t now = millis();
    if (now - last_ctrl_ms >= CONTROL_PERIOD_MS) {
        step_servos((now - last_ctrl_ms) / 1000.0f);
        last_ctrl_ms = now;
    }
}
