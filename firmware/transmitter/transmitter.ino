/*
 * Robotic Arm - Transmitter (worn on the user's arm)
 *
 * Board : Arduino Nano (ATmega328P, 2 KB RAM, 10-bit ADC)
 * Inputs: 5 flex sensors, 1 EMG muscle sensor, AD8232 ECG heart monitor
 * Link  : ESP-01 (AT firmware) on hardware Serial D0/D1
 *
 * Calibrates every sensor, then sends up to 20 UDP packets per second
 * (limited by the 9600 baud link to the Wi-Fi module).
 * All memory is statically allocated: no malloc, no String.
 *
 * LED (D13): slow blink = relax your arm, solid = open/close hand and flex
 * muscle, fast blink forever = Wi-Fi setup failed.
 */
#include <Arduino.h>
#include "ArmProtocol.h"
#include "Calibration.h"
#include "HeartMonitor.h"
#include "Esp01.h"
#include "secrets.h"

// ---- Pin map ----
static const uint8_t FLEX_PINS[ARM_NUM_FINGERS] = {A0, A1, A2, A3, A4}; // thumb..pinky
static const uint8_t EMG_PIN  = A5;
static const uint8_t ECG_PIN  = A6;   // AD8232 OUTPUT (A6/A7 are analog-only)
static const uint8_t LO_PLUS  = 2;    // AD8232 LO+  (electrode off detect)
static const uint8_t LO_MINUS = 3;    // AD8232 LO-
static const uint8_t LED_PIN  = 13;

// ---- Timing ----
static const uint32_t ESP_BAUD         = 9600;
static const uint32_t SAMPLE_PERIOD_MS = 4;     // 250 Hz for EMG + ECG
static const uint32_t SEND_PERIOD_MS   = 50;    // up to 20 Hz (9600 baud limit)
static const uint16_t REST_CAL_MS      = 2000;
static const uint16_t ACTIVE_CAL_MS    = 4000;
static const uint8_t  ADC_OVERSAMPLE   = 4;

// ---- State (all static) ----
static FlexChannel  flex[ARM_NUM_FINGERS];
static EmgChannel   emg;
static HeartMonitor heart;
static ArmPacket    packet;
static uint8_t      seq = 0;
static uint32_t     last_sample_ms = 0;
static uint32_t     last_send_ms = 0;

static uint16_t read_adc(uint8_t pin)
{
    uint16_t sum = 0;
    for (uint8_t i = 0; i < ADC_OVERSAMPLE; i++) sum += analogRead(pin);
    return sum / ADC_OVERSAMPLE;
}

static void calibrate(void)
{
    // Phase 1: relaxed arm -> EMG resting baseline
    uint32_t start = millis();
    float sum = 0.0f;
    uint16_t n = 0;
    while (millis() - start < REST_CAL_MS) {
        digitalWrite(LED_PIN, (millis() / 400) & 1);
        sum += read_adc(EMG_PIN);
        n++;
        delay(5);
    }
    emg_set_baseline(&emg, sum / (float)n);

    // Phase 2: move through the full range -> flex min/max + EMG peak
    digitalWrite(LED_PIN, HIGH);
    start = millis();
    while (millis() - start < ACTIVE_CAL_MS) {
        for (uint8_t i = 0; i < ARM_NUM_FINGERS; i++) {
            flex_learn(&flex[i], read_adc(FLEX_PINS[i]));
        }
        emg_feed(&emg, read_adc(EMG_PIN));
        emg_learn_peak(&emg);
        delay(5);
    }
    digitalWrite(LED_PIN, LOW);
}

void setup(void)
{
    pinMode(LED_PIN, OUTPUT);
    pinMode(LO_PLUS, INPUT);
    pinMode(LO_MINUS, INPUT);

    for (uint8_t i = 0; i < ARM_NUM_FINGERS; i++) flex_init(&flex[i], 0.35f);
    emg_init(&emg, 0.04f);
    hr_reset(&heart);

    esp_begin(ESP_BAUD);
    while (!(esp_join(WIFI_SSID, WIFI_PASS) && esp_open_udp_tx(DEST_IP, ARM_UDP_PORT))) {
        for (uint8_t i = 0; i < 20; i++) {       // fast blink, then retry
            digitalWrite(LED_PIN, i & 1);
            delay(80);
        }
    }

    calibrate();
}

void loop(void)
{
    esp_tx_service();   // finishes any send in progress, discards replies
    uint32_t now = millis();

    // ---- 250 Hz: EMG envelope + ECG beat detection ----
    if (now - last_sample_ms >= SAMPLE_PERIOD_MS) {
        last_sample_ms = now;
        emg_feed(&emg, analogRead(EMG_PIN));
        if (digitalRead(LO_PLUS) || digitalRead(LO_MINUS)) {
            hr_reset(&heart);                      // electrodes not attached
        } else {
            hr_feed(&heart, analogRead(ECG_PIN), now);
        }
    }

    // ---- 20 Hz: build and send packet ----
    if (now - last_send_ms >= SEND_PERIOD_MS && !esp_tx_busy()) {
        last_send_ms = now;
        for (uint8_t i = 0; i < ARM_NUM_FINGERS; i++) {
            packet.flex[i] = flex_scale(&flex[i], read_adc(FLEX_PINS[i]));
        }
        packet.emg = emg_output(&emg);
        packet.bpm = hr_bpm(&heart);
        packet.seq = seq++;
        arm_packet_seal(&packet);
        esp_tx_start((const uint8_t *)&packet, sizeof(packet));
    }
}
