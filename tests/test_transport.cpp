#include "Esp01.h"
#include <cassert>
#include <iostream>

int main(int argc, char **argv)
{
    uint8_t payload[16] = {};
    assert(!esp_tx_start(nullptr, 1));
    assert(!esp_tx_start(payload, 0));
    assert(!esp_tx_start(payload, 33));
    Serial.capacity = 0;
    assert(!esp_tx_start(payload, 16));
    Serial.capacity = 64;
    assert(esp_tx_start(payload, 16));
    assert(Serial.tx == "AT+CIPSEND=16\r\n");
    assert(!esp_tx_start(payload, 16));
    Serial.inject("\r\nOK\r\n>"); esp_tx_service();
    assert(esp_tx_busy());
    assert(Serial.tx.size() == std::strlen("AT+CIPSEND=16\r\n") + 16);
    Serial.inject("\r\nSEND O"); esp_tx_service(); assert(esp_tx_busy());
    Serial.inject("K\r\n"); esp_tx_service(); assert(!esp_tx_busy());
    assert(esp_tx_start(payload, 16));
    if (argc > 1 && std::strcmp(argv[1], "error") == 0) {
        Serial.inject("ERROR\r\n");
    } else {
        if (argc > 1 && std::strcmp(argv[1], "ack-timeout") == 0) {
            Serial.inject(">"); esp_tx_service();
        }
        fake_ms = 1001;
    }
    esp_tx_service();
    assert(esp_tx_faulted() && !esp_tx_start(payload, 16));
    Serial.inject(">"); esp_tx_service(); assert(esp_tx_faulted());
    std::cout << "PASS ESP sender: backpressure, fragmented ACK, no overlap, timeout latch\n";
}
