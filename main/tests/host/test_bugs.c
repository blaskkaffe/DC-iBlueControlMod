/* Failing tests for what was found wrong in iblue_mapcfg.c / iblue_dcapi.c (and, as MODEL tests, in the glue that cannot be built here).
 * Each test says what SHOULD happen; it fails ("REPRODUCED", exit 1) while the bug is there and passes once it is fixed.
 * Usage: test_bugs N   (run.sh runs 1..N one by one: an ASAN abort is a reproduction too). test_bugs list  prints the table. */
#include "tcommon.h"
#include "adapter/adapter.h"
#include "iblue/iblue_gameid.h"
#include "iblue/iblue_dcapi.h"

void adapter_q_fb(struct raw_fb *fb_data) { (void)fb_data; }

/* ---- model of the firmware glue (same as test_engine.c): a 15 slot queue that drops when full or muted */
struct msg { uint8_t op, a, b; uint8_t n; struct iblue_mapent e[IBLUE_MAP_PER_OUT]; };
#define IBLUE_CFG_MAP_SET_T 8
#define SLOTS 8
static struct msg q[64];
static int qn, dropped, queue_cap = 15, mute;
static struct iblue_mapent cfg_map[4][128];
static size_t cfg_n[4];
static int slots_used(void) { int c = 0; for (int i = 0; i < qn; i++) { c += q[i].op == IBLUE_CFG_MAP_SET_T; } return c; }
static int push(struct msg m) {
    if (mute || qn >= queue_cap) { dropped++; return 0; }
    q[qn++] = m;
    return 1;
}
int iblue_dcapi_cfg_request(uint8_t op, uint8_t a, uint8_t b) { struct msg m = {op, a, b, 0, {{0}}}; return push(m); }
/* the real one: a mailbox of 8 slots, and ONE message; the slot is free again when the message is not queued */
int iblue_dcapi_cfg_set_map(uint8_t port, uint8_t out, const struct iblue_mapent *e, size_t n) {
    struct msg m = {IBLUE_CFG_MAP_SET_T, port, out, (uint8_t)n, {{0}}};
    if (n > IBLUE_MAP_PER_OUT || slots_used() >= SLOTS) { return 0; }
    for (size_t i = 0; i < n; i++) { m.e[i] = e[i]; }
    return push(m);
}
const struct iblue_mapent *iblue_dcapi_cfg_map(uint32_t port, size_t *count) { if (port >= 4) { *count = 0; return NULL; } *count = cfg_n[port]; return cfg_map[port]; }
static void apply_msg(const struct msg *m) {
    uint8_t a = m->a, b = m->b;
    if (m->op == IBLUE_CFG_MAP_SET_T && a < 4 && b < IBLUE_OUT_COUNT) {
        struct iblue_mapent es[IBLUE_MAP_PER_OUT];
        size_t r;
        for (int k = 0; k < m->n; k++) { es[k] = m->e[k]; }
        r = iblue_map_replace(cfg_map[a], cfg_n[a], 128, a, b, es, m->n);
        if (r == (size_t)-1) { cfg_n[a] = iblue_map_drop_kb(cfg_map[a], cfg_n[a], a); r = iblue_map_replace(cfg_map[a], cfg_n[a], 128, a, b, es, m->n); }
        if (r != (size_t)-1) { cfg_n[a] = r; }
    }
}
static void drain(void) { for (int i = 0; i < qn; i++) { apply_msg(&q[i]); } qn = 0; }
static void factory(uint8_t port) {
    cfg_n[port] = 0;
    for (uint32_t i = 0; i < 115; i++) { cfg_map[port][cfg_n[port]++] = (struct iblue_mapent){(uint8_t)i, (uint8_t)i, port, 100, 50, 135, 0, 0}; }
    for (uint32_t k = 0; k < 10; k++) { cfg_map[port][cfg_n[port]++] = (struct iblue_mapent){(uint8_t)(20 + k), (uint8_t)(118 + k), port, 100, 50, 135, 0, 0}; }
}

static int hit;
#define EXPECT(c, ...) do { if (!(c)) { hit = 1; printf("  expected: %s\n  ", #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static char ans[IBLUE_DCAPI_ANS_MAX + 1];
static const char *ask(const char *req) { iblue_dcapi_handle(req, strlen(req), ans, sizeof(ans)); return ans; }

/* 1: algo_names[5] ("passthrough", 11 characters) fills char[11] with no NUL: every compare / render of it reads past the array (ran: ASAN) */
static void bug1(void) {
    struct iblue_mapent e;
    const char *why = "";
    int c = iblue_map_parse("a:algo=passthrough", 18, 0, 0, &e, &why);   /* ASAN: global-buffer-overflow in eq_ci */
    EXPECT(c == 1 && e.algo == 5, "parse of algo=passthrough gave %d", c);
}

/* 2: an answer is cut in the middle of a line when the operation that overflows is "get events" or "set net.selected <bad>" (ran) */
static void bug2(void) {
    size_t n;
    factory(0);
    iblue_dcapi_port_count(1);
    ask("dc1\nget port.A.map.*\nget events\n");
    n = strlen(ans);
    EXPECT(n > 0 && ans[n - 1] == '\n', "answer of %zu bytes ends with '%c' (0x%02x): half a line; the engine promises a cut at a line end", n, n ? ans[n - 1] : '?', n ? ans[n - 1] : 0);
    ask("dc1\nget port.A.map.*\nset net.selected pizza\n");
    n = strlen(ans);
    EXPECT(n > 0 && ans[n - 1] == '\n', "same with set net.selected pizza: ends with 0x%02x", n ? ans[n - 1] : 0);
    {
        char small[30];
        n = iblue_dcapi_handle("dc1\nget events\n", 15, small, sizeof(small));
        EXPECT(n == 0 || small[n - 1] == '\n', "get events into a 30 byte buffer: [%s]", small);
    }
}

/* 2b: a value that does not fit is shortened inside its line ("ok port.A.map.Z lb" for "lb, rb, lt"), or left empty ("ok port.A.map.Z "), and the line is still ended */
static void bug2b(void) {
    int bad = 0;
    char full[200];
    factory(0);
    iblue_dcapi_port_count(1);
    cfg_n[0] = 0;
    cfg_map[0][cfg_n[0]++] = (struct iblue_mapent){IBM_LS, IBM_LS, 0, 100, 50, 135, 0, 0};
    cfg_map[0][cfg_n[0]++] = (struct iblue_mapent){IBM_RS, IBM_LS, 0, 100, 50, 135, 0, 0};
    cfg_map[0][cfg_n[0]++] = (struct iblue_mapent){IBM_LM, IBM_LS, 0, 100, 50, 135, 0, 0};
    iblue_dcapi_handle("dc1\nget port.A.map.Z\n", 21, full, sizeof(full));   /* "dc1\nok port.A.map.Z lb, rb, lt\n" */
    for (size_t cap = 1; cap < strlen(full); cap++) {
        char *b = malloc(cap);
        size_t n = iblue_dcapi_handle("dc1\nget port.A.map.Z\n", 21, b, cap);
        /* allowed: the whole answer, or a cut before the line (here: "" or "dc1\n"); never a line with another value */
        if (n > 4 && (n != strlen(full) || memcmp(b, full, n) != 0)) { bad++; if (bad < 4) { printf("  cap %zu: [%s]\n", cap, b); } }
        free(b);
    }
    EXPECT(bad == 0, "%d buffer sizes gave a line with a wrong (shortened or empty) value", bad);
}

/* 3: iblue_map_replace and iblue_layout_apply remove entries whose source has no name (ids 26, 30), which render and parse cannot see (ran) */
static void bug3(void) {
    struct iblue_mapent t[128];
    size_t n = 0, r;
    uint32_t outZ = (uint32_t)iblue_out_find("Z", 1);
    t[n++] = (struct iblue_mapent){26, iblue_out_dst(outZ), 0, 100, 50, 135, 0, 0};       /* "Digital LT" of the Xbox column: no input name */
    t[n++] = (struct iblue_mapent){30, iblue_out_dst(outZ), 0, 100, 50, 135, 0, 0};
    r = iblue_map_replace(t, n, 128, 0, outZ, NULL, 0);
    EXPECT(r == 2, "replace left %zu of 2 entries whose source has no name", r);
    n = 2;
    t[0].dst_btn = t[1].dst_btn = IBM_RB_DOWN;
    r = iblue_layout_apply(1, t, n, 128, 0);
    {
        int kept = 0;
        for (size_t i = 0; r != (size_t)-1 && i < r; i++) { kept += (t[i].src_btn == 26 || t[i].src_btn == 30); }
        EXPECT(kept == 2, "layout_apply kept %d of 2 nameless entries (the doc of replace says they are never touched; for a layout this is a question)", kept);
    }
}

/* 4: iblue_layout_match compares at most 47 characters of every line: a table that equals a layout and then has more inputs than fit is called that layout (ran) */
static void bug4(void) {
    struct iblue_mapent t[128], e[4];
    const char *why;
    size_t n = 0;
    int c;
    factory(0);
    memcpy(t, cfg_map[0], sizeof(struct iblue_mapent) * cfg_n[0]);
    n = cfg_n[0];
    n = iblue_layout_apply(0, t, n, 128, 0);                                                 /* standard: RT = "lb, rt" */
    EXPECT(n != (size_t)-1 && iblue_layout_match(t, n, 0) == 0, "setup: standard");
    c = iblue_map_parse("lb, rt, dpad2.left:deadzone=3%:threshold=99:scale=255:turbo=255:algo=aggressive", 79, (uint32_t)iblue_out_find("RT", 2), 0, e, &why);
    EXPECT(c == 3, "setup: three inputs");
    n = iblue_map_replace(t, n, 128, 0, (uint32_t)iblue_out_find("RT", 2), e, (size_t)c);
    EXPECT(iblue_layout_match(t, n, 0) == -1, "RT has a third input now, the port is custom, but the match says layout %d", iblue_layout_match(t, n, 0));
}

/* 5: "set port.A.map.Z" with no value at all clears the output (an empty value is read as none); the other keys answer range for an empty value (ran) */
static void bug5(void) {
    qn = 0;
    factory(0);
    iblue_dcapi_port_count(1);
    ask("dc1\nset port.A.map.Z\n");
    EXPECT(qn == 0 && strstr(ans, "err") != NULL, "a set without a value queued %d messages and answered [%s]", qn, ans + 4);
}

/* 6: a request longer than 252 bytes is cut silently and the last line, cut in the middle, is applied (ran): "turbo=255" becomes "turbo=25" */
static void bug6(void) {
    char text[400];
    uint8_t wire[400], back[IBLUE_DCAPI_ANS_MAX + 4];
    size_t n = 0, pad;
    factory(0);
    iblue_dcapi_port_count(1);
    qn = 0;
    n += (size_t)snprintf(text + n, sizeof(text) - n, "dc1\n");
    while (n < 252 - 29 - 5) { n += (size_t)snprintf(text + n, sizeof(text) - n, "id 123456789\n"); }
    while (n < 252 - 29 + 2) { text[n++] = '\n'; }                                         /* so that the cut is after "turbo=25" */
    n += (size_t)snprintf(text + n, sizeof(text) - n, "set port.A.map.Z lb:turbo=255\n");
    pad = (n + 3) & ~3u;
    memset(wire, 0, sizeof(wire));
    for (size_t w = 0; w < pad; w += 4) { for (size_t j = 0; j < 4; j++) { wire[w + j] = (w + 3 - j) < n ? (uint8_t)text[w + 3 - j] : 0; } }
    iblue_dcapi_maple(wire, pad, back);
    EXPECT(qn == 0, "a request of %zu bytes was cut at 252 and applied: %d messages queued", n, qn);
}

/* 7: the prefix match of get port.A.map.<prefix>* folds case with |0x20, so DEL matches '_' and control characters match digits (ran) */
static void bug7(void) {
    factory(0);
    iblue_dcapi_port_count(1);
    ask("dc1\nget port.A.map.A1\x7f*\n");
    EXPECT(strstr(ans, "ok port.A.map.A1_UP") == NULL, "a prefix with a DEL listed the A1 outputs: [%.40s]", ans + 4);
}

/* 8 (MODEL): sets that need more than the 15 slots of the queue: the tail is dropped without anyone knowing and the engine answers ok for all.
 * The table is half changed: MAP_CLEAR applied, some MAP_ADD lost (glue: adapter_q_fb() drops when the queue is full, iblue_dcapi_cfg_request() returns void). */
static void bug8(void) {
    char line[80];
    factory(0);
    iblue_dcapi_port_count(1);
    qn = dropped = 0;
    ask("dc1\nset port.A.map.A a, b\nset port.A.map.B a, b\nset port.A.map.X a, b\nset port.A.map.Y a, b\nset port.A.map.START a, b, x, y\n");   /* 4 x 3 + 5 = 17 messages */
    EXPECT(strstr(ans, "err") == NULL && strstr(ans, "ok port.A.map.START a, b, x, y") != NULL, "(the engine answers ok for all five)");
    drain();
    iblue_dcapi_handle("dc1\nget port.A.map.START\n", 25, line, sizeof(line));
    EXPECT(dropped == 0 && strstr(line, "a, b, x, y") != NULL, "%d messages dropped; START was answered 'a, b, x, y' but now is: %s", dropped, line + 4);
}

/* 9 (MODEL): "call sys.commit" answers done and clears sys.unsaved although the COMMIT message was dropped (queue full or rumble_mute) */
static void bug9(void) {
    factory(0);
    iblue_dcapi_port_count(1);
    iblue_dcapi_port_set(0, IBLUE_PORT_BT, 0, 0, 1, 1);
    qn = dropped = 0;
    mute = 1;                                                                                  /* adapter_q_fb(): if (!rumble_mute) enqueue */
    ask("dc1\nset port.A.acc both\n");                                                          /* the mode change itself is lost ... */
    ask("dc1\ncall sys.commit\nget sys.unsaved\n");
    mute = 0;
    EXPECT(!(dropped > 0 && strstr(ans, "ok sys.commit done") != NULL), "%d messages dropped while the answer says [%s]", dropped, ans + 4);
}

/* 10: port.A.layout is not touched by a map set: it keeps saying "all" until the manager task recomputes it (read: every 32nd loop, about 320 ms) (ran) */
static void bug10(void) {
    factory(0);
    iblue_dcapi_port_count(1);
    iblue_dcapi_port_layout(0, 1);
    qn = 0;
    ask("dc1\nset port.A.map.Z rb\nget port.A.layout\n");
    EXPECT(strstr(ans, "port.A.layout all") == NULL, "after a map set the port still reads layout all: [%s]", strstr(ans, "ok port.A.layout"));
}

/* 11 (suspicion, ran): stored values the parser refuses are rendered anyway; get then set of the same text fails (threshold > 100) */
static void bug11(void) {
    struct iblue_mapent e = {IBM_RB_DOWN, IBM_LS, 0, 100, 150, 135, 0, 0}, f;
    char t[80];
    const char *why;
    iblue_map_render(&e, 1, 0, (uint32_t)iblue_out_find("Z", 1), t, sizeof(t));
    EXPECT(iblue_map_parse(t, strlen(t), 0, 0, &f, &why) == 1, "rendered [%s] but the parser refuses it (%s)", t, why);
}

static const struct { void (*fn)(void); const char *title; } bugs[] = {
    {bug1, "algo_names[5] has no NUL: read past the array (ASAN)"},
    {bug2, "answer ends in the middle of a line (get events / set net.selected after an overflow)"},
    {bug2b, "a value that does not fit is shortened or empty inside a line that is still ended"},
    {bug3, "replace / layout_apply delete entries whose source has no name"},
    {bug4, "layout_match compares 47 characters only: a custom port is reported as a layout"},
    {bug5, "set port.X.map.OUT with no value clears the output"},
    {bug6, "a request longer than 252 bytes is cut silently and the cut line applied"},
    {bug7, "prefix get folds case with |0x20: DEL matches _"},
    {bug8, "MODEL: more than 15 queued messages: CLEAR applied, ADDs lost, answer ok"},
    {bug9, "MODEL: sys.commit answers done and clears unsaved when the message was dropped"},
    {bug10, "port.X.layout stale after a map set"},
    {bug11, "suspicion: rendered threshold > 100 is refused by the parser"},
};
#define NBUGS (sizeof(bugs) / sizeof(bugs[0]))

int main(int argc, char **argv) {
    if (argc < 2) { return 2; }
    if (!strcmp(argv[1], "list")) {
        for (size_t i = 0; i < NBUGS; i++) { printf("%zu\t%s\n", i + 1, bugs[i].title); }
        return 0;
    }
    {
        size_t i = (size_t)atoi(argv[1]);
        if (i < 1 || i > NBUGS) { return 2; }
        iblue_dcapi_init("bugs");
        bugs[i - 1].fn();
        printf("BUG %zu: %s: %s\n", i, bugs[i - 1].title, hit ? "REPRODUCED" : "not reproduced");
        return hit ? 1 : 0;
    }
}
