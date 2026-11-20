/* Host tests of the emulated Dreamcast microphone (main/wired/maple_mic.c). */
#include "tcommon.h"
#include "wired/maple_mic.h"

static uint32_t w[160];
static uint8_t rsp;

static uint32_t ctl(uint32_t sub, uint32_t dt1, uint32_t now_us) {
    memset(w, 0xAA, sizeof(w));
    return maple_mic_control(0, MAPLE_MIC_FUNC, (sub << 24) | (dt1 << 16), now_us, w, &rsp);
}

/* the text of n words, with each word's bytes put back in reading order */
static void text(const uint32_t *src, uint32_t n, char *out) {
    const uint8_t *b = (const uint8_t *)src;

    for (uint32_t i = 0; i < n * 4; i += 4) {
        out[i] = (char)b[i + 3];
        out[i + 1] = (char)b[i + 2];
        out[i + 2] = (char)b[i + 1];
        out[i + 3] = (char)b[i];
    }
    out[n * 4] = 0;
}

/* sample i of a 16-bit answer, as packed by maple_mic.c */
static int16_t pcm(uint32_t i) {
    uint32_t v = w[2 + i / 2];
    uint32_t lo = (i & 1) ? (v >> 8) & 0xFF : v >> 24;
    uint32_t hi = (i & 1) ? v & 0xFF : (v >> 16) & 0xFF;

    return (int16_t)(lo | (hi << 8));
}

int main(void) {
    char t[200];
    uint32_t n;

    /* device info */
    n = maple_mic_info(w, 0);
    CHECK(n == 28 && w[0] == 0x10 && w[1] == 0x3F000000 && w[2] == 0 && w[3] == 0);
    text(&w[4], 8, t);
    CHECK((uint8_t)t[0] == 0xFF && t[1] == 0);
    CHECK(memcmp(t + 2, "MicDevice for Dreamcast       ", 30) == 0);
    n = maple_mic_info(w, 1);
    text(&w[28], 30, t);
    CHECK(n == 58 && memcmp(t, "Version 1.000,2000/02/24,315-6182A", 34) == 0);

    /* mu-law (G.711) */
    CHECK(maple_mic_ulaw(0) == 0xFF);
    CHECK(maple_mic_ulaw(32767) == 0x80);
    CHECK(maple_mic_ulaw(-32768) == 0x00);
    CHECK(maple_mic_ulaw(-1) == 0x7F);

    maple_mic_reset(0);

    /* wrong function code, unknown subcommand */
    n = maple_mic_control(0, 0x00000001, 0x01000000, 0, w, &rsp);
    CHECK(n == 0 && rsp == MAPLE_MIC_RSP_NO_FUNC);
    CHECK(ctl(0x00, 0, 0) == 0 && rsp == MAPLE_MIC_RSP_UNKNOWN);
    CHECK(ctl(0x06, 0, 0) == 0 && rsp == MAPLE_MIC_RSP_UNKNOWN);

    /* not sampling: a data frame without samples */
    n = ctl(MAPLE_MIC_SUB_GET_SAMPLES, 0x0F, 1000);
    CHECK(n == 2 && rsp == MAPLE_MIC_RSP_DATA && w[0] == 0x10 && w[1] == 0x000F0000);

    /* settings */
    CHECK(ctl(MAPLE_MIC_SUB_AMP_GAIN, 0x20, 0) == 0 && rsp == MAPLE_MIC_RSP_ACK);
    CHECK(ctl(MAPLE_MIC_SUB_EXTU_BIT, 3, 0) == 0 && rsp == MAPLE_MIC_RSP_UNKNOWN);
    CHECK(ctl(MAPLE_MIC_SUB_EXTU_BIT, 1, 0) == 0 && rsp == MAPLE_MIC_RSP_ACK);
    CHECK(ctl(MAPLE_MIC_SUB_VOLUME, 2, 0) == 0 && rsp == MAPLE_MIC_RSP_UNKNOWN);
    CHECK(ctl(MAPLE_MIC_SUB_VOLUME, 1, 0) == 0 && rsp == MAPLE_MIC_RSP_ACK);
    CHECK(ctl(MAPLE_MIC_SUB_BASIC, 0x90, 0) == 0 && rsp == MAPLE_MIC_RSP_UNKNOWN); /* bit 4 must be 0 */
    CHECK(ctl(MAPLE_MIC_SUB_BASIC, 0x88, 0) == 0 && rsp == MAPLE_MIC_RSP_UNKNOWN); /* frequency level 2 does not exist */

    /* 16-bit at 11.025 kHz: 10 ms gives 110 samples */
    CHECK(ctl(MAPLE_MIC_SUB_BASIC, 0x80, 100000) == 0 && rsp == MAPLE_MIC_RSP_ACK);
    n = ctl(MAPLE_MIC_SUB_GET_SAMPLES, 0x0F, 110000);
    CHECKM(n == 2 + 55 && rsp == MAPLE_MIC_RSP_DATA, "n %u", n);
    CHECKM(w[1] == ((uint32_t)MAPLE_MIC_ST_SAMPLING << 24 | 0x0F << 16 | 110), "status word %08X", w[1]);
    {
        int16_t lo = 0, hi = 0;
        int crossings = 0;

        CHECK(pcm(0) == 0);
        for (uint32_t i = 0; i < 110; i++) {
            int16_t s = pcm(i);

            lo = s < lo ? s : lo;
            hi = s > hi ? s : hi;
            if (i && (pcm(i - 1) < 0) != (s < 0)) {
                crossings++;
            }
        }
        /* 440 Hz at about -12 dBFS: 10 ms is 4.4 periods, about 8 zero crossings */
        CHECKM(hi > 7900 && hi <= 8192 && lo < -7900 && lo >= -8192, "range %d..%d", lo, hi);
        CHECKM(crossings >= 7 && crossings <= 10, "crossings %d", crossings);
    }

    /* a fast poll: the fraction carries over, 1 ms later 11 samples */
    n = ctl(MAPLE_MIC_SUB_GET_SAMPLES, 0x0F, 111000);
    CHECKM((w[1] & 0xFFFF) == 11 && n == 2 + 6, "count %u n %u", w[1] & 0xFFFF, n);

    /* at most 240 per answer, the rest waits */
    n = ctl(MAPLE_MIC_SUB_GET_SAMPLES, 0x0F, 141000); /* 30 ms: 330.75 samples plus the earlier fractions: 331 */
    CHECK((w[1] & 0xFFFF) == 240 && n == 2 + 120);
    n = ctl(MAPLE_MIC_SUB_GET_SAMPLES, 0x0F, 141000);
    CHECKM((w[1] & 0xFFFF) == 91, "rest %u", w[1] & 0xFFFF);

    /* a long gap overflows the device buffer, reported once */
    ctl(MAPLE_MIC_SUB_GET_SAMPLES, 0x0F, 541000);
    CHECK(w[1] >> 24 == (MAPLE_MIC_ST_SAMPLING | MAPLE_MIC_ST_OVERFLOW));
    ctl(MAPLE_MIC_SUB_GET_SAMPLES, 0x0F, 542000);
    CHECK(w[1] >> 24 == MAPLE_MIC_ST_SAMPLING);

    /* gain 0 silences the tone */
    ctl(MAPLE_MIC_SUB_GET_SAMPLES, 0x00, 552000);
    {
        int quiet = 1;

        for (uint32_t i = 0; i < (w[1] & 0xFFFF); i++) {
            quiet &= pcm(i) == 0;
        }
        CHECK(quiet && (w[1] & 0xFFFF) > 0);
    }

    /* 8 kHz mu-law: 30 ms is 240 samples in 60 words; status says 8 kHz, mu-law, sampling */
    CHECK(ctl(MAPLE_MIC_SUB_BASIC, 0x85, 600000) == 0 && rsp == MAPLE_MIC_RSP_ACK);
    n = ctl(MAPLE_MIC_SUB_GET_SAMPLES, 0x0F, 630000);
    CHECKM(n == 2 + 60 && w[1] == ((uint32_t)(MAPLE_MIC_ST_SAMPLING | MAPLE_MIC_ST_ULAW | MAPLE_MIC_ST_FS_8K) << 24 | 0x0F << 16 | 240),
        "n %u status %08X", n, w[1]);

    /* stop: no more samples */
    CHECK(ctl(MAPLE_MIC_SUB_BASIC, 0x05, 640000) == 0 && rsp == MAPLE_MIC_RSP_ACK);
    n = ctl(MAPLE_MIC_SUB_GET_SAMPLES, 0x0F, 700000);
    CHECK(n == 2 && w[1] == ((uint32_t)(MAPLE_MIC_ST_ULAW | MAPLE_MIC_ST_FS_8K) << 24 | 0x0F << 16));

    printf("mic: %d checks, %d failed\n", checks, fails);
    return fails != 0;
}
