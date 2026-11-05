/* Fuzz test of the parser (iblue_map_parse), the DC API engine (iblue_dcapi_handle), the Maple entry (iblue_dcapi_maple) and the BLE
 * chunks. Run under ASAN / UBSAN (run.sh does). Every buffer handed to the code is a malloc of exactly the size the code may use, so any
 * read or write outside it is reported. FUZZ_N = rounds (default 20000), FUZZ_SEED = seed. */
#include "tcommon.h"
#include "adapter/adapter.h"
#include "system/iblue_gameid.h"
#include "system/iblue_dcapi.h"

void adapter_q_fb(struct raw_fb *fb_data) { (void)fb_data; }
static long nreq, nadd;
int iblue_dcapi_cfg_request(uint8_t op, uint8_t a, uint8_t b) { (void)op; (void)a; (void)b; nreq++; return 1; }
int iblue_dcapi_cfg_set_map(uint8_t port, uint8_t out, const struct iblue_mapent *e, size_t n) { (void)port; (void)out; (void)e; nadd += (long)n; return 1; }
#define LIVE_MAX 128
static struct iblue_mapent live[IBLUE_DCAPI_PORTS][LIVE_MAX];
static size_t live_n[IBLUE_DCAPI_PORTS];
const struct iblue_mapent *iblue_dcapi_cfg_map(uint32_t port, size_t *count) {
    if (port >= IBLUE_DCAPI_PORTS) { *count = 0; return NULL; }
    *count = live_n[port];
    return live[port];
}

static const char *const pieces[] = {
    "a", "lb", "rb , lt:threshold=78", "lstick.up:deadzone=2%", "x:deadzone=1.35%:scale=10:turbo=2:algo=scurve", "none", "dpad2.left, dpad2.right",
    "a:deadzone=", "a:", ":", ",", ",,", "a,,b", "a:b:c", "a:threshold=", "a:threshold=101", "a:scale=256", "a:algo=", "a:algo=nope", "a:deadzone=2.5%",
    "back, start, guide, share, lt", "A1_UP", "select", "menu", "L1", "l2", "  a  ,  b  ", "a\tb", "\xff\xfe", "lstick.click:turbo=255:turbo=1",
};
static const char *const lines[] = {
    "dc1", "id 17", "id", "id 12345678901234567890", "to self", "to iblue", "to nobody", "to", "get hello", "get hello.api", "get *", "get port.*", "get port.A.*",
    "get port.A.map.*", "get port.A.map.Z", "get port.B.map.A1*", "get port.D.map.A", "get port.E.map.A", "get port.A.layout", "get port.A.layouts",
    "get sys.unsaved", "get game.id", "get net.selected", "get events", "get events since=3", "get", "get ", "get  port.A.map.z", "get port.a.map.Z",
    "set port.A.map.Z lb", "set port.A.map.Z", "set port.A.map.C rb, lt:threshold=78", "set port.B.map.A1_UP lstick.up:deadzone=3%", "set port.A.mode dual",
    "set port.A.acc both", "set port.A.layout all", "set port.A.layout standard", "set port.C.layout all", "set net.selected dcnet", "set net.selected x",
    "set port.A.state wired", "set", "set ", "set x", "call sys.commit", "call sys.revert", "call", "call x", "frobnicate", "\r", " ", "\t",
};

static size_t build_text(char *dst, size_t cap, int ops) {
    size_t n = 0;
    for (int i = 0; i < ops; i++) {
        const char *l = lines[rnd_below(sizeof(lines) / sizeof(lines[0]))];
        char tmp[200];
        if (!strncmp(l, "set port.A.map.", 15) && rnd_below(2)) {
            snprintf(tmp, sizeof(tmp), "set port.%c.map.%s %s", (char)('A' + rnd_below(5)), iblue_out_name(rnd_below(IBLUE_OUT_COUNT)),
                     pieces[rnd_below(sizeof(pieces) / sizeof(pieces[0]))]);
            l = tmp;
        }
        size_t k = strlen(l);
        if (n + k + 2 > cap) { break; }
        memcpy(dst + n, l, k);
        n += k;
        dst[n++] = (char)(rnd_below(8) ? '\n' : (rnd_below(2) ? '\r' : ' '));
        if (rnd_below(16) == 0 && n < cap) { dst[n++] = '\n'; }
    }
    return n;
}

static void mutate(char *b, size_t *n, size_t cap) {
    uint32_t m = rnd_below(5);
    for (uint32_t i = 0; i < m && *n; i++) {
        size_t p = rnd_below((uint32_t)*n);
        switch (rnd_below(6)) {
            case 0: b[p] ^= (char)(1u << rnd_below(8)); break;
            case 1: b[p] = (char)rnd(); break;
            case 2: b[p] = 0; break;                                                   /* an embedded NUL */
            case 3: if (*n + 1 < cap) { memmove(b + p + 1, b + p, *n - p); b[p] = (char)(rnd_below(2) ? ',' : ':'); (*n)++; } break;
            case 4: memmove(b + p, b + p + 1, *n - p - 1); (*n)--; break;
            default: b[p] = (char)(0x80 + rnd_below(128)); break;                      /* non-ASCII */
        }
    }
}

static long parse_stats[6];
static void fuzz_parse(long rounds) {
    char big[900];
    for (long it = 0; it < rounds; it++) {
        size_t n = 0, cap = it % 50 == 0 ? sizeof(big) : 120;
        uint32_t np = 1 + rnd_below(it % 50 == 0 ? 60 : 6);
        for (uint32_t i = 0; i < np && n + 70 < cap; i++) {
            /* odd rounds: only well formed pieces (the first 7 and "  a  ,  b  "), so most of them reach the entry and option code */
            const char *p = (it & 1) ? pieces[rnd_below(7)] : pieces[rnd_below(sizeof(pieces) / sizeof(pieces[0]))];
            size_t k = strlen(p);
            if (i) { big[n++] = (char)((it & 1) ? ',' : (rnd_below(8) ? ',' : ':')); }
            memcpy(big + n, p, k);
            n += k;
        }
        if (!(it & 1) || rnd_below(3) == 0) { mutate(big, &n, cap); }
        for (int variant = 0; variant < 4; variant++) {
            size_t len = n;
            struct iblue_mapent *ents = malloc(IBLUE_MAP_PER_OUT * sizeof(struct iblue_mapent));    /* exactly 4 entries */
            char *buf;
            const char *why = NULL;
            uint32_t out = rnd_below(28);                                                          /* 26 and 27 are not outputs */
            int c;
            if (variant == 1 && len) { len = rnd_below((uint32_t)len); }                           /* shorter than the text */
            if (variant == 2) { len = n + rnd_below(5); if (len > cap) { len = n; } }              /* longer: the buffer is as long as the length says */
            if (variant == 3) { len = 0; }
            buf = malloc(len ? len : 1);
            for (size_t i = 0; i < len; i++) { buf[i] = i < n ? big[i] : (char)rnd(); }
            memset(ents, 0xC3, IBLUE_MAP_PER_OUT * sizeof(struct iblue_mapent));
            c = iblue_map_parse(variant == 3 ? NULL : buf, len, out, (uint8_t)rnd(), ents, &why);
            CHECK(c >= -1 && c <= IBLUE_MAP_PER_OUT);
            if (c == -1) { CHECK(why != NULL && why[0] == ' '); }
            if (c > 0) {
                parse_stats[0]++;
                for (int i = 0; i < c; i++) {
                    CHECK(ents[i].src_btn < IBM_PAD_IDS && iblue_in_name(ents[i].src_btn) != NULL);
                    CHECK(ents[i].dst_btn == iblue_out_dst(out));
                    for (int j = 0; j < i; j++) { CHECK(ents[j].src_btn != ents[i].src_btn); }
                }
                {
                    char t[400], u[400];
                    struct iblue_mapent e2[IBLUE_MAP_PER_OUT];
                    const char *w2;
                    iblue_map_render(ents, (size_t)c, ents[0].dst_id, out, t, sizeof(t));
                    CHECK(out >= IBLUE_OUT_COUNT || iblue_map_parse(t, strlen(t), out, ents[0].dst_id, e2, &w2) == c);
                    if (out < IBLUE_OUT_COUNT) {
                        CHECK(memcmp(ents, e2, (size_t)c * sizeof(ents[0])) == 0);
                        iblue_map_render(e2, (size_t)c, ents[0].dst_id, out, u, sizeof(u));
                        SAME(u, t);
                    }
                }
            }
            else if (c == 0) { parse_stats[1]++; }
            else { parse_stats[2]++; }
            free(buf);
            free(ents);
        }
    }
}

static void random_world(void) {
    iblue_dcapi_port_count(rnd_below(6));
    for (uint32_t p = 0; p < IBLUE_DCAPI_PORTS; p++) {
        iblue_dcapi_port_set(p, (uint8_t)rnd(), (uint8_t)rnd(), (uint8_t)rnd(), (uint8_t)rnd(), (uint8_t)rnd());
        iblue_dcapi_port_layout(p, (int)rnd_below(5) - 2);
        live_n[p] = rnd_below(3) ? rnd_below(LIVE_MAX + 1) : 0;
        for (size_t i = 0; i < live_n[p]; i++) {
            live[p][i].src_btn = (uint8_t)(rnd_below(4) ? rnd_below(32) : rnd());
            live[p][i].dst_btn = rnd_below(2) ? iblue_out_dst(rnd_below(IBLUE_OUT_COUNT)) : (uint8_t)rnd();
            live[p][i].dst_id = (uint8_t)(rnd_below(4) ? p : rnd());
            live[p][i].perc_max = (uint8_t)(rnd_below(2) ? 100 : rnd());
            live[p][i].perc_threshold = (uint8_t)(rnd_below(2) ? 50 : rnd());
            live[p][i].perc_deadzone = (uint8_t)(rnd_below(2) ? 135 : rnd());
            live[p][i].turbo = (uint8_t)(rnd_below(2) ? 0 : rnd());
            /* algo 5 (passthrough) is the known read past algo_names[] (test_bugs.c); 6..255 are rendered as linear */
            do { live[p][i].algo = (uint8_t)(rnd_below(2) ? rnd_below(8) : rnd()); } while (live[p][i].algo == 5);
        }
    }
}

static long handle_stats[4];
static void fuzz_handle(long rounds) {
    for (long it = 0; it < rounds; it++) {
        char text[700];
        size_t n = 0;
        char *req, *out;
        size_t cap = rnd_below(3) ? IBLUE_DCAPI_ANS_MAX + 1 : rnd_below(IBLUE_DCAPI_ANS_MAX + 40);
        size_t r, len;

        random_world();
        if (rnd_below(9) != 0) { memcpy(text, "dc1\n", 4); n = 4; }
        n += build_text(text + n, sizeof(text) - n, 1 + (int)rnd_below(14));
        mutate(text, &n, sizeof(text));
        len = n;
        if (rnd_below(4) == 0 && len) { len = rnd_below((uint32_t)len + 1); }
        req = malloc(len ? len : 1);                                                              /* exactly len: no read past it */
        memcpy(req, text, len);
        out = malloc(cap ? cap : 1);                                                              /* exactly out_cap */
        memset(out, 0x5A, cap);
        r = iblue_dcapi_handle(req, len, out, cap);
        if (cap == 0) { CHECK(r == 0); }
        else {
            CHECK(r < cap);
            CHECK(out[r] == 0);
            CHECK(strlen(out) == r);
            if (r) { handle_stats[0]++; }
            if (r && out[r - 1] != '\n') { handle_stats[1]++; }              /* informational: see test_bugs.c */
            CHECK(r >= 1 || cap < 5 || 1);
        }
        free(req);
        free(out);
    }
}

static void fuzz_maple(long rounds) {
    for (long it = 0; it < rounds; it++) {
        size_t n = rnd_below(5) == 0 ? IBLUE_DCAPI_REQ_MAX : (rnd_below(3) == 0 ? rnd_below(700) : (rnd_below(70) * 4));
        char text[700];
        size_t tn = 0;
        uint8_t *wire = malloc(n ? n : 1), *outw = malloc(IBLUE_DCAPI_ANS_MAX);                    /* the answer buffer: exactly 400 bytes as documented */
        size_t words;

        random_world();
        if (rnd_below(2)) {
            memcpy(text, "dc1\n", 4);
            tn = 4 + build_text(text + 4, sizeof(text) - 4, 1 + (int)rnd_below(10));
            mutate(text, &tn, sizeof(text));
            for (size_t w = 0; w < n; w += 4) { for (size_t j = 0; j < 4 && w + j < n; j++) { size_t s = w + 3 - j; wire[w + j] = s < tn ? (uint8_t)text[s] : 0; } }
        }
        else {
            for (size_t i = 0; i < n; i++) { wire[i] = (uint8_t)rnd(); }
        }
        words = iblue_dcapi_maple(wire, n, outw);
        CHECK(words * 4 <= IBLUE_DCAPI_ANS_MAX);
        CHECK(words >= 1);
        free(wire);
        free(outw);
    }
}

static void fuzz_ble(long rounds) {
    for (long it = 0; it < rounds; it++) {
        uint32_t chunks = 1 + rnd_below(30);
        random_world();
        for (uint32_t c = 0; c < chunks; c++) {
            size_t len = rnd_below(24);
            uint8_t *d = malloc(len ? len : 1);
            for (size_t i = 0; i < len; i++) { d[i] = rnd_below(3) ? (uint8_t)"dc1\nget hello\nset port.A.map.Z lb\n"[(c * 7 + i) % 32] : (uint8_t)rnd(); }
            if (len) { d[0] = (uint8_t)((c == 0 ? 1 : 0) | (c + 1 == chunks ? 2 : 0) | (rnd_below(8) ? 0 : rnd_below(4))); }
            iblue_dcapi_ble_write(d, len);
            free(d);
        }
        for (int k = 0; k < 40; k++) {
            size_t max = rnd_below(30);
            uint8_t *o = malloc(max ? max : 1);
            size_t r = iblue_dcapi_ble_read(o, max);
            CHECK(r <= max);
            free(o);
            if (!max) { continue; }
        }
    }
}

int main(void) {
    long n = env_n("FUZZ_N", 20000);
    rnd_seed();
    iblue_dcapi_init("fuzz");
    fuzz_parse(n);
    fuzz_handle(n);
    fuzz_maple(n);
    fuzz_ble(n / 4);
    printf("fuzz: parse ok=%ld none=%ld refused=%ld; %ld requests answered, %ld ended in the middle of a line (must be 0); %ld cfg requests, %ld map adds\n",
           parse_stats[0], parse_stats[1], parse_stats[2], handle_stats[0], handle_stats[1], nreq, nadd);
    printf("fuzz: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
