/* iBlueControlMod: Joy-Con motion, settings and per controller state. See iblue_motion_ctl.h. */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <nvs.h>
#include <esp_timer.h>
#include "iblue/iblue_motion_ctl.h"
#include "iblue/iblue_led.h"

#ifdef CONFIG_BLUERETRO_IBLUE_MOTION

#define NVS_NS "iblue"
#define KEY_CFG "mcfg"
#define KEY_CAL "mcal"          /* one blob per side: mcal0 left, mcal1 right */
#define BLOB_VERSION 1
#define SAMPLE_DT 0.005f        /* three samples per report: 5 ms apart */
#define ACC_SCALE 0.000244f     /* g per step at +-8 g */
#define GYRO_SCALE 0.001222f    /* rad/s per step at +-2000 dps */
#define POSE_WAIT_MS 4000       /* a pose is tried for this long, then it counts as failed */
#define CAL_HOLD_S 1.0f

struct cal_blob {               /* what is learned about one side and kept */
    uint8_t version;
    uint8_t axis_known;
    float axis[3];
    float x_sign;
    float yaw_range_deg;
};

struct dev_state {
    int used;
    int side;
    struct iblue_motion m;
    uint32_t seen_seq;
    int pose_failed;
    float hold_t;
    int hold_done;
};

static struct iblue_motion_cfg cfg;
static struct cal_blob cal[2];
static struct dev_state devs[IBLUE_MOTION_DEVS];
static volatile uint32_t req_seq;
static volatile int req_pose;
static volatile int64_t req_due_ms;

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static int side_idx(int side) { return side < 0 ? 0 : 1; }

static void cal_save(int idx) {
    nvs_handle_t nvs;
    char key[8];

    snprintf(key, sizeof(key), "%s%d", KEY_CAL, idx);
    if (nvs_open(NVS_NS, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_blob(nvs, key, &cal[idx], sizeof(cal[idx]));
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

void iblue_motion_ctl_init(void) {
    nvs_handle_t nvs;
    uint8_t blob[1 + 2 * IBLUE_MOTION_CFG_FIELDS];
    size_t len = sizeof(blob);

    cfg = iblue_motion_cfg_default;
    memset(cal, 0, sizeof(cal));
    if (nvs_open(NVS_NS, NVS_READONLY, &nvs) == ESP_OK) {
        if (nvs_get_blob(nvs, KEY_CFG, blob, &len) == ESP_OK && len == sizeof(blob) && blob[0] == BLOB_VERSION) {
            int16_t *v = (int16_t *)&cfg;

            for (int i = 0; i < IBLUE_MOTION_CFG_FIELDS; i++) {
                v[i] = (int16_t)((uint16_t)blob[1 + 2 * i] | ((uint16_t)blob[2 + 2 * i] << 8));
            }
        }
        for (int s = 0; s < 2; s++) {
            char key[8];
            struct cal_blob c;
            size_t l = sizeof(c);

            snprintf(key, sizeof(key), "%s%d", KEY_CAL, s);
            if (nvs_get_blob(nvs, key, &c, &l) == ESP_OK && l == sizeof(c) && c.version == BLOB_VERSION) {
                cal[s] = c;
            }
        }
        nvs_close(nvs);
    }
    iblue_motion_cfg_clamp(&cfg);
}

int iblue_motion_ctl_mode(void) { return cfg.mode == 1; }

void iblue_motion_ctl_get_cfg(struct iblue_motion_cfg *c) { *c = cfg; }

void iblue_motion_ctl_set_cfg(const struct iblue_motion_cfg *c) {
    nvs_handle_t nvs;
    uint8_t blob[1 + 2 * IBLUE_MOTION_CFG_FIELDS];
    const int16_t *v = (const int16_t *)&cfg;

    cfg = *c;
    iblue_motion_cfg_clamp(&cfg);
    for (int i = 0; i < IBLUE_MOTION_DEVS; i++) {
        if (devs[i].used) { iblue_motion_set_cfg(&devs[i].m, &cfg); }
    }
    blob[0] = BLOB_VERSION;
    for (int i = 0; i < IBLUE_MOTION_CFG_FIELDS; i++) {
        blob[1 + 2 * i] = (uint8_t)((uint16_t)v[i] & 0xFF);
        blob[2 + 2 * i] = (uint8_t)((uint16_t)v[i] >> 8);
    }
    if (nvs_open(NVS_NS, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_blob(nvs, KEY_CFG, blob, sizeof(blob));
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

void iblue_motion_ctl_reset_cal(void) {
    memset(cal, 0, sizeof(cal));
    for (int s = 0; s < 2; s++) { cal_save(s); }
    for (int i = 0; i < IBLUE_MOTION_DEVS; i++) {
        if (devs[i].used) { devs[i].used = 0; } /* started again from the defaults at the next report */
    }
}

void iblue_motion_ctl_request_pose(int pose, int delay_s) {
    if (pose < 0 || pose >= IBLUE_MOTION_POSE_MAX) { return; }
    if (delay_s < 0) { delay_s = 0; }
    if (delay_s > 30) { delay_s = 30; }
    req_pose = pose;
    req_due_ms = now_ms() + (int64_t)delay_s * 1000;
    req_seq++;
}

void iblue_motion_ctl_gone(int dev) {
    if (dev >= 0 && dev < IBLUE_MOTION_DEVS) { devs[dev].used = 0; }
}

static void dev_start(struct dev_state *d, int side) {
    int idx = side_idx(side);

    memset(d, 0, sizeof(*d));
    iblue_motion_init(&d->m, &cfg);
    d->used = 1;
    d->side = side < 0 ? -1 : 1;
    d->m.side = (int8_t)d->side;
    d->seen_seq = req_seq;
    if (cal[idx].version == BLOB_VERSION) {
        if (cal[idx].axis_known) {
            memcpy(d->m.axis, cal[idx].axis, sizeof(d->m.axis));
            d->m.axis_known = 1;
        }
        if (cal[idx].x_sign != 0.0f) { d->m.x_sign = cal[idx].x_sign; }
        d->m.yaw_range_deg = cal[idx].yaw_range_deg;
    }
}

static int16_t rd16(const uint8_t *p) { return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }

static void keep_learned(struct dev_state *d) {
    int idx = side_idx(d->side);

    cal[idx].version = BLOB_VERSION;
    cal[idx].axis_known = (uint8_t)d->m.axis_known;
    memcpy(cal[idx].axis, d->m.axis, sizeof(cal[idx].axis));
    cal[idx].x_sign = d->m.x_sign;
    cal[idx].yaw_range_deg = d->m.yaw_range_deg;
    cal_save(idx);
}

int iblue_motion_ctl_feed(int dev, int side, const uint8_t *imu, int cal_pressed, int16_t *x, int16_t *y, int *shake) {
    struct dev_state *d;
    int64_t t = now_ms();

    if (!iblue_motion_ctl_mode() || dev < 0 || dev >= IBLUE_MOTION_DEVS) { return 0; }
    d = &devs[dev];
    if (!d->used || d->side != (side < 0 ? -1 : 1)) { dev_start(d, side); }

    for (int s = 0; s < 3; s++) {
        const uint8_t *p = imu + 12 * s;
        float a[3], g[3];

        for (int i = 0; i < 3; i++) {
            a[i] = (float)rd16(p + 2 * i) * ACC_SCALE;
            g[i] = (float)rd16(p + 6 + 2 * i) * GYRO_SCALE;
        }
        iblue_motion_update(&d->m, a, g, SAMPLE_DT);
    }

    /* holding the calibration button (Minus on the left, Home on the right) for a second: forward for everyone */
    if (cfg.cal_button && cal_pressed) {
        d->hold_t += 3 * SAMPLE_DT;
        if (d->hold_t >= CAL_HOLD_S && !d->hold_done) {
            d->hold_done = 1;
            iblue_motion_ctl_request_pose(IBLUE_MOTION_POSE_FORWARD, 0);
        }
    }
    else {
        d->hold_t = 0.0f;
        d->hold_done = 0;
    }

    /* a pose that was asked for */
    if (d->seen_seq != req_seq && t >= req_due_ms) {
        int r = iblue_motion_pose(&d->m, req_pose);

        if (r == 0) {
            d->seen_seq = req_seq;
            d->pose_failed = 0;
            if (req_pose != IBLUE_MOTION_POSE_FORWARD) { keep_learned(d); }
            iblue_led_notice(1);
        }
        else if (t >= req_due_ms + POSE_WAIT_MS) {
            d->seen_seq = req_seq;
            d->pose_failed = 1;
            iblue_led_notice(3);
        }
    }

    *x = (int16_t)lroundf(iblue_motion_x(&d->m) * 1400.0f);
    *y = (int16_t)lroundf(iblue_motion_y(&d->m) * 1400.0f);
    *shake = iblue_motion_shake(&d->m);
    return 1;
}

static int8_t to_i8(float v, float lo, float hi) {
    return (int8_t)lroundf(v < lo ? lo : (v > hi ? hi : v));
}

int iblue_motion_ctl_status(uint8_t *out, int max) {
    int n = 0;

    for (int i = 0; i < IBLUE_MOTION_DEVS && n < max; i++) {
        struct dev_state *d = &devs[i];

        if (!d->used) { continue; }
        out[6 * n + 0] = (uint8_t)(int8_t)d->side;
        out[6 * n + 1] = (uint8_t)((d->m.fwd_known ? 1 : 0) | (d->m.axis_known ? 2 : 0) | (d->m.shaking ? 4 : 0) |
                                   (d->m.yaw_range_deg > 0.0f ? 8 : 0) | (d->pose_failed ? 16 : 0));
        out[6 * n + 2] = (uint8_t)to_i8(iblue_motion_x(&d->m) * 127.0f, -127, 127);
        out[6 * n + 3] = (uint8_t)to_i8(iblue_motion_y(&d->m) * 127.0f, -127, 127);
        out[6 * n + 4] = (uint8_t)to_i8(iblue_motion_elevation_deg(&d->m), -90, 90);
        out[6 * n + 5] = (uint8_t)to_i8(iblue_motion_heading_deg(&d->m) / 2.0f, -127, 127);
        n++;
    }
    return n;
}

#endif /* CONFIG_BLUERETRO_IBLUE_MOTION */
