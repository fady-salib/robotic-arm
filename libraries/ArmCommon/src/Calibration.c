#include "Calibration.h"
#include <math.h>

void flex_init(FlexChannel *c, float alpha)
{
    c->raw_min  = 0xFFFF;
    c->raw_max  = 0;
    c->filtered = 0.0f;
    c->alpha    = alpha;
    c->primed   = 0;
}

void flex_learn(FlexChannel *c, uint16_t raw)
{
    if (raw < c->raw_min) c->raw_min = raw;
    if (raw > c->raw_max) c->raw_max = raw;
}

uint16_t flex_scale(FlexChannel *c, uint16_t raw)
{
    if (c->raw_max < c->raw_min ||
        (uint16_t)(c->raw_max - c->raw_min) < CAL_MIN_SPAN) {
        return 0;   /* not calibrated or sensor disconnected */
    }

    int32_t v = ((int32_t)raw - c->raw_min) * CAL_OUT_MAX /
                (int32_t)(c->raw_max - c->raw_min);
    if (v < 0) v = 0;
    if (v > CAL_OUT_MAX) v = CAL_OUT_MAX;

    if (!c->primed) {
        c->filtered = (float)v;
        c->primed = 1;
    } else {
        c->filtered += c->alpha * ((float)v - c->filtered);
    }
    return (uint16_t)(c->filtered + 0.5f);
}

void emg_init(EmgChannel *e, float alpha)
{
    e->baseline = 0.0f;
    e->envelope = 0.0f;
    e->peak     = 1.0f;
    e->alpha    = alpha;
}

void emg_set_baseline(EmgChannel *e, float baseline)
{
    e->baseline = baseline;
}

void emg_feed(EmgChannel *e, uint16_t raw)
{
    float rectified = fabsf((float)raw - e->baseline);
    e->envelope += e->alpha * (rectified - e->envelope);
}

void emg_learn_peak(EmgChannel *e)
{
    if (e->envelope > e->peak) e->peak = e->envelope;
}

uint16_t emg_output(const EmgChannel *e)
{
    if (e->peak < EMG_MIN_PEAK) return 0; /* no usable calibration */
    float peak = (e->peak < EMG_MIN_PEAK) ? EMG_MIN_PEAK : e->peak;
    float v = e->envelope * CAL_OUT_MAX / peak;
    if (v < 0.0f) v = 0.0f;
    if (v > CAL_OUT_MAX) v = CAL_OUT_MAX;
    return (uint16_t)(v + 0.5f);
}
