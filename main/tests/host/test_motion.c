/* Host test of the motion maths (iblue/iblue_motion.c): a controller is moved the way a player would, with the
 * accelerometer and gyro it would give, and the tilt, heading, calibration and shake are checked against the truth. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../../iblue/iblue_motion.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR(a, b, tol) do { checks++; if (fabsf((float)(a) - (float)(b)) > (tol)) { fails++; printf("FAIL %s:%d: %s = %f, expected %f +- %f\n", __FILE__, __LINE__, #a, (double)(a), (double)(b), (double)(tol)); } } while (0)

#define HZ 200.0f
#define DT (1.0f / HZ)
#define D2R 0.0174532925f

/* the rigid body: R takes a vector of the controller's frame to the world (z up, y forward = the TV, x right) */
struct body {
    float R[3][3];
    float bias[3];   /* gyro bias of the sensor, rad/s */
    float lin[3];    /* linear acceleration in the world, g (a shake) */
};

static void mat_tvec(const float R[3][3], const float v[3], float o[3]) {
    for (int i = 0; i < 3; i++) o[i] = R[0][i] * v[0] + R[1][i] * v[1] + R[2][i] * v[2];
}

static void rodrigues(const float w[3], float dt, float out[3][3]) {
    float ang = sqrtf(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]) * dt;
    float k[3] = {0, 0, 0};
    float c, s, t;

    if (ang < 1e-9f) { memset(out, 0, 9 * sizeof(float)); out[0][0] = out[1][1] = out[2][2] = 1; return; }
    k[0] = w[0] * dt / ang; k[1] = w[1] * dt / ang; k[2] = w[2] * dt / ang;
    c = cosf(ang); s = sinf(ang); t = 1 - c;
    out[0][0] = t * k[0] * k[0] + c;        out[0][1] = t * k[0] * k[1] - s * k[2]; out[0][2] = t * k[0] * k[2] + s * k[1];
    out[1][0] = t * k[0] * k[1] + s * k[2]; out[1][1] = t * k[1] * k[1] + c;        out[1][2] = t * k[1] * k[2] - s * k[0];
    out[2][0] = t * k[0] * k[2] - s * k[1]; out[2][1] = t * k[1] * k[2] + s * k[0]; out[2][2] = t * k[2] * k[2] + c;
}

/* turn the body about a world axis by ang radians over `secs`, feeding the motion maths at 200 Hz */
static void step(struct iblue_motion *m, struct body *b, float dt, const float w_world[3]) {
    float wb[3], acc_w[3], a[3], g[3], dR[3][3], N[3][3];

    mat_tvec(b->R, w_world, wb);
    acc_w[0] = b->lin[0]; acc_w[1] = b->lin[1]; acc_w[2] = 1.0f + b->lin[2]; /* at rest the sensor reads +1 g up */
    mat_tvec(b->R, acc_w, a);
    for (int i = 0; i < 3; i++) g[i] = wb[i] + b->bias[i];
    iblue_motion_update(m, a, g, dt);
    /* the body turns: world rotation applied to R */
    rodrigues(w_world, dt, dR);
    for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) N[i][j] = dR[i][0] * b->R[0][j] + dR[i][1] * b->R[1][j] + dR[i][2] * b->R[2][j];
    memcpy(b->R, N, sizeof(N));
}

static void turn(struct iblue_motion *m, struct body *b, const float axis[3], float deg, float secs) {
    float n = secs * HZ;
    float w[3] = {axis[0] * deg * D2R / secs, axis[1] * deg * D2R / secs, axis[2] * deg * D2R / secs};

    for (int i = 0; i < (int)n; i++) step(m, b, DT, w);
}

static void hold(struct iblue_motion *m, struct body *b, float secs) {
    const float zero[3] = {0, 0, 0};

    for (int i = 0; i < (int)(secs * HZ); i++) step(m, b, DT, zero);
}

static const float AX_X[3] = {1, 0, 0};   /* world right: pitching up is a turn about it */
static const float AX_Z[3] = {0, 0, 1};   /* world up: yaw, counter-clockwise from above is positive */

/* the controller's long axis in its own frame (unknown to the maths until the poses are taken) */
static const float P_TRUE[3] = {0.28f, 0.84f, 0.46f};

/* orientation with the long axis pointing along the world's +y (the TV), level, in a random-looking mounting */
static void start_body(struct body *b) {
    float p[3] = {P_TRUE[0], P_TRUE[1], P_TRUE[2]};
    float n = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
    float x[3], y[3], z[3];
    float t[3] = {0, 0, 1};

    for (int i = 0; i < 3; i++) p[i] /= n;
    /* body frame: p is body-y (pointing); build a right-handed frame, then R's columns are the body axes in the world:
     * body vector v maps to world v0*bx + v1*by + v2*bz, and the long axis p maps to the world's +y */
    /* choose the world images of the body axes: e_p -> world +y; other two arbitrary but orthonormal */
    memset(b, 0, sizeof(*b));
    /* build an orthonormal basis (u1, u2, p) of the body frame, then map it to (world +x, world +z, world +y) */
    {
        float u1[3], u2[3], d;

        for (int i = 0; i < 3; i++) u1[i] = t[i];
        d = u1[0] * p[0] + u1[1] * p[1] + u1[2] * p[2];
        for (int i = 0; i < 3; i++) u1[i] -= d * p[i];
        n = sqrtf(u1[0] * u1[0] + u1[1] * u1[1] + u1[2] * u1[2]);
        for (int i = 0; i < 3; i++) u1[i] /= n;
        u2[0] = p[1] * u1[2] - p[2] * u1[1]; u2[1] = p[2] * u1[0] - p[0] * u1[2]; u2[2] = p[0] * u1[1] - p[1] * u1[0];
        /* R = [wx wy wz] * [u2 p u1]^T : the body vector u2 -> world x, p -> world y, u1 -> world z */
        for (int i = 0; i < 3; i++) {
            x[i] = u2[i]; y[i] = p[i]; z[i] = u1[i];
        }
        for (int i = 0; i < 3; i++) {
            b->R[0][i] = x[i];  /* world x component of a body vector v = x . v */
            b->R[1][i] = y[i];
            b->R[2][i] = z[i];
        }
    }
}

int main(void) {
    struct iblue_motion m;
    struct body b;

    /* the defaults are inside their ranges, an out of range setting is clamped */
    {
        struct iblue_motion_cfg c = iblue_motion_cfg_default;

        CHECK(iblue_motion_cfg_clamp(&c) == 0);
        c.el_range = 500; c.shake_mg = -5;
        CHECK(iblue_motion_cfg_clamp(&c) == 1 && c.el_range == iblue_motion_cfg_max.el_range && c.shake_mg == iblue_motion_cfg_min.shake_mg);
    }

    /* ---- calibration with the poses ---- */
    start_body(&b);
    iblue_motion_init(&m, NULL);
    m.cfg.filter_ms = 0;
    m.side = -1; /* a left controller */
    {
        float up_rate = 90.0f;

        /* not still enough yet: a pose is refused */
        CHECK(iblue_motion_pose(&m, IBLUE_MOTION_POSE_FORWARD) == -1);
        hold(&m, &b, 1.0f);
        CHECK(iblue_motion_pose(&m, IBLUE_MOTION_POSE_FORWARD) == 0);           /* pointing at the TV, level */
        /* pitch up 90 degrees (about the world x axis), hold, take the "up" pose */
        turn(&m, &b, AX_X, up_rate, 0.8f);
        hold(&m, &b, 0.8f);
        CHECK(iblue_motion_pose(&m, IBLUE_MOTION_POSE_UP) == 0);
        /* and 180 degrees back down */
        turn(&m, &b, AX_X, -180.0f, 1.2f);
        hold(&m, &b, 0.8f);
        CHECK(iblue_motion_pose(&m, IBLUE_MOTION_POSE_DOWN) == 0);
        /* back to level */
        turn(&m, &b, AX_X, 90.0f, 0.8f);
        hold(&m, &b, 0.8f);
    }
    {
        float d = m.axis[0] * P_TRUE[0] + m.axis[1] * P_TRUE[1] + m.axis[2] * P_TRUE[2];
        float nn = sqrtf(P_TRUE[0] * P_TRUE[0] + P_TRUE[1] * P_TRUE[1] + P_TRUE[2] * P_TRUE[2]);

        CHECK(m.axis_known == 1);
        CHECK(d / nn > 0.995f); /* the pointing axis was found from the poses */
    }
    NEAR(iblue_motion_elevation_deg(&m), 0.0f, 3.0f); /* back level: the middle */

    /* ---- height: 30 degrees up and 30 down give a third more than half of the range ---- */
    turn(&m, &b, AX_X, 30.0f, 0.5f);
    hold(&m, &b, 0.6f);
    NEAR(iblue_motion_elevation_deg(&m), 30.0f, 3.0f);
    NEAR(iblue_motion_y(&m), 30.0f / 45.0f, 0.07f);
    turn(&m, &b, AX_X, -60.0f, 0.8f);
    hold(&m, &b, 0.6f);
    NEAR(iblue_motion_y(&m), -30.0f / 45.0f, 0.07f);
    turn(&m, &b, AX_X, -60.0f, 0.8f); /* far below the range: it stops at the end */
    hold(&m, &b, 0.5f);
    NEAR(iblue_motion_y(&m), -1.0f, 0.001f);
    turn(&m, &b, AX_X, 90.0f, 0.8f);
    hold(&m, &b, 0.8f);
    NEAR(iblue_motion_y(&m), 0.0f, 0.07f);

    /* ---- a fast move is followed by the gyro, not only by the accelerometer: no waiting before the check ---- */
    turn(&m, &b, AX_X, 60.0f, 0.2f);
    NEAR(iblue_motion_elevation_deg(&m), 60.0f, 6.0f);
    turn(&m, &b, AX_X, -60.0f, 0.2f);
    NEAR(iblue_motion_elevation_deg(&m), 0.0f, 6.0f);
    hold(&m, &b, 0.8f);

    /* ---- sideways: out to its own side (left) 80 degrees sets the range and the direction ---- */
    CHECK(iblue_motion_pose(&m, IBLUE_MOTION_POSE_FORWARD) == 0);
    turn(&m, &b, AX_Z, 80.0f, 0.8f); /* counter-clockwise from above: to the left */
    hold(&m, &b, 0.8f);
    CHECK(iblue_motion_pose(&m, IBLUE_MOTION_POSE_OUT) == 0);
    NEAR(m.yaw_range_deg, 80.0f, 6.0f);
    NEAR(iblue_motion_x(&m), -1.0f, 0.08f); /* all the way left */
    turn(&m, &b, AX_Z, -80.0f, 0.8f);       /* back to forward */
    hold(&m, &b, 0.6f);
    NEAR(iblue_motion_x(&m), 0.0f, 0.08f);
    turn(&m, &b, AX_Z, -40.0f, 0.5f);       /* half way to the right */
    hold(&m, &b, 0.6f);
    NEAR(iblue_motion_x(&m), 0.5f, 0.1f);

    /* ---- the gyro bias is learned while the controller lies still: no drift ---- */
    start_body(&b);
    iblue_motion_init(&m, NULL);
    m.cfg.filter_ms = 0;
    b.bias[0] = 0.02f; b.bias[1] = -0.015f; b.bias[2] = 0.025f;
    hold(&m, &b, 4.0f);
    iblue_motion_pose(&m, IBLUE_MOTION_POSE_FORWARD);
    hold(&m, &b, 20.0f);
    NEAR(iblue_motion_heading_deg(&m), 0.0f, 2.0f);

    /* ---- shake: one burst is one press, the lockout stops a second one, the next shake later counts ---- */
    start_body(&b);
    iblue_motion_init(&m, NULL);
    hold(&m, &b, 1.0f);
    CHECK(m.shakes == 0 && iblue_motion_shake(&m) == 0);
    b.lin[2] = 2.0f; /* a jolt of 2 g */
    for (int i = 0; i < 6; i++) step(&m, &b, DT, (const float[3]){0, 0, 0});
    b.lin[2] = 0.0f;
    CHECK(m.shakes == 1);
    CHECK(iblue_motion_shake(&m) == 1); /* pressed */
    hold(&m, &b, 0.12f);
    CHECK(iblue_motion_shake(&m) == 0); /* released after shake_ms */
    b.lin[2] = -2.0f;
    for (int i = 0; i < 6; i++) step(&m, &b, DT, (const float[3]){0, 0, 0});  /* too soon: locked out */
    b.lin[2] = 0.0f;
    CHECK(m.shakes == 1);
    hold(&m, &b, 0.6f);
    b.lin[2] = 2.0f;
    for (int i = 0; i < 6; i++) step(&m, &b, DT, (const float[3]){0, 0, 0});
    b.lin[2] = 0.0f;
    CHECK(m.shakes == 2);
    /* a slow tilt is not a shake */
    {
        int before = (int)m.shakes;

        hold(&m, &b, 1.0f);
        iblue_motion_pose(&m, IBLUE_MOTION_POSE_FORWARD);
        turn(&m, &b, AX_X, 80.0f, 1.0f);
        turn(&m, &b, AX_X, -80.0f, 1.0f);
        CHECK((int)m.shakes == before);
    }

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
