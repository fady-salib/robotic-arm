#include "HeartMonitor.h"

void hr_reset(HeartMonitor *h)
{
    h->hi = 0.0f;
    h->lo = 0.0f;
    h->last_beat_ms = 0;
    h->primed = 0;
    h->have_beat = 0;
    h->above = 0;
    h->count = 0;
    h->idx = 0;
    h->bpm = 0;
}

uint8_t hr_bpm(const HeartMonitor *h)
{
    return h->bpm;
}

static void hr_store(HeartMonitor *h, uint8_t bpm)
{
    h->rates[h->idx] = bpm;
    h->idx = (uint8_t)((h->idx + 1) % HR_HISTORY);
    if (h->count < HR_HISTORY) h->count++;

    uint16_t sum = 0;
    for (uint8_t i = 0; i < h->count; i++) sum += h->rates[i];
    h->bpm = (uint8_t)(sum / h->count);
}

void hr_feed(HeartMonitor *h, uint16_t raw, uint32_t now_ms)
{
    float x = (float)raw;

    if (!h->primed) {
        h->hi = x;
        h->lo = x;
        h->primed = 1;
        return;
    }

    /* Peak trackers: jump to new extremes, decay slowly toward the signal */
    if (x > h->hi) h->hi = x; else h->hi -= (h->hi - x) * HR_DECAY;
    if (x < h->lo) h->lo = x; else h->lo += (x - h->lo) * HR_DECAY;

    if (h->have_beat && (now_ms - h->last_beat_ms) > HR_TIMEOUT_MS) {
        h->bpm = 0;
        h->count = 0;
        h->idx = 0;
        h->have_beat = 0;
        h->above = 0;
    }

    float span = h->hi - h->lo;
    if (span < HR_MIN_SPAN) {
        h->above = 0;
        return;
    }

    float rise = h->lo + 0.60f * span;   /* beat when signal climbs above   */
    float fall = h->lo + 0.40f * span;   /* re-arm when it drops below this */

    if (x > rise) {
        if (!h->above) {
            h->above = 1;
            uint32_t dt = now_ms - h->last_beat_ms;
            if (h->have_beat && dt < HR_REFRACTORY_MS) {
                /* too soon after the last beat: noise, ignore */
            } else {
                if (h->have_beat && dt <= HR_TIMEOUT_MS) {
                    uint32_t bpm = 60000UL / dt;
                    if (bpm >= 30 && bpm <= 220) hr_store(h, (uint8_t)bpm);
                }
                h->last_beat_ms = now_ms;
                h->have_beat = 1;
            }
        }
    } else if (x < fall) {
        h->above = 0;
    }
}
