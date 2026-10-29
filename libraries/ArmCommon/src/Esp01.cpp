#include "Esp01.h"
#include "EspIpd.h"

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
    if (!esp_simple(F("AT+CIPDINFO=0"), 1000)) return false;
    esp_drain();
    Serial.print(F("AT+CIPSTART=0,\"UDP\",\"0.0.0.0\","));
    Serial.print(port);
    Serial.print(',');
    Serial.print(port);
    Serial.print(F(",2\r\n"));
    return esp_wait_for("OK", 3000);
}

/* Non-blocking CIPSEND transaction. Do not issue another command until the
 * module acknowledges SEND OK. On ambiguity, latch a fault rather than sending
 * an AT command that a module still awaiting payload could transmit as data. */
static uint8_t tx_buf[ESP_TX_MAX];
static uint8_t tx_len = 0;
static uint8_t tx_state = 0; /* idle, prompt, SEND OK, fault */
static uint32_t tx_start_ms = 0;
static char tx_reply[16];
static uint8_t tx_reply_len = 0;

bool esp_tx_busy(void) { return tx_state != 0; }
bool esp_tx_faulted(void) { return tx_state == 3; }

bool esp_tx_start(const uint8_t *data, uint8_t len)
{
    if (tx_state || !data || !len || len > ESP_TX_MAX ||
        Serial.availableForWrite() < 24) return false;
    memcpy(tx_buf, data, len);
    tx_len = len;
    tx_reply_len = 0;
    tx_reply[0] = 0;
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
        char c = (char)Serial.read();
        if (tx_state != 1 && tx_state != 2) continue;
        if (tx_reply_len == sizeof(tx_reply) - 1) {
            memmove(tx_reply, tx_reply + 1, --tx_reply_len);
        }
        tx_reply[tx_reply_len++] = c;
        tx_reply[tx_reply_len] = 0;
        if (strstr(tx_reply, "ERROR") || strstr(tx_reply, "FAIL") ||
            strstr(tx_reply, "busy")) {
            tx_state = 3;
        } else if (tx_state == 1 && c == '>') {
            if (Serial.availableForWrite() < tx_len) {
                tx_state = 3;
            } else {
                Serial.write(tx_buf, tx_len);
                tx_state = 2;
                tx_start_ms = millis();
                tx_reply_len = 0;
                tx_reply[0] = 0;
            }
        } else if (tx_state == 2 && strstr(tx_reply, "SEND OK")) {
            tx_state = 0;
        }
    }
    if ((tx_state == 1 || tx_state == 2) &&
        millis() - tx_start_ms > 1000) tx_state = 3;
}

bool esp_poll(uint8_t *buf, uint8_t len)
{
    static EspIpdParser parser = {};
    while (Serial.available()) {
        if (esp_ipd_feed(&parser, (uint8_t)Serial.read(), millis(), buf, len))
            return true;
    }
    return false;
}
