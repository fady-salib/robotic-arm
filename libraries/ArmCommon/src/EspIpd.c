#include "EspIpd.h"
#include <string.h>

bool esp_ipd_feed(EspIpdParser *p, uint8_t byte, uint32_t now_ms,
                  uint8_t *output, uint8_t expected_length)
{
    static const char header[] = "+IPD,";
    if ((p->state || p->match) &&
        (uint32_t)(now_ms - p->last_byte_ms) > ESP_IPD_GAP_MS) {
        p->state = 0;
        p->match = 0;
    }
    p->last_byte_ms = now_ms;
    switch (p->state) {
    case 0:
        if (byte == (uint8_t)header[p->match]) {
            if (++p->match == sizeof(header) - 1) {
                p->match = 0;
                p->state = 1;
                p->digits = 0;
                p->number = 0;
            }
        } else p->match = (byte == '+') ? 1 : 0;
        break;
    case 1: /* connection id */
    case 2: /* payload length */
        if (byte >= '0' && byte <= '9') {
            if (++p->digits > 4) { p->state = 0; break; }
            p->number = (uint16_t)(p->number * 10 + byte - '0');
            if (p->number > (p->state == 1 ? 4 : 1500)) p->state = 0;
        } else if (p->state == 1 && byte == ',' && p->digits) {
            p->state = 2;
            p->digits = 0;
            p->number = 0;
        } else if (p->state == 2 && byte == ':' && p->digits) {
            p->length = p->number;
            p->received = 0;
            p->state = p->length ? 3 : 0;
        } else {
            p->state = 0;
        }
        break;
    default: /* binary payload; never interpret embedded headers */
        if (p->received < ESP_IPD_MAX) p->data[p->received] = byte;
        if (++p->received == p->length) {
            p->state = 0;
            if (output && expected_length && expected_length <= ESP_IPD_MAX &&
                p->length == expected_length) {
                memcpy(output, p->data, expected_length);
                return true;
            }
        }
        break;
    }
    return false;
}
