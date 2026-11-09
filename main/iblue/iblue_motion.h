/*
 * iBlueControlMod: motion of a controller (Joy-Con) as a position, as pure maths.
 * Portable C (no ESP-IDF), tested on the host (tests/host/test_motion.c). Used for the Samba de Amigo maracas
 * (docs/samba-de-amigo.md): the game wants each maraca as an X and Y position and a shake switch; a controller with
 * an accelerometer and a gyro knows which way it points and how it moves, so
 *   Y (height)    = how far the controller points above or below the horizon,
 *   X (sideways)  = how far it has turned left or right of "forward" (the TV),
 *   shake         = a burst of acceleration.
 *
 * What it needs to know:
 *  - the pointing axis: which of the controller's own axes is the long axis of the maraca (a constant of the
 *    controller, learned once with the "up" and "down" poses, kept over restarts),
 *  - forward: where the TV is, learned each session with the "forward" pose (both controllers pointed at the TV,
 *    level); it sets the heading to zero.
 * Poses (all taken with the controller held still): forward, up, down, out (outwards to its own side).
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_MOTION_H_
#define _IBLUE_MOTION_H_

#include <stdint.h>

/* Settings (the numbers are what the BLE command and the NVS blob carry, 16 bit each, in this order) */
struct iblue_motion_cfg {
    int16_t mode;        /* 0 off, 1 maracas: the motion of Joy-Cons becomes a position and a shake */
    int16_t el_range;    /* degrees above / below the horizon that give the full height range */
    int16_t yaw_range;   /* degrees left / right of forward that give the full sideways range */
    int16_t use_x;       /* 1: report the sideways position, 0: leave X in the middle */
    int16_t shake_mg;    /* strength of a shake (change in acceleration), in thousandths of g */
    int16_t shake_ms;    /* how long the shake switch stays pressed, ms */
    int16_t lockout_ms;  /* after a shake no new one for this long, ms */
    int16_t y_invert;    /* 1: turn the height around */
    int16_t x_invert;    /* 1: turn the sideways position around */
    int16_t yaw_leak;    /* the heading drifts back to forward at this rate, hundredths of a degree-fraction per second (0 = never) */
    int16_t filter_ms;   /* smoothing of X and Y, ms */
    int16_t cal_button;  /* 1: Home (right Joy-Con) or Capture (left) held for a second calibrates forward for all */
};
#define IBLUE_MOTION_CFG_FIELDS 12

extern const struct iblue_motion_cfg iblue_motion_cfg_default;
extern const struct iblue_motion_cfg iblue_motion_cfg_min;
extern const struct iblue_motion_cfg iblue_motion_cfg_max;
int iblue_motion_cfg_clamp(struct iblue_motion_cfg *c); /* 1 if anything changed */

/* Poses, in the order of the BLE command */
enum {
    IBLUE_MOTION_POSE_FORWARD = 0, /* pointing at the TV, level (heading zero) */
    IBLUE_MOTION_POSE_UP,          /* pointing straight up */
    IBLUE_MOTION_POSE_DOWN,        /* pointing straight down */
    IBLUE_MOTION_POSE_OUT,         /* pointing outwards to its own side (the left one to the left, the right one to the
                                    * right), level: sets the sideways range and which way is right */
    IBLUE_MOTION_POSE_MAX,
};

struct iblue_motion {
    struct iblue_motion_cfg cfg;
    /* learned, kept over restarts */
    float axis[3];        /* the pointing axis in the controller's frame, unit length */
    int8_t axis_known;    /* 0 default guess, 1 learned from the up (and down) pose */
    float x_sign;         /* +1 or -1: which way of turning is "right" for this controller */
    float yaw_range_deg;  /* the sideways range learned with the out pose, 0 = use the setting */
    int8_t side;          /* -1 the left controller (a left Joy-Con), +1 the right one, 0 unknown */
    /* per session */
    float up[3];          /* the direction of "up" in the controller's frame (from gravity), unit length */
    float bias[3];        /* gyro bias, rad/s */
    float yaw;            /* heading relative to forward, rad (turned right is positive after x_sign) */
    float fwd_up[3];      /* "up" in the forward pose */
    float el_zero;        /* the height of the pointing axis in the forward pose: that is the middle */
    int8_t fwd_known;
    float hp_mag;         /* low-passed |acceleration|, g */
    float still_t;        /* how long the controller has been still, s */
    float bias_sum[3];
    float bias_t;
    float now;            /* time, s */
    float shake_until;
    float lock_until;
    float xs, ys;         /* smoothed outputs */
    float pose_up[3], pose_dn[3];
    int8_t have_up, have_dn;
    int32_t shakes;       /* count, for the status */
    int8_t shaking;
    int8_t started;
};

void iblue_motion_init(struct iblue_motion *m, const struct iblue_motion_cfg *cfg);
void iblue_motion_set_cfg(struct iblue_motion *m, const struct iblue_motion_cfg *cfg);
/* One sample: acceleration in g (a controller lying still reads about 1 g upwards), gyro in rad/s, dt in seconds. */
void iblue_motion_update(struct iblue_motion *m, const float accel[3], const float gyro[3], float dt);
/* Take the pose the controller is held in now. Returns 0, or -1 if it is not held still enough. */
int iblue_motion_pose(struct iblue_motion *m, int pose);

/* Outputs: x and y in -1..1 (0 = forward / level), shake 0 or 1 */
float iblue_motion_x(const struct iblue_motion *m);
float iblue_motion_y(const struct iblue_motion *m);
int iblue_motion_shake(const struct iblue_motion *m);
/* Height above the horizon, degrees, and heading, degrees (for the status and the tests) */
float iblue_motion_elevation_deg(const struct iblue_motion *m);
float iblue_motion_heading_deg(const struct iblue_motion *m);

#endif /* _IBLUE_MOTION_H_ */
