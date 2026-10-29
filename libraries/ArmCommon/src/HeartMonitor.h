/* HeartMonitor.h - heart rate from an AD8232 ECG analog output.
 *
 * Detects R-peaks with an adaptive threshold (decaying min/max trackers),
 * a hysteresis band and a refractory period, then averages the last 4
 * instantaneous BPM estimates. Feed it a sample every ~4 ms (250 Hz).
 */
#ifndef ARM_HEART_MONITOR_H
#define ARM_HEART_MONITOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HR_HISTORY        4
#define HR_REFRACTORY_MS  300     /* max ~200 bpm */
#define HR_TIMEOUT_MS     3000    /* no beat for this long -> bpm = 0 */
#define HR_MIN_SPAN       40.0f   /* ADC counts; below this = no signal */
#define HR_DECAY          0.002f  /* per-sample decay of the peak trackers */

typedef struct {
    float    hi, lo;
    uint32_t last_beat_ms;
    uint8_t  primed, have_beat, above;
    uint8_t  count, idx;
    uint8_t  rates[HR_HISTORY];
    uint8_t  bpm;
} HeartMonitor;

void    hr_reset(HeartMonitor *h);   /* also call when electrodes are off */
void    hr_feed(HeartMonitor *h, uint16_t raw, uint32_t now_ms);
uint8_t hr_bpm(const HeartMonitor *h);

#ifdef __cplusplus
}
#endif
#endif
