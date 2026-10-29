/* Incremental parser for CIPMUX=1, CIPDINFO=0: +IPD,<id>,<length>:<bytes>. */
#ifndef ARM_ESP_IPD_H
#define ARM_ESP_IPD_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define ESP_IPD_MAX 32
#define ESP_IPD_GAP_MS 100
typedef struct {
    uint32_t last_byte_ms;
    uint16_t length, received, number;
    uint8_t state, match, digits;
    uint8_t data[ESP_IPD_MAX];
} EspIpdParser;
/* Zero-initialize once. Output is published atomically only on a full frame.
 * Other lengths are consumed and discarded (up to 1500 bytes). */
bool esp_ipd_feed(EspIpdParser *p, uint8_t byte, uint32_t now_ms,
                  uint8_t *output, uint8_t expected_length);
#ifdef __cplusplus
}
#endif
#endif
