/* Esp01.h - minimal UDP driver for an ESP-01 / ESP8266 running AT firmware.
 *
 * Uses the Nano's hardware Serial (D0/D1). No SoftwareSerial, because it
 * can disturb Servo interrupt timing. No dynamic memory, no String.
 */
#ifndef ARM_ESP01_H
#define ARM_ESP01_H

#include <Arduino.h>
#include <stdint.h>

void esp_begin(uint32_t baud);
void esp_drain(void);                                   /* discard RX bytes   */
bool esp_wait_for(const char *expect, uint16_t timeout_ms);
bool esp_join(const char *ssid, const char *pass);      /* station mode + join */
bool esp_open_udp_tx(const char *dest_ip, uint16_t port);
bool esp_open_udp_rx(uint16_t port);

/* Non-blocking UDP send (at 9600 baud a blocking send would stall sensor
 * sampling). Call esp_tx_service() on every loop pass. */
#define ESP_TX_MAX 32
bool esp_tx_busy(void);
bool esp_tx_start(const uint8_t *data, uint8_t len);    /* false if busy      */
void esp_tx_service(void);                              /* advance + eat RX   */
bool esp_tx_faulted(void); /* timeout/error latches until MCU+module restart */

bool esp_poll(uint8_t *buf, uint8_t len);               /* true when `len`-byte
                                                           datagram is in buf */
#endif
