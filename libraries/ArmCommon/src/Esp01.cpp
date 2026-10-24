#include "Esp01.h"

void esp_begin(uint32_t baud)
{
    Serial.begin(baud);
}

void esp_drain(void)
{
    while (Serial.available()) Serial.read();
}

bool esp_wait_for(const char *expect, uint16_t timeout_ms)
{
    uint8_t idx = 0;
    uint32_t start = millis();
    while (millis() - start < timeout_ms) {
        while (Serial.available()) {
            char c = (char)Serial.read();
            if (c == expect[idx]) idx++;
            else idx = (c == expect[0]) ? 1 : 0;
            if (expect[idx] == 0) return true;
        }
    }
    return false;
}

static bool esp_simple(const __FlashStringHelper *cmd, uint16_t timeout_ms)
{
    esp_drain();
    Serial.print(cmd);
    Serial.print(F("\r\n"));
    return esp_wait_for("OK", timeout_ms);
}

bool esp_join(const char *ssid, const char *pass)
{
    bool alive = false;
    for (uint8_t i = 0; i < 5 && !alive; i++) alive = esp_simple(F("AT"), 500);
    if (!alive) return false;

    esp_simple(F("ATE0"), 500);                       /* echo off */
    if (!esp_simple(F("AT+CWMODE=1"), 1000)) return false;

    esp_drain();
    Serial.print(F("AT+CWJAP=\""));
    Serial.print(ssid);
    Serial.print(F("\",\""));
    Serial.print(pass);
    Serial.print(F("\"\r\n"));
    return esp_wait_for("OK", 20000);
}

bool esp_open_udp_tx(const char *dest_ip, uint16_t port)
{
    if (!esp_simple(F("AT+CIPMUX=0"), 1000)) return false;
    esp_drain();
    Serial.print(F("AT+CIPSTART=\"UDP\",\""));
    Serial.print(dest_ip);
    Serial.print(F("\","));
    Serial.print(port);
    Serial.print(',');
    Serial.print(port);
    Serial.print(F(",0\r\n"));
    return esp_wait_for("OK", 3000);
}

bool esp_open_udp_rx(uint16_t port)
{
    if (!esp_simple(F("AT+CIPMUX=1"), 1000)) return false;
    esp_drain();
    Serial.print(F("AT+CIPSTART=0,\"UDP\",\"0.0.0.0\","));
    Serial.print(port);
    Serial.print(',');
    Serial.print(port);
    Serial.print(F(",2\r\n"));
    return esp_wait_for("OK", 3000);
}

/* ---- Non-blocking send: AT+CIPSEND=<n>, wait for '>', then write data ---- */
static uint8_t  tx_buf[ESP_TX_MAX];
static uint8_t  tx_len = 0;
static uint8_t  tx_state = 0;          /* 0 idle, 1 waiting for '>' prompt */
static uint32_t tx_start_ms = 0;

bool esp_tx_busy(void)
{
    return tx_state != 0;
}

bool esp_tx_start(const uint8_t *data, uint8_t len)
{
    if (tx_state != 0 || len > ESP_TX_MAX) return false;
    memcpy(tx_buf, data, len);
    tx_len = len;
    Serial.print(F("AT+CIPSEND="));
    Serial.print(len);
    Serial.print(F("\r\n"));
    tx_start_ms = millis();
    tx_state = 1;
    return true;
}

void esp_tx_service(void)
{
    while (Serial.available()) {
        char c = (char)Serial.read();    /* replies like "SEND OK" are discarded */
        if (tx_state == 1 && c == '>') {
            Serial.write(tx_buf, tx_len);
            tx_state = 0;
        }
    }
    if (tx_state == 1 && millis() - tx_start_ms > 300) tx_state = 0;  /* give up */
}

/* Incoming data looks like  +IPD,0,16:<16 raw bytes>  */
bool esp_poll(uint8_t *buf, uint8_t len)
{
    static const char HDR[] = "+IPD,";
    static uint8_t  state = 0;       /* 0 header, 1 meta, 2 payload, 3 skip */
    static uint8_t  match = 0;
    static uint16_t plen = 0;
    static uint16_t got = 0;
    static bool     past_comma = false;

    while (Serial.available()) {
        uint8_t c = (uint8_t)Serial.read();
        switch (state) {
        case 0:
            if (c == (uint8_t)HDR[match]) {
                match++;
                if (HDR[match] == 0) {
                    state = 1; match = 0; plen = 0; past_comma = false;
                }
            } else {
                match = (c == '+') ? 1 : 0;
            }
            break;
        case 1:
            if (c == ',') {
                past_comma = true;
                plen = 0;
            } else if (c == ':') {
                got = 0;
                state = (plen == len) ? 2 : (plen > 0 ? 3 : 0);
            } else if (c >= '0' && c <= '9' && past_comma) {
                plen = (uint16_t)(plen * 10 + (c - '0'));
                if (plen > 1500) { state = 0; match = 0; }
            }
            break;
        case 2:
            buf[got++] = c;
            if (got >= len) { state = 0; return true; }
            break;
        default:
            if (++got >= plen) state = 0;
            break;
        }
    }
    return false;
}
