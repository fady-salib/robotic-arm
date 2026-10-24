/* Calibration.h - turn noisy raw ADC counts into clean 0..1000 values.
 *
 * Raw reading   : whatever the ADC returns (depends on wiring, voltage, user).
 * Calibrated    : raw mapped onto the range seen during calibration, filtered.
 */
#ifndef ARM_CALIBRATION_H
#define ARM_CALIBRATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CAL_OUT_MAX   1000
#define CAL_MIN_SPAN  30    /* ignore ranges smaller than this (no signal) */
#define EMG_MIN_PEAK  25.0f /* never amplify noise below this many counts */

/* ---- Flex sensor: min/max scaling + exponential smoothing ---- */
typedef struct {
    uint16_t raw_min;
    uint16_t raw_max;
    float    filtered;
    float    alpha;      /* 0..1, higher = faster, noisier */
    uint8_t  primed;
} FlexChannel;

void     flex_init(FlexChannel *c, float alpha);
void     flex_learn(FlexChannel *c, uint16_t raw);   /* call while calibrating */
uint16_t flex_scale(FlexChannel *c, uint16_t raw);   /* 0..1000 */

/* ---- EMG: remove DC offset, rectify, smooth into an envelope ---- */
typedef struct {
    float baseline;     /* resting raw value */
    float envelope;
    float peak;         /* strongest contraction seen during calibration */
    float alpha;
} EmgChannel;

void     emg_init(EmgChannel *e, float alpha);
void     emg_set_baseline(EmgChannel *e, float baseline);
void     emg_feed(EmgChannel *e, uint16_t raw);      /* update envelope */
void     emg_learn_peak(EmgChannel *e);              /* call after emg_feed */
uint16_t emg_output(const EmgChannel *e);            /* 0..1000 */

#ifdef __cplusplus
}
#endif
#endif
