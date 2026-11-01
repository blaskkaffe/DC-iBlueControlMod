/* iBlueControlMod: boot animation maths (water ripples). See iblue_anim_curve.h. */

#include <math.h>
#include "iblue/iblue_anim_curve.h"

#define GAMMA 2.2f          /* LED brightness is not linear in duty */
#define WIDTH_DEFAULT 4.0f  /* the tank of the default settings: LED i sits at i + 0.5, the walls at 0 and 4 */
#define SPEED 1.6f          /* ripple speed, LED spacings per second */
#define WAVELENGTH 2.4f     /* crest to crest */
#define FRONT 0.45f         /* how sharp the leading edge is */
#define TRAIL 1.7f         /* how long the trailing ripples last behind the front */
#define NEAR 3.0f          /* distance from which the amplitude falls with the square root of it */
#define DAMPING_S 4.5f      /* the ripples die out with this time constant */
/* the water level curve (REST_LOW, REST_HIGH, RISE_AT, RISE_LEN, FADE_AT) and more are in the settings */
#define SETTLE 0.16f        /* the light comes in over this part of the animation */
#define GAIN 1.7f           /* the ripples are drawn a bit larger than life */
#define SPLASH_S 0.25f      /* time for the splash to build up */
#define TWO_PI 6.2831853f
#define IMAGES 2            /* mirror images of a drop on each side: the walls */

/* total_s ctrl led_dist tank_len group_pos density depth low high rise_at rise_len fade_at */
const struct iblue_anim_cfg iblue_anim_cfg_default = {15, 20, 100, 400, 0, 100, 100, 20, 42, 30, 20, 45};
const struct iblue_anim_cfg iblue_anim_cfg_min     = { 5,  0,  30, 100, -500, 30,  20,  0,  0,  0,  1, 10};
const struct iblue_anim_cfg iblue_anim_cfg_max     = {60, 100, 300, 1500, 500, 300, 200, 100, 100, 90, 60, 95};

static struct iblue_anim_cfg cfg_store[2] = {
    {15, 20, 100, 400, 0, 100, 100, 20, 42, 30, 20, 45},
    {15, 20, 100, 400, 0, 100, 100, 20, 42, 30, 20, 45},
};
static volatile int cfg_cur;

int iblue_anim_cfg_clamp(struct iblue_anim_cfg *c) {
    int16_t *v = (int16_t *)c;
    const int16_t *lo = (const int16_t *)&iblue_anim_cfg_min;
    const int16_t *hi = (const int16_t *)&iblue_anim_cfg_max;
    int changed = 0;

    for (int i = 0; i < IBLUE_ANIM_CFG_FIELDS; i++) {
        if (v[i] < lo[i]) { v[i] = lo[i]; changed = 1; }
        if (v[i] > hi[i]) { v[i] = hi[i]; changed = 1; }
    }
    return changed;
}

void iblue_anim_set_cfg(const struct iblue_anim_cfg *c) {
    int next = cfg_cur ^ 1;

    cfg_store[next] = *c;
    iblue_anim_cfg_clamp(&cfg_store[next]);
    cfg_cur = next; /* the frame in progress finishes with the old one */
}

const struct iblue_anim_cfg *iblue_anim_get_cfg(void) {
    return &cfg_store[cfg_cur];
}

/* the settings as the numbers the maths use */
struct params {
    float width;     /* tank length, LED spacings */
    float dist;      /* LED spacing */
    float x0;        /* where LED A sits */
    float speed;     /* ripple speed, LED spacings per second */
    float damping_s; /* the ripples die out with this time constant */
    float depth;     /* brightness of the water */
    float low, high, rise_at, rise_len, fade_at;
};

static void get_params(struct params *p) {
    const struct iblue_anim_cfg *c = iblue_anim_get_cfg();
    float rho = (float)c->density / 100.0f;

    p->width = (float)c->tank_len / 100.0f;
    p->dist = (float)c->led_dist / 100.0f;
    p->x0 = (p->width - 3.0f * p->dist) / 2.0f + (float)c->group_pos / 100.0f;
    p->speed = SPEED / rho;
    p->damping_s = DAMPING_S / rho;
    p->depth = (float)c->depth / 100.0f;
    p->low = (float)c->level_low / 100.0f;
    p->high = (float)c->level_high / 100.0f;
    p->rise_at = (float)c->rise_at / 100.0f;
    p->rise_len = (float)c->rise_len / 100.0f;
    p->fade_at = (float)c->fade_at / 100.0f;
}

/* where LED i is in the tank (never outside the walls) */
static float led_x(const struct params *p, int i) {
    float x = p->x0 + (float)i * p->dist;

    return x < 0.02f ? 0.02f : (x > p->width - 0.02f ? p->width - 0.02f : x);
}

struct drop {
    float at;   /* time of the impact, seconds */
    float x;    /* where, 0..WIDTH */
    float amp;
    float soft; /* 1 = as the others; more = a broader, slower swell: longer wavelength, wider front */
};

#define FIRST_DROP 0.04f    /* the first drop falls at this fraction of the animation */
#define DROP_GAP_S 1.74f    /* seconds between drops */
#define LAST_DROP 0.75f     /* no new drop after this fraction of the animation */
/* Where the drops fall: the first in the middle, then to either side */
static const float drop_x[] = {2.00f, 0.85f, 3.20f, 1.30f, 2.85f, 0.55f, 3.45f};

/* Drop k of an animation of total_s seconds, 0 if there is no such drop. The first one is the big, slow
 * swell; the others follow at a steady pace. A longer animation just gets more of them. */
static int drop_k(int k, float total_s, struct drop *d) {
    d->at = FIRST_DROP * total_s + (float)k * DROP_GAP_S;
    if (d->at > LAST_DROP * total_s) {
        return 0;
    }
    d->x = drop_x[k % (int)(sizeof(drop_x) / sizeof(drop_x[0]))];
    d->amp = k == 0 ? 0.70f : (k % 2 ? 0.90f : 0.95f);
    d->soft = k == 0 ? 1.5f : 1.0f;
    return 1;
}

static float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
static float smooth(float x) { x = clamp01(x); return x * x * (3.0f - 2.0f * x); }
/* gentler at both ends than smooth() */
static float smoother(float x) { x = clamp01(x); return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f); }

/* Distance to the impact, rounded off so the water has no sharp point right where the drop hits */
static float dist(float d) { return sqrtf(d * d + 0.04f); }

/* Height of one ring ripple at distance r, `age` seconds after the impact. s is how far the point
 * is behind the front (negative: the front has not arrived yet). */
static float ripple(const struct params *p, float r, float age, float soft) {
    float s = r - p->speed * age;
    float front = FRONT * soft;
    float trail = TRAIL * soft;
    float env = s > 0.0f ? expf(-(s / front) * (s / front)) : expf(-(s / trail) * (s / trail));
    float spread = 1.0f / sqrtf(1.0f + r / NEAR);
    /* the splash builds up over a moment, so the water does not jump at the impact */
    return cosf(TWO_PI * s / (WAVELENGTH * soft)) * env * spread * expf(-age / p->damping_s) * smooth(age / (SPLASH_S * soft));
}

/* Height of the water at x, t seconds into the animation */
static float height(const struct params *p, float x, float t, float total_s) {
    float h = 0.0f;
    struct drop dr;

    for (int i = 0; drop_k(i, total_s, &dr); i++) {
        float age = t - dr.at;

        if (age < 0.0f) {
            continue;
        }
        /* the drop and its mirror images in the two walls */
        for (int k = -IMAGES; k <= IMAGES; k++) {
            float dx = dr.x * p->width / WIDTH_DEFAULT; /* the drops fall at the same places of a longer or shorter tank */
            float a = 2.0f * p->width * (float)k + dx;
            float b = 2.0f * p->width * (float)k - dx;

            h += dr.amp * ripple(p, dist(x - a), age, dr.soft);
            h += dr.amp * ripple(p, dist(x - b), age, dr.soft);
        }
    }
    return h;
}

float iblue_anim_height(float x, uint32_t elapsed_ms, uint32_t total_ms) {
    struct params p;

    get_params(&p);
    return height(&p, x, (float)elapsed_ms / 1000.0f, (float)total_ms / 1000.0f);
}

/* Level of the calm water: the start level, up to the raised level where the curve of the settings says */
static float rest_level(const struct params *p, float u) {
    return p->low + (p->high - p->low) * smoother((u - p->rise_at) / p->rise_len);
}

/* ... and the whole thing eased in at the start and out at the end */
static float master(const struct params *p, float u) {
    return clamp01(smoother(u / SETTLE) * (1.0f - smoother((u - p->fade_at) / (1.0f - p->fade_at))));
}

float iblue_anim_calm(float u) {
    struct params p;

    get_params(&p);
    return rest_level(&p, u) * master(&p, u);
}

uint32_t iblue_anim_duty(uint32_t elapsed_ms, uint32_t total_ms, int led) {
    struct params p;
    float u, t, rest, v;

    if (total_ms == 0 || elapsed_ms >= total_ms || led < 0 || led >= IBLUE_ANIM_LEDS) {
        return 0;
    }
    get_params(&p);
    u = (float)elapsed_ms / (float)total_ms; /* 0..1 over the animation */
    t = (float)elapsed_ms / 1000.0f;
    rest = rest_level(&p, u);
    /* height of the water is the brightness: calm water glows, a crest is bright, a trough dark; the depth
     * of the water scales all of it */
    v = clamp01(p.depth * (rest + (1.0f - rest) * GAIN * height(&p, led_x(&p, led), t, (float)total_ms / 1000.0f)));
    v *= master(&p, u);
    return (uint32_t)(powf(v, GAMMA) * (float)IBLUE_ANIM_MAX + 0.5f);
}

/* The gaps start at 350 ms and each is 0.8775 of the one before (the last is 49 ms): sixteen blinks, then the
 * boom at 2504 */
static const uint16_t blink_at[IBLUE_ANIM_INTRO_BLINKS] = {0, 350, 657, 927, 1163, 1371, 1553, 1713, 1853, 1976, 2084, 2178, 2262, 2335, 2399, 2455};
/* A first (it is lit already), then sparks all over, never the same LED twice in a row */
static const uint8_t blink_led[IBLUE_ANIM_INTRO_BLINKS] = {0, 2, 1, 3, 1, 0, 3, 2, 0, 1, 3, 0, 2, 3, 1, 0};

uint32_t iblue_anim_intro_blink_at(int k) {
    return (k >= 0 && k < IBLUE_ANIM_INTRO_BLINKS) ? blink_at[k] : 0;
}

int iblue_anim_intro_blink_led(int k) {
    return (k >= 0 && k < IBLUE_ANIM_INTRO_BLINKS) ? blink_led[k] : 0;
}

uint32_t iblue_anim_intro_duty(uint32_t ms, int led) {
    uint32_t boom = IBLUE_ANIM_INTRO_BOOM_AT_MS;
    float level;

    if (led < 0 || led >= IBLUE_ANIM_LEDS) {
        return 0;
    }
    if (ms < boom) {
        for (int k = 0; k < IBLUE_ANIM_INTRO_BLINKS; k++) {
            if (blink_led[k] == led && ms >= (uint32_t)blink_at[k] && ms < (uint32_t)blink_at[k] + IBLUE_ANIM_INTRO_BLINK_MS) {
                return IBLUE_ANIM_MAX;
            }
        }
        return 0;
    }
    if (ms < boom + IBLUE_ANIM_INTRO_BOOM_HOLD_MS) {
        return IBLUE_ANIM_MAX; /* boom: all four */
    }
    if (ms >= IBLUE_ANIM_INTRO_MS) {
        return 0;
    }
    level = 1.0f - (float)(ms - boom - IBLUE_ANIM_INTRO_BOOM_HOLD_MS) / (float)IBLUE_ANIM_INTRO_BOOM_FADE_MS; /* what the eye sees */
    return (uint32_t)(powf(level, GAMMA) * (float)IBLUE_ANIM_MAX + 0.5f);
}
