/* iBlueControlMod: motion of a controller as a position. See iblue_motion.h. */

#include <math.h>
#include <string.h>
#include "iblue/iblue_motion.h"

#define RAD2DEG 57.2957795f
#define DEG2RAD 0.0174532925f
#define STILL_GYRO 0.06f   /* rad/s: below this (after the bias) the controller counts as still */
#define STILL_ACC 0.06f    /* g: and the acceleration is within this of 1 g */
#define STILL_POSE_S 0.35f /* how long it has to be still for a pose */

/* mode, el_range, yaw_range, use_x, shake_mg, shake_ms, lockout_ms, y_invert, x_invert, yaw_leak, filter_ms, cal_button */
const struct iblue_motion_cfg iblue_motion_cfg_default = {0, 45, 45, 1, 1200, 80, 120, 0, 0, 0, 40, 1};
const struct iblue_motion_cfg iblue_motion_cfg_min     = {0, 10, 10, 0,  300, 20,   0, 0, 0, 0,   0, 0};
const struct iblue_motion_cfg iblue_motion_cfg_max     = {1, 90, 120, 1, 4000, 300, 500, 1, 1, 100, 300, 1};

int iblue_motion_cfg_clamp(struct iblue_motion_cfg *c) {
    int16_t *v = (int16_t *)c;
    const int16_t *lo = (const int16_t *)&iblue_motion_cfg_min;
    const int16_t *hi = (const int16_t *)&iblue_motion_cfg_max;
    int changed = 0;

    for (int i = 0; i < IBLUE_MOTION_CFG_FIELDS; i++) {
        if (v[i] < lo[i]) { v[i] = lo[i]; changed = 1; }
        if (v[i] > hi[i]) { v[i] = hi[i]; changed = 1; }
    }
    return changed;
}

static float dot(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static float norm(const float a[3]) { return sqrtf(dot(a, a)); }

static void cross(const float a[3], const float b[3], float out[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

static int unit(float a[3]) {
    float n = norm(a);

    if (n < 1e-6f) { return -1; }
    a[0] /= n; a[1] /= n; a[2] /= n;
    return 0;
}

static float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

void iblue_motion_set_cfg(struct iblue_motion *m, const struct iblue_motion_cfg *cfg) {
    m->cfg = *cfg;
    iblue_motion_cfg_clamp(&m->cfg);
}

void iblue_motion_init(struct iblue_motion *m, const struct iblue_motion_cfg *cfg) {
    memset(m, 0, sizeof(*m));
    iblue_motion_set_cfg(m, cfg ? cfg : &iblue_motion_cfg_default);
    m->axis[0] = 1.0f; /* a guess for the long axis of a Joy-Con until the up pose has been taken */
    m->x_sign = 1.0f;
    m->up[2] = 1.0f;
    m->hp_mag = 1.0f;
}

/* the height as an angle above the horizon, radians */
static float elevation(const struct iblue_motion *m) {
    return asinf(clampf(dot(m->axis, m->up), -1.0f, 1.0f));
}

float iblue_motion_elevation_deg(const struct iblue_motion *m) { return (elevation(m) - m->el_zero) * RAD2DEG; }
float iblue_motion_heading_deg(const struct iblue_motion *m) { return -m->yaw * m->x_sign * RAD2DEG; } /* right is positive */

void iblue_motion_update(struct iblue_motion *m, const float accel[3], const float gyro[3], float dt) {
    float mag = norm(accel);
    float w[3] = {gyro[0] - m->bias[0], gyro[1] - m->bias[1], gyro[2] - m->bias[2]};
    float still_gyro = norm(w);
    float au[3];

    if (dt <= 0.0f || dt > 0.5f || mag < 0.2f) {
        return; /* no usable sample */
    }
    m->now += dt;
    au[0] = accel[0] / mag; au[1] = accel[1] / mag; au[2] = accel[2] / mag;

    if (!m->started) {
        memcpy(m->up, au, sizeof(au));
        m->hp_mag = mag;
        m->started = 1;
        return;
    }

    /* still? then learn the gyro bias and count the time */
    if (still_gyro < STILL_GYRO && fabsf(mag - 1.0f) < STILL_ACC) {
        m->still_t += dt;
        if (m->still_t > 0.4f) {
            for (int i = 0; i < 3; i++) { m->bias[i] += (gyro[i] - m->bias[i]) * clampf(dt * 0.5f, 0.0f, 1.0f); }
        }
    }
    else {
        m->still_t = 0.0f;
    }

    /* heading: the turn about the vertical (about "up"), counter-clockwise from above is positive */
    m->yaw += dot(w, m->up) * dt;
    if (m->cfg.yaw_leak > 0) {
        m->yaw *= 1.0f - clampf((float)m->cfg.yaw_leak * 0.01f * dt, 0.0f, 1.0f);
    }

    /* "up" in the controller's frame: turn it against the controller's rotation, then pull it towards the
     * accelerometer when that is trustworthy (about 1 g: not shaken) */
    {
        float c[3];
        float k;

        cross(w, m->up, c);
        m->up[0] -= c[0] * dt; m->up[1] -= c[1] * dt; m->up[2] -= c[2] * dt;
        unit(m->up);
        k = 0.04f * clampf(1.0f - fabsf(mag - 1.0f) / 0.3f, 0.0f, 1.0f);
        if (m->still_t > 0.3f) { k = 0.2f; }
        m->up[0] += (au[0] - m->up[0]) * k;
        m->up[1] += (au[1] - m->up[1]) * k;
        m->up[2] += (au[2] - m->up[2]) * k;
        unit(m->up);
    }

    /* shake: a sudden change of the strength of the acceleration */
    {
        float change = mag - m->hp_mag;

        m->hp_mag += change * (dt / (dt + 0.15f));
        if (fabsf(change) * 1000.0f > (float)m->cfg.shake_mg && m->now >= m->lock_until) {
            m->shake_until = m->now + (float)m->cfg.shake_ms * 0.001f;
            m->lock_until = m->shake_until + (float)m->cfg.lockout_ms * 0.001f;
            m->shakes++;
        }
        m->shaking = m->now < m->shake_until;
    }

    /* the outputs, smoothed */
    {
        float yr = clampf(iblue_motion_elevation_deg(m) / (float)m->cfg.el_range, -1.0f, 1.0f);
        float range = m->yaw_range_deg > 0.0f ? m->yaw_range_deg : (float)m->cfg.yaw_range;
        float xr = m->cfg.use_x ? clampf(iblue_motion_heading_deg(m) / range, -1.0f, 1.0f) : 0.0f;
        float a = (m->cfg.filter_ms > 0) ? dt / (dt + (float)m->cfg.filter_ms * 0.001f) : 1.0f;

        if (m->cfg.y_invert) { yr = -yr; }
        if (m->cfg.x_invert) { xr = -xr; }
        m->ys += (yr - m->ys) * a;
        m->xs += (xr - m->xs) * a;
    }
}

int iblue_motion_pose(struct iblue_motion *m, int pose) {
    float p[3];

    if (!m->started || m->still_t < STILL_POSE_S) {
        return -1;
    }
    switch (pose) {
        case IBLUE_MOTION_POSE_FORWARD:
            memcpy(m->fwd_up, m->up, sizeof(m->up));
            m->fwd_known = 1;
            m->yaw = 0.0f;
            m->el_zero = elevation(m);
            m->xs = 0.0f;
            m->ys = 0.0f;
            return 0;
        case IBLUE_MOTION_POSE_UP:
        case IBLUE_MOTION_POSE_DOWN:
            /* pointing up, the accelerometer reads 1 g along the pointing axis; pointing down, minus 1 g */
            if (pose == IBLUE_MOTION_POSE_UP) { memcpy(m->pose_up, m->up, sizeof(m->up)); m->have_up = 1; }
            else { memcpy(m->pose_dn, m->up, sizeof(m->up)); m->have_dn = 1; }
            p[0] = p[1] = p[2] = 0.0f;
            if (m->have_up) { for (int i = 0; i < 3; i++) { p[i] += m->pose_up[i]; } }
            if (m->have_dn) { for (int i = 0; i < 3; i++) { p[i] -= m->pose_dn[i]; } }
            if (unit(p) != 0) { return -1; }
            if (m->fwd_known) {
                /* the pointing axis is level in the forward pose: take out what is along "up" of that pose */
                float d = dot(p, m->fwd_up);

                p[0] -= d * m->fwd_up[0]; p[1] -= d * m->fwd_up[1]; p[2] -= d * m->fwd_up[2];
                if (unit(p) != 0) { return -1; }
            }
            memcpy(m->axis, p, sizeof(p));
            m->axis_known = 1;
            if (m->fwd_known) { m->el_zero = asinf(clampf(dot(m->axis, m->fwd_up), -1.0f, 1.0f)); }
            return 0;
        case IBLUE_MOTION_POSE_OUT:
        {
            /* pointing outwards, level: the heading says how far "left / right" is and which way */
            float h = m->yaw; /* counter-clockwise positive, from the forward pose */

            if (!m->fwd_known || m->side == 0 || fabsf(h) < 20.0f * DEG2RAD) { return -1; }
            m->x_sign = (-(float)m->side * h > 0.0f) ? 1.0f : -1.0f;
            m->yaw_range_deg = clampf(fabsf(h) * RAD2DEG, 20.0f, 120.0f);
            return 0;
        }
        default:
            return -1;
    }
}

float iblue_motion_x(const struct iblue_motion *m) { return m->xs; }
float iblue_motion_y(const struct iblue_motion *m) { return m->ys; }
int iblue_motion_shake(const struct iblue_motion *m) { return m->shaking; }
