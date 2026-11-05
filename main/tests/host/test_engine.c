/* Behaviour of the DC API engine (iblue_dcapi.c) as the firmware uses it: limits, cutting, addressing, case, whitespace, ports, and the
 * messages it queues. MODEL: the glue (iblue_dcapi_cfg.c, the 15-slot feedback queue) cannot be compiled here, so a model of it is in this
 * file (apply_msg() copies the MAP_* / SET_LAYOUT cases of iblue_dcapi_cfg_apply(), the queue is a plain array). Everything must pass today;
 * what is wrong is in test_bugs.c. */
#include "tcommon.h"
#include "adapter/adapter.h"
#include "system/iblue_gameid.h"
#include "system/iblue_dcapi.h"

void adapter_q_fb(struct raw_fb *fb_data) { (void)fb_data; }

/* ---- the model of the glue */
#define QCAP 15                                   /* queue_bss_init(16, ...) puts item_num - 1 free items in: 15 usable (read) */
struct msg { uint8_t op, a, b; uint8_t n; struct iblue_mapent e[IBLUE_MAP_PER_OUT]; };
#define IBLUE_CFG_MAP_SET_T 8
#define SLOTS 8
static struct msg q[64];
static int qn, dropped;
static int queue_cap = QCAP;
static int mute = 0;                              /* adapter_q_fb() throws everything away while rumble_mute is set (read) */
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
const struct iblue_mapent *iblue_dcapi_cfg_map(uint32_t port, size_t *count) {
    if (port >= 4) { *count = 0; return NULL; }
    *count = cfg_n[port];
    return cfg_map[port];
}
static int layout_refused;
static void apply_msg(const struct msg *m) {
    uint8_t a = m->a, b = m->b;
    switch (m->op) {
        case IBLUE_CFG_MAP_SET_T:
            if (a < 4 && b < IBLUE_OUT_COUNT) {
                struct iblue_mapent es[IBLUE_MAP_PER_OUT];
                size_t r;
                for (int k = 0; k < m->n; k++) { es[k] = m->e[k]; }
                r = iblue_map_replace(cfg_map[a], cfg_n[a], 128, a, b, es, m->n);
                if (r == (size_t)-1) { cfg_n[a] = iblue_map_drop_kb(cfg_map[a], cfg_n[a], a); r = iblue_map_replace(cfg_map[a], cfg_n[a], 128, a, b, es, m->n); }
                if (r != (size_t)-1) { cfg_n[a] = r; }
            }
            break;
        case IBLUE_CFG_SET_LAYOUT:
            if (a < 4 && b < IBLUE_LAYOUT_COUNT) {
                size_t r = iblue_layout_apply(b, cfg_map[a], cfg_n[a], 128, a);
                if (r != (size_t)-1) { cfg_n[a] = r; } else { layout_refused++; }
            }
            break;
        default: break;
    }
}
static void drain(void) {
    for (int i = 0; i < qn; i++) { apply_msg(&q[i]); }
    qn = 0;
}
static void factory(uint8_t port) {
    cfg_n[port] = 0;
    for (uint32_t i = 0; i < 115; i++) { cfg_map[port][cfg_n[port]++] = (struct iblue_mapent){(uint8_t)i, (uint8_t)i, port, 100, 50, 135, 0, 0}; }
    for (uint32_t k = 0; k < 10; k++) { cfg_map[port][cfg_n[port]++] = (struct iblue_mapent){(uint8_t)(20 + k), (uint8_t)(118 + k), port, 100, 50, 135, 0, 0}; }
}

static char ans[IBLUE_DCAPI_ANS_MAX + 1];
static const char *ask(const char *req) {
    iblue_dcapi_handle(req, strlen(req), ans, sizeof(ans));
    return ans;
}
static int count_of(const char *hay, const char *needle) {
    int c = 0;
    for (const char *p = hay; (p = strstr(p, needle)) != NULL; p++) { c++; }
    return c;
}

int main(void) {
    iblue_dcapi_init("test");
    iblue_dcapi_port_count(2);
    factory(0);
    factory(1);

    /* ---- operations per request: 8 are answered, the rest is dropped without a word */
    {
        char many[900] = "dc1\n";
        for (int i = 0; i < 8; i++) { strcat(many, "get hello.api\n"); }
        iblue_dcapi_handle(many, strlen(many), ans, sizeof(ans));
        CHECK(count_of(ans, "ok hello.api 1\n") == 8 && count_of(ans, "err") == 0);
        strcat(many, "get hello.kind\n");
        iblue_dcapi_handle(many, strlen(many), ans, sizeof(ans));
        CHECK(count_of(ans, "ok hello.api 1\n") == 8 && strstr(ans, "hello.kind") == NULL && count_of(ans, "err") == 0);   /* the 9th: no answer, no error */
        /* id and to lines are not operations */
        SAME(ask("dc1\nid 1\nto self\nget hello.api\nget hello.api\nget hello.api\nget hello.api\nget hello.api\nget hello.api\nget hello.api\nget hello.api\nget hello.kind\n"),
             "dc1\nid 1\nok hello.api 1\nok hello.api 1\nok hello.api 1\nok hello.api 1\nok hello.api 1\nok hello.api 1\nok hello.api 1\nok hello.api 1\n");
    }

    /* ---- get port.A.map.* : cut at a line end, every line whole and equal to the single get */
    {
        const char *all = ask("dc1\nget port.A.map.*\n");
        size_t len = strlen(all);
        char line1[64];
        CHECK(len <= IBLUE_DCAPI_ANS_MAX && len > 300 && all[len - 1] == '\n');
        CHECK(strncmp(all, "dc1\nok port.A.map.A a\n", 22) == 0);
        for (const char *p = all + 4; *p;) {
            const char *nl = strchr(p, '\n');
            char single[200], key[40];
            size_t kl;
            CHECK(nl != NULL && strncmp(p, "ok port.A.map.", 14) == 0);
            if (!nl) { break; }
            kl = (size_t)(strchr(p + 14, ' ') - (p + 14));
            memcpy(key, p + 14, kl);
            key[kl] = 0;
            snprintf(single, sizeof(single), "dc1\nget port.A.map.%s\n", key);
            iblue_dcapi_handle(single, strlen(single), line1, sizeof(line1));
            CHECK(strlen(line1) == (size_t)(nl - p) + 1 + 4 && memcmp(line1 + 4, p, (size_t)(nl - p) + 1) == 0);
            p = nl + 1;
        }
        CHECK(count_of(all, "\n") < 27);                                                  /* it was cut: 26 lines + dc1 do not fit in 400 */
    }

    /* ---- addressing: "to" something else is refused whole */
    SAME(ask("dc1\nto A4CF12B3\nget hello.api\n"), "dc1\nerr to unavailable not this device\n");
    SAME(ask("dc1\nget hello.api\nto A4CF12B3\n"), "dc1\nerr to unavailable not this device\n");   /* also when it comes after the operation */
    SAME(ask("dc1\nto port.A\nset port.A.map.Z rb\n"), "dc1\nerr to unavailable not this device\n");
    CHECK(qn == 0);                                                                       /* ... and nothing is queued */
    CHECK(strstr(ask("dc1\nto iBlueControlMod\nget hello.api\n"), "ok hello.api 1") != NULL);
    CHECK(strstr(ask("dc1\nto IBLUE\nget hello.api\n"), "ok hello.api 1") == NULL);        /* the names are case sensitive */
    CHECK(strstr(ask("dc1\nto\nget hello.api\n"), "ok hello.api 1") != NULL);               /* an empty "to" means this device */
    SAME(ask("dc1\nid 12345678901234567\nget hello.api\n"), "dc1\nid 1234567890\nok hello.api 1\n");   /* the id is cut at 10 characters */

    /* ---- case: output names and input names do not care, the key, the operation and the port letter do */
    SAME(ask("dc1\nget port.A.map.z\nget port.A.map.Z\n"), "dc1\nok port.A.map.Z lb\nok port.A.map.Z lb\n");
    SAME(ask("dc1\nget port.a.map.Z\nget PORT.A.map.Z\nget port.A.MAP.Z\nGET hello.api\n"),
         "dc1\nerr port.a.map.Z unknown\nerr PORT.A.map.Z unknown\nerr port.A.MAP.Z unknown\nerr GET unknown\n");
    SAME(ask("dc1\nget port.A.map.dpad2_up\n"), "dc1\nok port.A.map.DPAD2_UP dpad2.up\n");
    SAME(ask("dc1\nget port.A.map.Dpad*\n"), "dc1\nok port.A.map.DPAD_UP dpad.up\nok port.A.map.DPAD_DOWN dpad.down\nok port.A.map.DPAD_LEFT dpad.left\nok port.A.map.DPAD_RIGHT dpad.right\n"
         "ok port.A.map.DPAD2_UP dpad2.up\nok port.A.map.DPAD2_DOWN dpad2.down\nok port.A.map.DPAD2_LEFT dpad2.left\nok port.A.map.DPAD2_RIGHT dpad2.right\n");
    qn = 0;
    SAME(ask("dc1\nset port.A.map.z LB , Rb:THRESHOLD=5\n"), "dc1\nok port.A.map.Z lb, rb:threshold=5\n");
    CHECK(qn == 1);
    qn = 0;

    /* ---- whitespace */
    SAME(ask("dc1\r\nget port.A.map.Z\r\n"), "dc1\nok port.A.map.Z lb\n");
    SAME(ask("  dc1  \n   get    port.A.map.Z   \n\n\n  \n"), "dc1\nok port.A.map.Z lb\n");
    SAME(ask("dc1\nget\tport.A.map.Z\n"), "dc1\nerr get\tport.A.map.Z unknown\n");                 /* a tab is not a separator */
    SAME(ask("dc1\nget port.A.map.Z\t\n"), "dc1\nerr port.A.map.Z\t unknown\n");
    SAME(ask("dc1 \nget hello.api"), "dc1\nok hello.api 1\n");                                   /* no final newline */
    SAME(ask("dc1\n\rget hello.api\n"), "dc1\nerr \rget unknown\n");                              /* only a CR at the END of a line is dropped */
    {
        const char req[] = "dc1\nget hello.api\n\0get hello.kind\n";                           /* a NUL ends the request, silently */
        iblue_dcapi_handle(req, sizeof(req) - 1, ans, sizeof(ans));
        SAME(ans, "dc1\nok hello.api 1\n");
    }
    SAME(ask("dc1\nset port.A.map.Z   lb   ,   rb  \n"), "dc1\nok port.A.map.Z lb, rb\n");
    CHECK(qn == 1);
    qn = 0;

    /* ---- two ports, and ports that are not there */
    {
        iblue_dcapi_port_set(0, IBLUE_PORT_BT, 0, 0, 1, 1);
        iblue_dcapi_port_set(1, IBLUE_PORT_EMPTY, 0, 0, 0, 0);
        SAME(ask("dc1\nget port.A.map.A\nget port.B.map.A\nget port.C.map.A\nget port.D.map.A\n"),
             "dc1\nok port.A.map.A a\nok port.B.map.A a\nerr port.C.map.A unavailable no such port\nerr port.D.map.A unavailable no such port\n");
        cfg_n[1] = 0;                                                                       /* B has no mapping at all */
        SAME(ask("dc1\nget port.A.map.A\nget port.B.map.A\n"), "dc1\nok port.A.map.A a\nok port.B.map.A none\n");
        qn = 0;
        SAME(ask("dc1\nset port.B.map.A x\nset port.C.map.A x\nset port.D.map.A x\nset port.C.layout all\nset port.E.map.A x\n"),
             "dc1\nok port.B.map.A x\nerr port.C.map.A unavailable no such port\nerr port.D.map.A unavailable no such port\nerr port.C.layout unavailable no such port\nerr port.E.map.A unknown\n");
        CHECK(qn == 1 && q[0].op == IBLUE_CFG_MAP_SET_T && q[0].a == 1 && q[0].n == 1);   /* only B's set queued something */
        drain();
        SAME(ask("dc1\nget port.B.map.A\nget port.A.map.A\n"), "dc1\nok port.B.map.A x\nok port.A.map.A a\n");     /* two ports are independent */
        factory(1);
        iblue_dcapi_port_count(3);
        CHECK(strstr(ask("dc1\nget port.C.state\n"), "ok port.C.state") != NULL);
        SAME(ask("dc1\nget port.C.map.A\n"), "dc1\nok port.C.map.A none\n");                  /* the table of C is empty in this model */
        iblue_dcapi_port_count(2);
    }

    /* ---- a refused set queues nothing; a good one queues 1 + n messages, the clear first, the adds in the order of the text */
    {
        static const char *const texts[] = {"none", "a", "lb, rb", "lt:threshold=78, rt, a", "back, start, guide, share"};
        static const char *const bad[] = {"zz", "a,a", "a:deadzone=9", "a,b,x,y,lb", "a,,b", "a:scale=300", "a:", ","};
        for (uint32_t port = 0; port < 2; port++) {
            for (uint32_t t = 0; t < 5; t++) {
                char req[200];
                int want = t == 0 ? 0 : 1 + (int)count_of(texts[t], ",");
                qn = 0;
                snprintf(req, sizeof(req), "dc1\nset port.%c.map.START %s\n", (char)('A' + port), texts[t]);
                CHECK(strstr(ask(req), "ok port.") == ans + 4);
                CHECK(qn == 1);
                CHECK(q[0].op == IBLUE_CFG_MAP_SET_T && q[0].a == port && q[0].b == iblue_out_find("START", 5) && q[0].n == want);
                if (t == 3) { CHECK(q[0].e[0].src_btn == IBM_LM && q[0].e[0].perc_threshold == 78 && q[0].e[1].src_btn == IBM_RM && q[0].e[2].src_btn == IBM_RB_DOWN); }
                if (t == 4) { CHECK(q[0].e[0].src_btn == IBM_MS && q[0].e[1].src_btn == IBM_MM && q[0].e[2].src_btn == 22 && q[0].e[3].src_btn == 23); }
            }
            for (uint32_t t = 0; t < 8; t++) {
                char req[200];
                qn = 0;
                snprintf(req, sizeof(req), "dc1\nset port.%c.map.Z %s\n", (char)('A' + port), bad[t]);
                CHECK(strncmp(ask(req), "dc1\nerr port.", 13) == 0);
                CHECK(qn == 0);
            }
        }
        qn = 0;
        factory(0);
        factory(1);
    }

    /* ---- a table with no room (keyboard mode: nothing may be dropped): err full, nothing queued */
    {
        qn = 0;
        iblue_dcapi_port_set(0, IBLUE_PORT_BT, 2, 0, 1, 1);
        factory(0);
        for (; cfg_n[0] < 127;) { cfg_map[0][cfg_n[0]++] = (struct iblue_mapent){50, 40, 0, 100, 50, 135, 0, 0}; }   /* other destinations: never touched */
        SAME(ask("dc1\nset port.A.map.Z lb, rb, lt, rt\n"), "dc1\nerr port.A.map.Z full no room in the mapping table\n");
        CHECK(qn == 0);
        iblue_dcapi_port_set(0, IBLUE_PORT_BT, 0, 0, 1, 1);
        factory(0);
    }

    /* ---- outside the keyboard mode the identity entries of keyboard and mouse are dropped when the room is needed; pad entries and combos stay */
    {
        size_t combos = 0, pad_entries = 0;
        qn = 0;
        factory(0);
        SAME(ask("dc1\nset port.A.map.Z lb, rb, lt, rt\nset port.A.map.C lb, rb, lt, rt\nset port.A.map.X lb, rb, lt, rt\nset port.A.map.Y lb, rb, lt, rt\n"
                 "set port.A.map.START a, b, x, y\nset port.A.map.D lb, rb, lt, rt\n"),
             "dc1\nok port.A.map.Z lb, rb, lt, rt\nok port.A.map.C lb, rb, lt, rt\nok port.A.map.X lb, rb, lt, rt\nok port.A.map.Y lb, rb, lt, rt\n"
             "ok port.A.map.START a, b, x, y\nok port.A.map.D lb, rb, lt, rt\n");
        drain();
        for (size_t i = 0; i < cfg_n[0]; i++) { combos += cfg_map[0][i].dst_btn >= 118; pad_entries += cfg_map[0][i].dst_btn < 32; }
        CHECK(combos == 10 && cfg_n[0] <= 128 && pad_entries == 32 - 6 + 6 * 4);   /* 32 pad ids; six outputs of one entry now have four */
        SAME(ask("dc1\nget port.A.map.Z\nget port.A.map.START\nget port.A.map.A\n"), "dc1\nok port.A.map.Z lb, rb, lt, rt\nok port.A.map.START a, b, x, y\nok port.A.map.A a\n");
        factory(0);
    }

    /* ---- whole pipeline through the model: several sets, then the new table is what get shows */
    {
        qn = 0;
        factory(0);
        SAME(ask("dc1\nset port.A.map.C lt:threshold=78, rb\nset port.A.map.D none\nset port.A.map.Z lb\nget port.A.map.C\n"),
             "dc1\nok port.A.map.C lt:threshold=78, rb\nok port.A.map.D none\nok port.A.map.Z lb\nok port.A.map.C rb\n");   /* the get in the same request still shows the old line */
        CHECK(qn == 3);   /* one message per output: C, D, Z */
        drain();
        SAME(ask("dc1\nget port.A.map.C\nget port.A.map.D\nget port.A.map.Z\n"), "dc1\nok port.A.map.C lt:threshold=78, rb\nok port.A.map.D none\nok port.A.map.Z lb\n");
        CHECK(cfg_n[0] == 125);                                                                /* C: -1 +2, D: -1, Z: -1 +1 */
    }

    /* ---- layout set queues one message; a refused one none */
    {
        qn = 0;
        factory(0);
        SAME(ask("dc1\nset port.A.layout standard\nset port.A.layout nope\n"), "dc1\nok port.A.layout standard\nerr port.A.layout range standard or all\n");
        CHECK(qn == 1 && q[0].op == IBLUE_CFG_SET_LAYOUT && q[0].a == 0 && q[0].b == 0);
        drain();
        CHECK(iblue_layout_match(cfg_map[0], cfg_n[0], 0) == 0);
        qn = 0;
    }

    printf("engine: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
