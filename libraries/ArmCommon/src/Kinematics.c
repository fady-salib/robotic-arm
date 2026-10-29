#include "Kinematics.h"
#include <math.h>

void kin_fk_2link(float l1, float l2, float t1, float t2, float *x, float *y)
{
    *x = l1 * cosf(t1) + l2 * cosf(t1 + t2);
    *y = l1 * sinf(t1) + l2 * sinf(t1 + t2);
}

bool kin_ik_2link(float l1, float l2, float x, float y, bool elbow_up,
                  float *t1, float *t2)
{
    if (!t1 || !t2) return false;
    *t1 = 0.0f;
    *t2 = 0.0f;
    if (!isfinite(l1) || !isfinite(l2) || !isfinite(x) || !isfinite(y) ||
        l1 <= 0.0f || l2 <= 0.0f) return false;
    bool reachable = true;
    float r = hypotf(x, y);
    float r_max = l1 + l2;
    float r_min = fabsf(l1 - l2);
    float denom = 2.0f * l1 * l2;
    if (!isfinite(r) || !isfinite(r_max) || !isfinite(denom) ||
        denom <= 0.0f) return false;

    if (r > r_max || r < r_min) {
        float target = (r > r_max) ? r_max : r_min;
        if (r == 0.0f) { x = target; y = 0.0f; }   /* avoid divide by zero */
        else           { x *= target / r; y *= target / r; }
        reachable = false;
    }

    float c2 = (x * x + y * y - l1 * l1 - l2 * l2) / denom;
    if (!isfinite(c2)) return false;
    if (c2 > 1.0f)  c2 = 1.0f;
    if (c2 < -1.0f) c2 = -1.0f;
    float s2 = sqrtf(1.0f - c2 * c2);
    if (!elbow_up) s2 = -s2;

    *t2 = atan2f(s2, c2);
    *t1 = atan2f(y, x) - atan2f(l2 * s2, l1 + l2 * c2);
    return reachable;
}

float kin_rad_to_deg(float rad)
{
    return rad * 57.2957795f;
}
