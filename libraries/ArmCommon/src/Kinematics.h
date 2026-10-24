/* Kinematics.h - planar 2-link arm.
 *
 * Joint 1 = elbow (rotates the forearm), joint 2 = wrist (rotates the hand).
 * Angles in radians, lengths in metres, x forward, y up.
 */
#ifndef ARM_KINEMATICS_H
#define ARM_KINEMATICS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward kinematics: joint angles -> fingertip position. */
void kin_fk_2link(float l1, float l2, float t1, float t2,
                  float *x, float *y);

/* Inverse kinematics: fingertip position -> joint angles.
 * Targets outside the reachable ring are pulled onto it.
 * Returns true if the target was reachable as given, false if clamped. */
bool kin_ik_2link(float l1, float l2, float x, float y, bool elbow_up,
                  float *t1, float *t2);

float kin_rad_to_deg(float rad);

#ifdef __cplusplus
}
#endif
#endif
