/* Property tests of system/iblue_mapcfg.c: round trips, render caps, replace, layouts, the deadzone codec. See run.sh.
 * Everything here must pass today; behaviour that is wrong lives in test_bugs.c. */
#include "tcommon.h"
#include "system/iblue_mapcfg.h"

#define MAXE 128
typedef struct iblue_mapent ent_t;

static uint32_t names[32], nnames;

static const char *const optsets[] = {
    "", ":deadzone=off", ":deadzone=3", ":deadzone=17%", ":deadzone=1.5%", ":deadzone=0.01%", ":deadzone=57%", ":threshold=0",
    ":threshold=100", ":scale=0", ":scale=255", ":turbo=255", ":algo=aggressive", ":algo=SCURVE",
    ":deadzone=2:threshold=7:scale=9:turbo=1:algo=wide", ":algo=relaxed:turbo=3", ":deadzone=5:deadzone=off",
};
#define NOPT (sizeof(optsets) / sizeof(optsets[0]))

/* every output x every combination of 0..4 distinct inputs, options rotating: text -> parse -> render -> parse -> render */
static void roundtrip_all(void) {
    uint64_t cases = 0, opt_i = 0;
    uint32_t idx[4];

    for (uint32_t out = 0; out < IBLUE_OUT_COUNT; out++) {
        for (uint32_t k = 0; k <= 4; k++) {
            /* iterate k-combinations of nnames in lexicographic order */
            for (uint32_t i = 0; i < k; i++) { idx[i] = i; }
            for (;;) {
                char text[300], again[300], third[300];
                ent_t e1[4], e2[4];
                const char *why = "";
                size_t tl = 0;
                int c1, c2;

                for (uint32_t i = 0; i < k; i++) {
                    const char *nm = iblue_in_name((uint8_t)names[idx[i]]);
                    const char *opt = optsets[(opt_i++) % NOPT];
                    if (i) { tl += (size_t)snprintf(text + tl, sizeof(text) - tl, " , "); }
                    tl += (size_t)snprintf(text + tl, sizeof(text) - tl, "%s%s", nm, opt);
                }
                text[tl] = 0;
                memset(e1, 0xEE, sizeof(e1));
                memset(e2, 0xEE, sizeof(e2));
                c1 = iblue_map_parse(text, tl, out, 1, e1, &why);
                CHECKM(c1 == (int)k, "parse [%s] -> %d (%s)", text, c1, why);
                if (c1 != (int)k) { goto next; }
                for (uint32_t i = 0; i < k; i++) {
                    CHECK(e1[i].src_btn == names[idx[i]] && e1[i].dst_btn == iblue_out_dst(out) && e1[i].dst_id == 1);
                }
                iblue_map_render(e1, (size_t)c1, 1, out, again, sizeof(again));
                c2 = iblue_map_parse(again, strlen(again), out, 1, e2, &why);
                CHECKM(c2 == c1 && memcmp(e1, e2, (size_t)c1 * sizeof(ent_t)) == 0, "[%s] -> [%s]", text, again);
                iblue_map_render(e2, (size_t)c2, 1, out, third, sizeof(third));
                SAME(third, again);
                CHECK(k || strcmp(again, "none") == 0);
                cases++;
            next:
                /* next combination */
                {
                    int32_t p = (int32_t)k - 1;
                    while (p >= 0 && idx[p] == nnames - k + (uint32_t)p) { p--; }
                    if (p < 0) { break; }
                    idx[p]++;
                    for (uint32_t q = (uint32_t)p + 1; q < k; q++) { idx[q] = idx[q - 1] + 1; }
                }
            }
        }
    }
    printf("  round trip text->entries->text: %llu combinations\n", (unsigned long long)cases);
}

/* every value of every option, struct -> render -> parse -> struct, for one entry */
static void roundtrip_options(void) {
    for (uint32_t out = 0; out < IBLUE_OUT_COUNT; out += 5) {
        for (uint32_t v = 0; v < 256; v++) {
            for (int which = 0; which < 5; which++) {
                ent_t e = {IBM_RB_DOWN, iblue_out_dst(out), 2, 100, 50, 135, 0, 0}, f;
                char t[80];
                const char *why;
                if (which == 0) { e.perc_deadzone = (uint8_t)v; }
                if (which == 1) { if (v > 100) { continue; } e.perc_threshold = (uint8_t)v; }
                if (which == 2) { e.perc_max = (uint8_t)v; }
                if (which == 3) { e.turbo = (uint8_t)v; }
                /* algo 5 (passthrough) reads past the end of algo_names[] (known bug, test_bugs.c): left out here so this test can run */
                if (which == 4) { if (v > 4) { continue; } e.algo = (uint8_t)v; }
                iblue_map_render(&e, 1, 2, out, t, sizeof(t));
                CHECKM(iblue_map_parse(t, strlen(t), out, 2, &f, &why) == 1 && memcmp(&e, &f, sizeof(e)) == 0, "option %d value %u: [%s]", which, v, t);
            }
        }
    }
}

/* render: any cap 0..80 */
static void render_caps(void) {
    ent_t tab[8];
    char full[600];
    size_t n = 0;
    const char *why;
    ent_t ents[4];
    static const char longline[] = "lstick.up:deadzone=17%:threshold=99:scale=255:turbo=255:algo=aggressive , dpad2.left:deadzone=1.5% , a , rstick.click:turbo=7";
    int c = iblue_map_parse(longline, sizeof(longline) - 1, 0, 3, ents, &why);

    CHECK(c == 4);
    memcpy(tab, ents, sizeof(ents));
    n = 4;
    iblue_map_render(tab, n, 3, 0, full, sizeof(full));
    for (int variant = 0; variant < 3; variant++) {
        size_t nn = variant == 0 ? n : (variant == 1 ? 1 : 0);   /* the long line, one short entry, no entry at all */
        char ref[600];
        iblue_map_render(tab, nn, 3, 0, ref, sizeof(ref));
        for (size_t cap = 0; cap <= 80; cap++) {
            unsigned char *b = malloc(cap + 16);
            size_t r;
            memset(b, 0xA5, cap + 16);
            r = iblue_map_render(tab, nn, 3, 0, (char *)b, cap);
            if (cap == 0) {
                CHECK(r == 0);
                for (size_t i = 0; i < 16; i++) { CHECK(b[i] == 0xA5); }
            }
            else {
                CHECK(r < cap);
                CHECK(b[r] == 0);
                CHECK(strlen((char *)b) == r);
                for (size_t i = cap; i < cap + 16; i++) { CHECKM(b[i] == 0xA5, "wrote past cap %zu (variant %d)", cap, variant); }
                /* the result is the whole line, or a whole number of entries of it (cut before a separator), or empty */
                CHECKM(r == strlen(ref) ? memcmp(b, ref, r) == 0 : (r == 0 || (memcmp(b, ref, r) == 0 && ref[r] == ',')),
                       "cap %zu variant %d: [%s] of [%s]", cap, variant, (char *)b, ref);
                if (r == strlen(ref)) { CHECK(r <= cap - 1); }
            }
            free(b);
        }
    }
}

/* random tables for replace and layouts: pad entries of ports 0..3, keyboard / mouse sources, combos, other ports, unrelated dsts.
 * No entry whose source has no name here (those are in test_bugs.c). */
static uint8_t named_src(void) {
    uint8_t s;
    do { s = (uint8_t)rnd_below(IBM_PAD_IDS); } while (!iblue_in_name(s));
    return s;
}
static size_t random_table(ent_t *t, size_t max) {
    size_t n = rnd_below((uint32_t)max + 1);
    for (size_t i = 0; i < n; i++) {
        ent_t e;
        e.dst_id = (uint8_t)rnd_below(4);
        switch (rnd_below(5)) {
            case 0: e.src_btn = (uint8_t)(32 + rnd_below(56)); e.dst_btn = iblue_out_dst(rnd_below(IBLUE_OUT_COUNT)); break;        /* keyboard / mouse source */
            case 1: e.src_btn = named_src(); e.dst_btn = (uint8_t)(118 + rnd_below(10)); break;                                   /* combo */
            case 2: e.src_btn = named_src(); e.dst_btn = (uint8_t)(rnd_below(2) ? 22 : 27); break;                                 /* a destination the pad has no output for */
            default: e.src_btn = named_src(); e.dst_btn = iblue_out_dst(rnd_below(IBLUE_OUT_COUNT)); break;
        }
        e.perc_max = (uint8_t)(rnd_below(3) ? 100 : rnd()); e.perc_threshold = (uint8_t)(rnd_below(3) ? 50 : rnd_below(101));
        e.perc_deadzone = (uint8_t)(rnd_below(3) ? 135 : rnd()); e.turbo = (uint8_t)(rnd_below(4) ? 0 : rnd()); e.algo = (uint8_t)rnd_below(5);
        t[i] = e;
    }
    return n;
}

static int is_owned(const ent_t *e, uint8_t port, uint8_t dst) {
    return e->dst_id == port && e->dst_btn == dst && e->src_btn < IBM_PAD_IDS;
}

static void replace_props(long iters) {
    for (long it = 0; it < iters; it++) {
        ent_t *tab = malloc(MAXE * sizeof(ent_t));          /* exactly the size the firmware has: ASAN sees an overrun */
        ent_t before[MAXE], ents[4], model[MAXE + 4];
        size_t n = random_table(tab, MAXE - (it % 3 ? 8 : 0)), mn = 0, nents = rnd_below(5);
        uint8_t port = (uint8_t)rnd_below(4);
        uint32_t out = rnd_below(IBLUE_OUT_COUNT);
        size_t max = (it % 5 == 0) ? n + rnd_below(3) : MAXE, r, keep = 0;
        if (max > MAXE) { max = MAXE; }                       /* max is the size of the table the caller owns */

        memcpy(before, tab, n * sizeof(ent_t));
        for (size_t i = 0; i < nents; i++) { ents[i] = (ent_t){named_src(), iblue_out_dst(out), port, 100, 50, 135, 0, 0}; }
        for (size_t i = 0; i < n; i++) {
            if (!is_owned(&before[i], port, iblue_out_dst(out))) { model[mn++] = before[i]; keep++; }
        }
        for (size_t i = 0; i < nents; i++) { model[mn++] = ents[i]; }
        r = iblue_map_replace(tab, n, max, port, out, nents ? ents : NULL, nents);
        if (keep + nents > max) {
            CHECK(r == (size_t)-1);
            CHECK(memcmp(tab, before, n * sizeof(ent_t)) == 0);        /* nothing changed */
        }
        else {
            CHECKM(r == mn, "size %zu want %zu", r, mn);
            CHECK(r <= max);
            CHECK(r == (size_t)-1 || memcmp(tab, model, mn * sizeof(ent_t)) == 0);   /* order of the others kept, new entries last */
            /* entries of another port, keyboard / mouse sources and combos are in the others, byte for byte and in order */
            if (r != (size_t)-1) {
                size_t w = 0;
                for (size_t i = 0; i < n; i++) {
                    if (before[i].dst_id != port || before[i].src_btn >= IBM_PAD_IDS || before[i].dst_btn >= 118) {
                        while (w < r && !(memcmp(&tab[w], &before[i], sizeof(ent_t)) == 0)) { w++; }
                        CHECK(w < r);
                        w++;
                    }
                }
            }
        }
        free(tab);
    }
}

/* layouts */
static void factory_default(ent_t *t, size_t *n, uint8_t port) {
    *n = 0;
    for (uint32_t i = 0; i < 88; i++) { t[(*n)++] = (ent_t){(uint8_t)i, (uint8_t)i, port, 100, 50, 135, 0, 0}; }
    for (uint32_t k = 0; k < 10; k++) { t[(*n)++] = (ent_t){(uint8_t)(20 + k), (uint8_t)(118 + k), port, 100, 50, 135, 0, 0}; }
}
static int ent_cmp(const void *a, const void *b) { return memcmp(a, b, sizeof(ent_t)); }

static void layout_props(long iters) {
    ent_t t[MAXE], u[MAXE], keep_copy[MAXE];
    size_t n, m;

    for (uint32_t l = 0; l < IBLUE_LAYOUT_COUNT; l++) {
        CHECK(iblue_layout_find(iblue_layout_name(l), strlen(iblue_layout_name(l))) == (int)l);
    }
    CHECK(iblue_layout_name(IBLUE_LAYOUT_COUNT) == NULL && iblue_layout_find("custom", 6) == -1 && iblue_layout_find("", 0) == -1);
    /* BlueRetro's factory default: "all" changes nothing visible */
    for (uint8_t port = 0; port < 4; port++) {
        char a[64], b[64];
        factory_default(t, &n, port);
        memcpy(u, t, n * sizeof(ent_t));
        CHECK(iblue_layout_match(t, n, port) == 1);                                       /* the factory default is "all" */
        CHECK(iblue_layout_match(t, n, (uint8_t)((port + 1) & 3)) == -1);                   /* ... and the other ports have no entries at all */
        m = iblue_layout_apply(1, t, n, MAXE, port);
        CHECK(m == n);                                                                    /* same number of entries */
        for (uint32_t o = 0; o < IBLUE_OUT_COUNT; o++) {
            iblue_map_render(u, n, port, o, a, sizeof(a));
            iblue_map_render(t, m, port, o, b, sizeof(b));
            SAME(b, a);
        }
        qsort(t, m, sizeof(ent_t), ent_cmp);
        qsort(u, n, sizeof(ent_t), ent_cmp);
        CHECK(memcmp(t, u, n * sizeof(ent_t)) == 0);                                      /* the same set of entries (the order changes: see test_bugs.c / the report) */
    }
    for (long it = 0; it < iters; it++) {
        uint32_t l = rnd_below(IBLUE_LAYOUT_COUNT);
        uint8_t port = (uint8_t)rnd_below(4);
        ent_t *tab = malloc(MAXE * sizeof(ent_t));
        size_t nn = random_table(tab, MAXE - 30), r1, r2;
        ent_t first[MAXE];

        memcpy(keep_copy, tab, nn * sizeof(ent_t));
        r1 = iblue_layout_apply(l, tab, nn, MAXE, port);
        CHECK(r1 != (size_t)-1);
        if (r1 == (size_t)-1) { free(tab); continue; }
        CHECK(iblue_layout_match(tab, r1, port) == (int)l);
        memcpy(first, tab, r1 * sizeof(ent_t));
        r2 = iblue_layout_apply(l, tab, r1, MAXE, port);
        CHECK(r2 == r1 && memcmp(first, tab, r1 * sizeof(ent_t)) == 0);                     /* applying twice = once, byte for byte */
        /* what the layout does not own is untouched, in order: other ports, keyboard / mouse, combos, destinations the pad has no output for */
        {
            size_t w = 0;
            for (size_t i = 0; i < nn; i++) {
                const ent_t *e = &keep_copy[i];
                int owned = e->dst_id == port && e->src_btn < IBM_PAD_IDS;
                if (owned) {
                    int dc = 0;
                    for (uint32_t o = 0; o < IBLUE_OUT_COUNT; o++) { dc |= iblue_out_dst(o) == e->dst_btn; }
                    owned = dc;
                }
                if (!owned) {
                    while (w < r1 && memcmp(&tab[w], e, sizeof(ent_t)) != 0) { w++; }
                    CHECK(w < r1);
                    w++;
                }
            }
        }
        /* a table that is too small for the layout: refused, unchanged */
        {
            ent_t small[MAXE];
            size_t k = nn;
            memcpy(small, keep_copy, nn * sizeof(ent_t));
            CHECK(iblue_layout_apply(l, small, k, 5, port) == (size_t)-1 || k + 0 <= 5);
            CHECK(memcmp(small, keep_copy, nn * sizeof(ent_t)) == 0 || k <= 5);
        }
        free(tab);
    }
    CHECK(iblue_layout_apply(IBLUE_LAYOUT_COUNT, t, 0, MAXE, 0) == (size_t)-1);
}

/* the deadzone codec */
static int dz_parse(const char *v, uint8_t *raw) {
    char text[40];
    ent_t e;
    const char *why;
    int c;
    snprintf(text, sizeof(text), "a:deadzone=%s", v);
    c = iblue_map_parse(text, strlen(text), 0, 0, &e, &why);
    if (c == 1) { *raw = e.perc_deadzone; }
    return c == 1 ? 0 : 1;
}

static void deadzone_codec(void) {
    uint8_t raw;
    char v[24];

    for (uint32_t r = 0; r <= 255; r++) {
        uint32_t want = r <= 200 ? r : (r - 198) * 100;
        CHECK(iblue_dz_x10000((uint8_t)r) == want);
        if (r <= 200) { CHECK(iblue_dz_x10000((uint8_t)r) == r); }                       /* old configurations: unchanged */
    }
    CHECK(iblue_dz_x10000(135) == 135);
    {
        static const uint32_t pct_x100[6] = {0, 135, 500, 1000, 1500, 2000};           /* 0, 1.35, 5, 10, 15, 20 percent */
        for (int l = 0; l <= 5; l++) {
            CHECK(iblue_dz_x10000(iblue_dz_from_level(l)) == pct_x100[l]);
            CHECK(iblue_dz_level(iblue_dz_from_level(l)) == l);
        }
    }
    CHECK(iblue_dz_from_level(-1) == 135 && iblue_dz_from_level(6) == 135);
    for (uint32_t r = 0; r <= 255; r++) {
        int l = iblue_dz_level((uint8_t)r);
        CHECK(l == -1 || iblue_dz_from_level(l) == r);
    }
    /* accepted */
    CHECK(!dz_parse("off", &raw) && raw == 0);
    CHECK(!dz_parse("OFF", &raw) && raw == 0);
    for (int l = 0; l <= 5; l++) { snprintf(v, sizeof(v), "%d", l); CHECK(!dz_parse(v, &raw) && raw == iblue_dz_from_level(l)); }
    for (uint32_t pct = 3; pct <= 57; pct++) {
        snprintf(v, sizeof(v), "%u%%", pct);
        CHECK(!dz_parse(v, &raw) && raw == pct + 198 && iblue_dz_x10000(raw) == pct * 100);   /* n% is exactly n percent */
    }
    for (uint32_t h = 0; h <= 299; h++) {                                                  /* n.m% / n.mm% up to 2 % in hundredths */
        snprintf(v, sizeof(v), "%u.%02u%%", h / 100, h % 100);
        if (h <= 200) { CHECKM(!dz_parse(v, &raw) && raw == h, "%s", v); } else { CHECKM(dz_parse(v, &raw), "%s must be refused", v); }
        if (h % 10 == 0) {
            snprintf(v, sizeof(v), "%u.%u%%", h / 100, (h % 100) / 10);
            if (h <= 200) { CHECKM(!dz_parse(v, &raw) && raw == h, "%s", v); } else { CHECKM(dz_parse(v, &raw), "%s must be refused", v); }
        }
    }
    CHECK(!dz_parse("1.35%", &raw) && raw == 135);
    CHECK(!dz_parse("0%", &raw) && raw == 0);
    CHECK(!dz_parse("2%", &raw) && raw == 200);
    CHECK(!dz_parse("5%", &raw) && raw == iblue_dz_from_level(2));                        /* the percent form and the level 2 agree */
    CHECK(!dz_parse("10%", &raw) && raw == iblue_dz_from_level(3));
    CHECK(!dz_parse("15%", &raw) && raw == iblue_dz_from_level(4));
    CHECK(!dz_parse("20%", &raw) && raw == iblue_dz_from_level(5));
    /* refused */
    static const char *const bad[] = {"2.5%", "6", "60%", "", "58%", "100%", "-1", "+1", "1.", ".5%", "%", "1.234%", "2.01%", "3.5%", "1 %", "1,5%", "0x1", "off%", "O", "10", "255", "1.5", "a%", "99999%", "1%%"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) { CHECKM(dz_parse(bad[i], &raw), "[%s] must be refused", bad[i]); }
}

int main(void) {
    long n = env_n("FUZZ_N", 3000);

    rnd_seed();
    for (uint32_t i = 0; i < IBM_PAD_IDS; i++) {
        if (iblue_in_name((uint8_t)i)) { names[nnames++] = i; }
    }
    CHECK(nnames == 30);
    CHECK(iblue_in_name(26) == NULL && iblue_in_name(30) == NULL && iblue_in_name(32) == NULL && iblue_in_name(255) == NULL);
    roundtrip_all();
    roundtrip_options();
    render_caps();
    replace_props(n * 4);
    layout_props(n);
    deadzone_codec();
    printf("props: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
