/*
 * iBlueControlMod: an emulated Dreamcast microphone (FT4). See maple_mic.h.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <esp_attr.h>
#include "maple_mic.h"

/*
 * How 16-bit samples sit in a Maple word: the first sample's low byte is the word's top byte, then its high
 * byte, then the second sample's low and high byte. This is what the DreamColor dongle sends (its Maple-side
 * handler packs bytes s0lo, s0hi, s1lo, s1hi into one word, top byte first). The Sega spec does not say.
 * If a game hears noise instead of the tone, swap the two bytes of each sample here.
 */
#define PACK16(a, b) (((uint32_t)((uint16_t)(a) & 0xFF) << 24) | ((uint32_t)((uint16_t)(a) >> 8) << 16) | \
                      ((uint32_t)((uint16_t)(b) & 0xFF) << 8) | ((uint32_t)((uint16_t)(b) >> 8)))
#define PACK8(a, b, c, d) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

/* Function definition block 1: Get_Sampling_Data, Basic_Control, AMP_GAIN, EXTU_BIT, Volume_Mode and the fixed
 * bit (FD10 to FD15, FT4 spec table 2-2). Not yet compared with a real Sega microphone. */
#define MIC_FUNC_DATA 0x3F000000

/* Standby and maximum current in 0.1 mA, packed like PWR_* in maple.c. Approximate (no real unit measured). */
#define MIC_STANDBY 0x0069
#define MIC_MAX     0x0120
#define SWAP16(x)   ((uint16_t)((((x) & 0xFF) << 8) | (((x) >> 8) & 0xFF)))
#define MIC_PWR     (((uint32_t)SWAP16(MIC_STANDBY) << 16) | SWAP16(MIC_MAX))

#define TONE_HZ 440

struct mic_state {
    uint8_t mode;     /* Basic_Control DT1 as last accepted */
    uint8_t gain;
    uint8_t extu;     /* EXTU_BIT mode 0..2 */
    uint8_t volume;   /* Volume_Mode 0..1 (no effect on the test tone) */
    uint8_t overflow;
    uint32_t last_us;
    uint32_t acc;     /* fractional samples, in units of 1/1000000 sample */
    uint32_t pending; /* samples "recorded" since the last poll */
    uint32_t phase;   /* test tone */
};

static DRAM_ATTR struct mic_state mic[MAPLE_MIC_PORTS];
static DRAM_ATTR int16_t samples[MAPLE_MIC_MAX_SAMPLES]; /* one answer's samples; the Maple interrupt is not reentrant */

/* sin(0..90 degrees) * 32767, 65 points */
static DRAM_ATTR const int16_t quarter_sine[65] = {
        0,   804,  1608,  2410,  3212,  4011,  4808,  5602,  6393,  7179,  7962,  8739,  9512,
    10278, 11039, 11793, 12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868,
    19519, 20159, 20787, 21403, 22005, 22594, 23170, 23731, 24279, 24811, 25329, 25832, 26319,
    26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956, 30273, 30571, 30852, 31113,
    31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757, 32767,
};

/* Area code FF (all regions), connector direction 0, then the 30 character name, as the real microphone */
static DRAM_ATTR const char mic_name[32] = "\xFF\x00" "MicDevice for Dreamcast       ";
/* The version text a Sega microphone reports in "all info" (as seen in the DreamColor dongle), 120 bytes */
static DRAM_ATTR const char mic_version[121] =
    "Version 1.000,2000/02/24,315-6182A   SoundSamplingSystem/SamplingRate10.9990or8.0000kHz/Bit14or8bit/NTSC/PAL/Consumer   ";

/* Text on the Maple bus travels with each word's bytes reversed */
static void IRAM_ATTR put_text(uint32_t *w, const char *s, uint32_t len) {
    uint8_t *d = (uint8_t *)w;

    for (uint32_t i = 0; i < len; i += 4) {
        d[i + 0] = (uint8_t)s[i + 3];
        d[i + 1] = (uint8_t)s[i + 2];
        d[i + 2] = (uint8_t)s[i + 1];
        d[i + 3] = (uint8_t)s[i + 0];
    }
}

static inline uint32_t IRAM_ATTR mic_rate(uint8_t mode) {
    return (mode & MAPLE_MIC_MODE_FS_8K) ? 8000 : 11025;
}

void IRAM_ATTR maple_mic_reset(uint32_t port) {
    if (port >= MAPLE_MIC_PORTS) {
        return;
    }
    memset(&mic[port], 0, sizeof(mic[port]));
    mic[port].gain = MAPLE_MIC_DEFAULT_GAIN;
}

uint32_t IRAM_ATTR maple_mic_info(uint32_t *w, uint32_t ext) {
    w[0] = MAPLE_MIC_FUNC;
    w[1] = MIC_FUNC_DATA;
    w[2] = 0;
    w[3] = 0;
    put_text(&w[4], mic_name, 32);
    w[27] = MIC_PWR;
    if (ext) {
        put_text(&w[28], mic_version, 120);
        return 28 + 30;
    }
    return 28;
}

uint8_t IRAM_ATTR maple_mic_ulaw(int16_t s) {
    int32_t mag = s;
    uint8_t sign = 0;
    uint8_t exp = 7;

    if (mag < 0) {
        sign = 0x80;
        mag = -mag;
    }
    if (mag > 32635) {
        mag = 32635;
    }
    mag += 132;
    for (uint32_t mask = 0x4000; !(mag & mask) && exp; mask >>= 1) {
        --exp;
    }
    return (uint8_t)~(sign | (exp << 4) | ((mag >> (exp + 3)) & 0x0F));
}

void IRAM_ATTR maple_mic_fill(uint32_t port, int16_t *dst, uint32_t n, uint32_t rate) {
    const uint32_t step = (uint32_t)(((uint64_t)TONE_HZ << 32) / rate);
    uint32_t phase = mic[port].phase;

    for (uint32_t i = 0; i < n; i++, phase += step) {
        uint32_t idx = phase >> 24;      /* 256 steps per period */
        uint32_t q = idx >> 6, k = idx & 63;
        int32_t v = quarter_sine[(q & 1) ? 64 - k : k];

        dst[i] = (int16_t)(((q & 2) ? -v : v) / 4); /* about -12 dBFS */
    }
    mic[port].phase = phase;
}

/* How many samples the device "recorded" since the last poll; sets the overflow flag past its buffer. */
static uint32_t IRAM_ATTR mic_take(struct mic_state *m, uint32_t now_us) {
    uint32_t dt = now_us - m->last_us;
    uint32_t n;

    m->last_us = now_us;
    if (dt > 200000) {
        dt = 200000; /* 200 ms is already past the buffer at either rate; keeps dt * rate within 32 bits */
    }
    m->acc += dt * mic_rate(m->mode);
    m->pending += m->acc / 1000000;
    m->acc %= 1000000;
    if (m->pending > MAPLE_MIC_BUF_SAMPLES) {
        m->pending = MAPLE_MIC_BUF_SAMPLES;
        m->overflow = 1;
    }
    n = m->pending > MAPLE_MIC_MAX_SAMPLES ? MAPLE_MIC_MAX_SAMPLES : m->pending;
    m->pending -= n;
    return n;
}

static int16_t IRAM_ATTR mic_post(int16_t s, uint8_t gain, uint8_t extu) {
    int32_t v = (int32_t)s * gain / 15;

    if (v > 32767) {
        v = 32767;
    }
    else if (v < -32768) {
        v = -32768;
    }
    if (extu == 0) {
        v &= ~3;
    }
    else if (extu == 2) {
        v = (v & ~3) | 2;
    }
    return (int16_t)v;
}

uint32_t IRAM_ATTR maple_mic_control(uint32_t port, uint32_t func, uint32_t arg, uint32_t now_us, uint32_t *w, uint8_t *rsp) {
    struct mic_state *m;
    uint8_t sub = arg >> 24, dt1 = (arg >> 16) & 0xFF;

    if (port >= MAPLE_MIC_PORTS) {
        *rsp = MAPLE_MIC_RSP_UNKNOWN;
        return 0;
    }
    if (func != MAPLE_MIC_FUNC) {
        *rsp = MAPLE_MIC_RSP_NO_FUNC;
        return 0;
    }
    m = &mic[port];
    *rsp = MAPLE_MIC_RSP_ACK;
    switch (sub) {
        case MAPLE_MIC_SUB_GET_SAMPLES:
        {
            int16_t *s = samples;
            uint32_t n = 0, words;
            uint8_t st = 0;

            m->gain = dt1;
            if (m->mode & MAPLE_MIC_MODE_SAMPLING) {
                n = mic_take(m, now_us);
                st |= MAPLE_MIC_ST_SAMPLING;
            }
            if (m->mode & MAPLE_MIC_MODE_FS_8K) {
                st |= MAPLE_MIC_ST_FS_8K;
            }
            if (m->mode & MAPLE_MIC_MODE_ULAW) {
                st |= MAPLE_MIC_ST_ULAW;
            }
            if (m->overflow) {
                st |= MAPLE_MIC_ST_OVERFLOW;
                m->overflow = 0;
            }
            maple_mic_fill(port, s, n, mic_rate(m->mode));
            for (uint32_t i = 0; i < n; i++) {
                s[i] = mic_post(s[i], m->gain, m->extu);
            }
            w[0] = MAPLE_MIC_FUNC;
            w[1] = ((uint32_t)st << 24) | ((uint32_t)m->gain << 16) | n; /* status, gain, expansion data 0, count */
            if (m->mode & MAPLE_MIC_MODE_ULAW) {
                words = (n + 3) / 4;
                for (uint32_t i = 0; i < words; i++) {
                    uint8_t b[4] = {0xFF, 0xFF, 0xFF, 0xFF}; /* mu-law silence pads the last word */

                    for (uint32_t j = 0; j < 4 && i * 4 + j < n; j++) {
                        b[j] = maple_mic_ulaw(s[i * 4 + j]);
                    }
                    w[2 + i] = PACK8(b[0], b[1], b[2], b[3]);
                }
            }
            else {
                words = (n + 1) / 2;
                for (uint32_t i = 0; i < words; i++) {
                    w[2 + i] = PACK16(s[i * 2], (i * 2 + 1 < n) ? s[i * 2 + 1] : 0);
                }
            }
            *rsp = MAPLE_MIC_RSP_DATA;
            return 2 + words;
        }
        case MAPLE_MIC_SUB_BASIC:
            /* bits 6-4 must be 0; only 11.025/8 kHz and 16-bit/mu-law exist */
            if ((dt1 & 0x70) || (dt1 & 0x08) || (dt1 & 0x02)) {
                *rsp = MAPLE_MIC_RSP_UNKNOWN;
                return 0;
            }
            if ((dt1 & MAPLE_MIC_MODE_SAMPLING) && (!(m->mode & MAPLE_MIC_MODE_SAMPLING) ||
                    ((dt1 ^ m->mode) & MAPLE_MIC_MODE_FS_8K))) {
                m->last_us = now_us; /* sampling starts now: the buffer is empty */
                m->acc = 0;
                m->pending = 0;
                m->overflow = 0;
            }
            m->mode = dt1;
            return 0;
        case MAPLE_MIC_SUB_AMP_GAIN:
            m->gain = dt1;
            return 0;
        case MAPLE_MIC_SUB_EXTU_BIT:
            if (dt1 > 2) {
                *rsp = MAPLE_MIC_RSP_UNKNOWN; /* the previous setting is kept */
                return 0;
            }
            m->extu = dt1;
            return 0;
        case MAPLE_MIC_SUB_VOLUME:
            if (dt1 > 1) {
                *rsp = MAPLE_MIC_RSP_UNKNOWN;
                return 0;
            }
            m->volume = dt1;
            return 0;
        case MAPLE_MIC_SUB_TEST:
            return 0; /* debugging only, the format is per device: acknowledge */
        default:
            *rsp = MAPLE_MIC_RSP_UNKNOWN;
            return 0;
    }
}
