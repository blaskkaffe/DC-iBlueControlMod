/* iBlueControlMod: the adapter's end of the DC API (dc1). See iblue_dcapi.h.
 * Runs in the Maple interrupt as well: everything it reads is in RAM (DRAM_ATTR), nothing from libc but memcpy/memset. */

#include <string.h>
#include <esp_attr.h>
#include "iblue/iblue_dcapi.h"
#include "iblue/iblue_gameid.h"
#include "iblue/iblue_mapcfg.h"

#ifdef CONFIG_BLUERETRO_IBLUE_DCAPI

/* ---- text, in RAM */
#define DSTR(name, text) static DRAM_ATTR const char name[] = text
DSTR(S_LEVEL, "dc1");
DSTR(S_ID, "id");
DSTR(S_TO, "to");
DSTR(S_GET, "get");
DSTR(S_SET, "set");
DSTR(S_CALL, "call");
DSTR(S_OK, "ok ");
DSTR(S_ERR, "err ");
DSTR(S_NL, "\n");
DSTR(S_SP, " ");
DSTR(S_HELLO, "hello");
DSTR(S_EVENTS, "events");
DSTR(S_UNKNOWN, " unknown");
DSTR(S_READONLY, " readonly");
DSTR(S_RANGE_MODE, " range normal dual keyboard mouse racing or maracas");
DSTR(S_RANGE_ACC, " range none vmu rumble or both");
DSTR(S_RANGE_LAYOUT, " range standard or all");
DSTR(S_MAPDOT, ".map.");
DSTR(S_YES, "yes");
DSTR(S_NO, "no");
DSTR(S_DONE, "done");
DSTR(S_NOTHING, "nothing");
DSTR(S_COMMIT, "sys.commit");
DSTR(S_REVERT, "sys.revert");
DSTR(S_NOTSET, " unavailable not set");
DSTR(S_NOGAME, " unavailable no game");
DSTR(S_NOEVENTS, "err events unavailable not implemented\n");
DSTR(S_RANGE, "err net.selected range dcnow or dcnet\n");
DSTR(S_BADFIRST, "err request bad first line must be dc1\n");
DSTR(S_NOTME, "err to unavailable not this device\n");
DSTR(S_KIND, "adapter");
DSTR(S_NAME, "iBlueControlMod");
DSTR(S_API, "1");
DSTR(S_CAPS, "api,net,port,game");
DSTR(S_DCNOW, "dcnow");
DSTR(S_DCNET, "dcnet");
DSTR(S_SELF, "self");
DSTR(S_IBLUE, "iblue");
DSTR(S_UNKNOWN_VERSION, "unknown");
DSTR(S_PORT, "port.");
DSTR(S_EMPTY, " empty give a value or none");
DSTR(S_FULL, " full no room in the mapping table");
DSTR(S_BUSY, " busy try again");
DSTR(S_TOOLONG, "err request too long\n");
DSTR(S_NOPORT, " unavailable no such port");
/* port fields: state mode acc name */
enum { F_STATE, F_MODE, F_ACC, F_NAME, F_MODES, F_ACCS, F_LAYOUT, F_LAYOUTS, F_COUNT };
static DRAM_ATTR const char field_names[F_COUNT][8] = {"state", "mode", "acc", "name", "modes", "accs", "layout", "layouts"};
DSTR(S_MODES, "normal,dual,keyboard,mouse,racing,maracas"); /* what `set port.X.mode` accepts here */
DSTR(S_ACCS, "none,vmu,rumble,both");
DSTR(S_STORE, "commit");
DSTR(S_LAYOUTS, "standard,all,custom");
DSTR(S_CUSTOM, "custom");
static DRAM_ATTR const char state_names[3][6] = {"empty", "bt", "wired"};
#define MODE_COUNT 6 /* BlueRetro dev_mode 0..5: DEV_PAD, DEV_PAD_ALT, DEV_KB, DEV_MOUSE, DEV_RACING, DEV_MARACAS */
static DRAM_ATTR const char mode_names[MODE_COUNT][9] = {"normal", "dual", "keyboard", "mouse", "racing", "maracas"};
static DRAM_ATTR const char acc_names[4][7] = {"none", "vmu", "rumble", "both"};
/* the product name the console reads in the device info (maple.c: ctrl_area_dir_name ...); the dual stick mode has the controller's name */
static DRAM_ATTR const char dc_names[MODE_COUNT][24] = {"Dreamcast Controller", "Dreamcast Controller", "Keyboard", "Dreamcast Mouse",
    "Racing Controller", "Maracas Controller"};
#define PORT_ID_BASE 100 /* key ids of the port keys: 100 + port * 8 + field */

enum { K_KIND, K_NAME, K_VERSION, K_API, K_CAPS, K_GAME, K_NET, K_UNSAVED, K_STORE, K_COUNT };
#define K_HELLO_LAST K_CAPS
static DRAM_ATTR const struct {
    char name[16];
    uint8_t id;
} keytab[K_COUNT] = {
    {"hello.kind", K_KIND}, {"hello.name", K_NAME}, {"hello.version", K_VERSION}, {"hello.api", K_API},
    {"hello.caps", K_CAPS}, {"game.id", K_GAME}, {"net.selected", K_NET}, {"sys.unsaved", K_UNSAVED}, {"sys.store", K_STORE},
};

#define VERSION_MAX 32
static DRAM_ATTR char version[VERSION_MAX + 1];
static DRAM_ATTR volatile uint8_t net_selected; /* 0 = not set, 1 = dcnow, 2 = dcnet */
static DRAM_ATTR volatile uint8_t unsaved; /* a live change waits for sys.commit */
static DRAM_ATTR volatile uint8_t port_cnt;
static DRAM_ATTR volatile uint8_t port_state[IBLUE_DCAPI_PORTS], port_mode[IBLUE_DCAPI_PORTS], port_acc[IBLUE_DCAPI_PORTS];
static DRAM_ATTR volatile uint8_t port_bt_type[IBLUE_DCAPI_PORTS], port_bt_sub[IBLUE_DCAPI_PORTS];
static DRAM_ATTR volatile uint8_t port_layout[IBLUE_DCAPI_PORTS] = {0xFF, 0xFF, 0xFF, 0xFF}; /* index of the matching layout, 0xFF = custom */

/* maple and ble keep their own buffers (static: the interrupt has little stack) */
static DRAM_ATTR char maple_req[IBLUE_DCAPI_REQ_MAX + 1];
static DRAM_ATTR char maple_ans[IBLUE_DCAPI_ANS_MAX + 1];
static DRAM_ATTR char ble_req[IBLUE_DCAPI_REQ_MAX + 1];
static DRAM_ATTR char ble_ans[IBLUE_DCAPI_ANS_MAX + 1];
static DRAM_ATTR size_t ble_req_len, ble_ans_len, ble_pos;
static DRAM_ATTR uint8_t ble_over; /* the request did not fit: it is refused, not cut and applied */

/* ---- output */
struct out {
    char *p;
    char *end; /* one before the end of the buffer: the NUL always fits */
    char *last_nl; /* where the last complete line ended */
    int cut;
};

static void IRAM_ATTR o_mem(struct out *o, const char *s, size_t n) {
    while (n--) {
        if (o->p >= o->end) {
            o->cut = 1;
            return;
        }
        *o->p++ = *s++;
    }
}

static void IRAM_ATTR o_str(struct out *o, const char *s) {
    size_t n = 0;

    while (s[n]) {
        n++;
    }
    o_mem(o, s, n);
}

/* what is out so far is whole lines (unless the buffer ran out: then the answer is cut at the last line that was whole) */
static void IRAM_ATTR o_whole(struct out *o) {
    if (!o->cut) {
        o->last_nl = o->p;
    }
}

static void IRAM_ATTR o_end_line(struct out *o) {
    o_str(o, S_NL);
    o_whole(o);
}

static int IRAM_ATTR eq(const char *s, size_t n, const char *lit) {
    size_t i = 0;

    while (i < n && lit[i] && s[i] == lit[i]) {
        i++;
    }
    return i == n && !lit[i];
}

/* the value of a key into o; 0 = written, 1 = not available (nothing written) */
static int IRAM_ATTR key_value(struct out *o, uint8_t id) {
    switch (id) {
        case K_KIND:
            o_str(o, S_KIND);
            return 0;
        case K_NAME:
            o_str(o, S_NAME);
            return 0;
        case K_VERSION:
            o_str(o, version[0] ? version : S_UNKNOWN_VERSION);
            return 0;
        case K_API:
            o_str(o, S_API);
            return 0;
        case K_CAPS:
            o_str(o, S_CAPS);
            return 0;
        case K_GAME:
            if (!iblue_gameid_current()[0]) {
                return 1;
            }
            o_str(o, iblue_gameid_current());
            return 0;
        case K_UNSAVED:
            o_str(o, unsaved ? S_YES : S_NO);
            return 0;
        case K_STORE:
            o_str(o, S_STORE);
            return 0;
        case K_NET:
            if (!net_selected) {
                return 1;
            }
            o_str(o, net_selected == 2 ? S_DCNET : S_DCNOW);
            return 0;
        default:
            break;
    }
    if (id >= PORT_ID_BASE) {
        uint32_t p = (uint32_t)(id - PORT_ID_BASE) / 8;
        uint32_t f = (uint32_t)(id - PORT_ID_BASE) % 8;

        if (p >= port_cnt || p >= IBLUE_DCAPI_PORTS || f >= F_COUNT) {
            return 1;
        }
        switch (f) {
            case F_STATE:
                o_str(o, state_names[port_state[p] > 2 ? 0 : port_state[p]]);
                return 0;
            case F_MODE:
                o_str(o, mode_names[port_mode[p] < MODE_COUNT ? port_mode[p] : 0]);
                return 0;
            case F_ACC:
                o_str(o, acc_names[port_acc[p] & 3]);
                return 0;
            case F_NAME:
                o_str(o, dc_names[port_mode[p] < MODE_COUNT ? port_mode[p] : 0]);
                return 0;
            case F_MODES:
                o_str(o, S_MODES);
                return 0;
            case F_ACCS:
                o_str(o, S_ACCS);
                return 0;
            case F_LAYOUT:
                o_str(o, port_layout[p] < IBLUE_LAYOUT_COUNT ? iblue_layout_name(port_layout[p]) : S_CUSTOM);
                return 0;
            case F_LAYOUTS:
                o_str(o, S_LAYOUTS);
                return 0;
            default:
                return 1;
        }
    }
    return 1;
}

/* "port.A.state" into buf (at least 16 bytes) */
static void IRAM_ATTR port_key_str(char *buf, uint32_t p, uint32_t f) {
    uint32_t n = 0;

    for (uint32_t i = 0; i < 5; i++) {
        buf[n++] = S_PORT[i];
    }
    buf[n++] = (char)('A' + p);
    buf[n++] = '.';
    for (uint32_t i = 0; field_names[f][i]; i++) {
        buf[n++] = field_names[f][i];
    }
    buf[n] = 0;
}

static void IRAM_ATTR line_ok(struct out *o, uint8_t id) {
    o_str(o, S_OK);
    if (id >= PORT_ID_BASE) {
        char nm[16];

        port_key_str(nm, (uint32_t)(id - PORT_ID_BASE) / 8, (uint32_t)(id - PORT_ID_BASE) % 8);
        o_str(o, nm);
    }
    else {
        o_str(o, keytab[id].name);
    }
    o_str(o, S_SP);
    key_value(o, id);
    o_end_line(o);
}

static void IRAM_ATTR line_err(struct out *o, const char *key, size_t klen, const char *why) {
    o_str(o, S_ERR);
    o_mem(o, key, klen);
    o_str(o, why);
    o_end_line(o);
}

static int IRAM_ATTR has_prefix(const char *name, const char *p, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (name[i] != p[i]) {
            return 0;
        }
    }
    return 1;
}

static int IRAM_ATTR find_key(const char *s, size_t n) {
    for (uint32_t i = 0; i < K_COUNT; i++) {
        if (eq(s, n, keytab[i].name)) {
            return (int)keytab[i].id;
        }
    }
    if (n >= 8 && has_prefix(S_PORT, s, 5) && s[5] >= 'A' && s[5] <= 'D' && s[6] == '.') {
        for (uint32_t f = 0; f < F_COUNT; f++) {
            if (eq(s + 7, n - 7, field_names[f])) {
                return PORT_ID_BASE + (s[5] - 'A') * 8 + (int)f;
            }
        }
    }
    return -1;
}

/* ---- port.<A-D>.map.<OUTPUT> (not in the key table: the outputs are a list of their own, iblue_mapcfg.c) */
/* 1 if key is port.X.map.<something>: the port 0..3 and the part after "map." */
static int IRAM_ATTR map_key(const char *key, size_t klen, uint32_t *port, const char **name, size_t *nlen) {
    if (klen < 12 || !has_prefix(S_PORT, key, 5) || key[5] < 'A' || key[5] > 'D' || !has_prefix(S_MAPDOT, key + 6, 5)) {
        return 0;
    }
    *port = (uint32_t)(key[5] - 'A');
    *name = key + 11;
    *nlen = klen - 11;
    return 1;
}

static void IRAM_ATTR map_key_out(struct out *o, uint32_t port, uint32_t out) {
    char pre[7] = {'p', 'o', 'r', 't', '.', (char)('A' + port), '.'};

    o_str(o, S_OK);
    o_mem(o, pre, 7);
    o_str(o, S_MAPDOT + 1);
    o_str(o, iblue_out_name(out));
    o_str(o, S_SP);
}

static void IRAM_ATTR map_get_line(struct out *o, uint32_t port, uint32_t out) {
    size_t n = 0;
    const struct iblue_mapent *map = iblue_dcapi_cfg_map(port, &n);
    size_t room = (size_t)(o->end - o->p) + 1;

    int cut = 0;

    map_key_out(o, port, out);
    if (!o->cut && room > 1) {
        o->p += iblue_map_render_ex(map, n, (uint8_t)port, out, o->p, (size_t)(o->end - o->p) + 1, &cut);
    }
    else {
        cut = 1;
    }
    if (cut) {
        o->cut = 1; /* a value that does not fit is not shown shortened: the line is left out */
        return;
    }
    o_end_line(o);
}

static char IRAM_ATTR lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; /* letters only: |0x20 made DEL match _ */
}

static void IRAM_ATTR map_get(struct out *o, const char *key, size_t klen, uint32_t port, const char *name, size_t nlen) {
    int rows = 0;

    if (port >= port_cnt || port >= IBLUE_DCAPI_PORTS) {
        line_err(o, key, klen, S_NOPORT);
        return;
    }
    if (nlen && name[nlen - 1] == '*') {
        for (uint32_t i = 0; i < IBLUE_OUT_COUNT; i++) {
            const char *nm = iblue_out_name(i);
            size_t k = 0;

            while (k < nlen - 1 && nm[k] && lower(nm[k]) == lower(name[k])) {
                k++;
            }
            if (k == nlen - 1) {
                map_get_line(o, port, i);
                rows++;
            }
        }
        if (!rows) {
            line_err(o, key, klen, S_UNKNOWN);
        }
        return;
    }
    {
        int out = iblue_out_find(name, nlen);

        if (out < 0) {
            line_err(o, key, klen, S_UNKNOWN);
        }
        else {
            map_get_line(o, port, (uint32_t)out);
        }
    }
}

static void IRAM_ATTR map_set(struct out *o, const char *key, size_t klen, uint32_t port, const char *name, size_t nlen, const char *val, size_t vlen) {
    struct iblue_mapent ents[IBLUE_MAP_PER_OUT];
    const char *why = S_UNKNOWN;
    int out = iblue_out_find(name, nlen);
    int c;

    if (port >= port_cnt || port >= IBLUE_DCAPI_PORTS) {
        line_err(o, key, klen, S_NOPORT);
        return;
    }
    if (out < 0) {
        line_err(o, key, klen, S_UNKNOWN);
        return;
    }
    int cut = 0;

    if (!vlen) {
        line_err(o, key, klen, S_EMPTY); /* "set port.A.map.Z" alone used to clear the output; "none" does that */
        return;
    }
    c = iblue_map_parse(val, vlen, (uint32_t)out, (uint8_t)port, ents, &why);
    if (c < 0) {
        line_err(o, key, klen, why);
        return;
    }
    {
        size_t have = 0;
        const struct iblue_mapent *live = iblue_dcapi_cfg_map(port, &have);

        if (live && iblue_map_size_after(live, have, (uint8_t)port, (uint32_t)out, (size_t)c, port_mode[port] != 2) > IBLUE_MAP_MAX) {
            line_err(o, key, klen, S_FULL);
            return;
        }
    }
    if (!iblue_dcapi_cfg_set_map((uint8_t)port, (uint8_t)out, ents, (size_t)c)) {
        line_err(o, key, klen, S_BUSY); /* nothing was changed */
        return;
    }
    unsaved = 1;
    port_layout[port] = 0xFF; /* until the manager task looks again, it is not a preset any more */
    map_key_out(o, port, (uint32_t)out);
    if (!o->cut) {
        o->p += iblue_map_render_ex(ents, (size_t)c, (uint8_t)port, (uint32_t)out, o->p, (size_t)(o->end - o->p) + 1, &cut);
    }
    else {
        cut = 1;
    }
    if (cut) {
        o->cut = 1;
        return;
    }
    o_end_line(o);
}

/* ---- the operations */
static void IRAM_ATTR op_get(struct out *o, const char *key, size_t klen) {
    int id;

    if (eq(key, klen, S_HELLO)) {
        for (uint32_t i = 0; i <= K_HELLO_LAST; i++) {
            line_ok(o, (uint8_t)i);
        }
        return;
    }
    if (eq(key, klen, S_EVENTS) || (klen > 6 && has_prefix(S_EVENTS, key, 6) && key[6] == ' ')) {
        o_str(o, S_NOEVENTS);
        o_whole(o);
        return;
    }
    {
        uint32_t mp;
        const char *mn;
        size_t mnl;

        if (map_key(key, klen, &mp, &mn, &mnl)) {
            map_get(o, key, klen, mp, mn, mnl);
            return;
        }
    }
    if (klen && key[klen - 1] == '*') {
        int rows = 0;

        for (uint32_t i = 0; i < K_COUNT; i++) {
            if (has_prefix(keytab[i].name, key, klen - 1)) {
                /* a key without a value (no game, network not set) is left out of a list */
                char tmp[2];
                struct out t = {tmp, tmp + 1, tmp, 0};

                if (key_value(&t, (uint8_t)i) == 0) {
                    line_ok(o, (uint8_t)i);
                    rows++;
                }
            }
        }
        for (uint32_t p = 0; p < port_cnt && p < IBLUE_DCAPI_PORTS; p++) {
            for (uint32_t f = 0; f < F_COUNT; f++) {
                char nm[16];
                char tmp[2];
                struct out t = {tmp, tmp + 1, tmp, 0};
                uint8_t pid = (uint8_t)(PORT_ID_BASE + p * 8 + f);

                port_key_str(nm, p, f);
                if (has_prefix(nm, key, klen - 1) && key_value(&t, pid) == 0) {
                    line_ok(o, pid);
                    rows++;
                }
            }
        }
        if (!rows) {
            line_err(o, key, klen, S_UNKNOWN);
        }
        return;
    }
    id = find_key(key, klen);
    if (id < 0) {
        line_err(o, key, klen, S_UNKNOWN);
        return;
    }
    {
        char tmp[2];
        struct out t = {tmp, tmp + 1, tmp, 0};

        if (key_value(&t, (uint8_t)id) != 0) {
            if (id >= PORT_ID_BASE) {
                line_err(o, key, klen, S_NOPORT);
            }
            else {
                line_err(o, key, klen, id == K_GAME ? S_NOGAME : S_NOTSET);
            }
            return;
        }
    }
    line_ok(o, (uint8_t)id);
}

static void IRAM_ATTR op_set(struct out *o, const char *key, size_t klen, const char *val, size_t vlen) {
    int id = find_key(key, klen);
    uint32_t mp;
    const char *mn;
    size_t mnl;

    if (map_key(key, klen, &mp, &mn, &mnl)) {
        map_set(o, key, klen, mp, mn, mnl, val, vlen);
    }
    else if (id < 0) {
        line_err(o, key, klen, S_UNKNOWN);
    }
    else if (id >= PORT_ID_BASE) {
        uint32_t p = (uint32_t)(id - PORT_ID_BASE) / 8;
        uint32_t f = (uint32_t)(id - PORT_ID_BASE) % 8;
        int v = -1;

        if (f == F_LAYOUT) {
            int l = iblue_layout_find(val, vlen);

            if (p >= port_cnt || p >= IBLUE_DCAPI_PORTS) {
                line_err(o, key, klen, S_NOPORT);
            }
            else if (l < 0) {
                line_err(o, key, klen, S_RANGE_LAYOUT);
            }
            else {
                /* a mapping only: no wired restart. Applied in normal code; not stored until call sys.commit */
                if (!iblue_dcapi_cfg_request(IBLUE_CFG_SET_LAYOUT, (uint8_t)p, (uint8_t)l)) {
                    line_err(o, key, klen, S_BUSY);
                    return;
                }
                port_layout[p] = (uint8_t)l;
                unsaved = 1;
                line_ok(o, (uint8_t)id);
            }
        }
        else if (f != F_MODE && f != F_ACC) {
            line_err(o, key, klen, S_READONLY);
        }
        else if (p >= port_cnt || p >= IBLUE_DCAPI_PORTS) {
            line_err(o, key, klen, S_NOPORT);
        }
        else {
            for (uint32_t i = 0; i < (f == F_MODE ? MODE_COUNT : 4); i++) {
                if (eq(val, vlen, f == F_MODE ? mode_names[i] : acc_names[i])) {
                    v = (int)i;
                }
            }
            if (v < 0) {
                line_err(o, key, klen, f == F_MODE ? S_RANGE_MODE : S_RANGE_ACC);
            }
            else {
                if ((f == F_MODE ? port_mode[p] : port_acc[p]) != v) {
                    /* applied at once in normal code (the console re-enumerates), not stored */
                    if (!iblue_dcapi_cfg_request(f == F_MODE ? IBLUE_CFG_SET_MODE : IBLUE_CFG_SET_ACC, (uint8_t)p, (uint8_t)v)) {
                        line_err(o, key, klen, S_BUSY);
                        return;
                    }
                    if (f == F_MODE) {
                        port_mode[p] = (uint8_t)v;
                    }
                    else {
                        port_acc[p] = (uint8_t)v;
                    }
                    unsaved = 1;
                }
                line_ok(o, (uint8_t)id);
            }
        }
    }
    else if (id != K_NET) {
        line_err(o, key, klen, S_READONLY); /* sys.unsaved, game.id and hello.* too */
    }
    else if (eq(val, vlen, S_DCNOW)) {
        net_selected = 1;
        line_ok(o, K_NET);
    }
    else if (eq(val, vlen, S_DCNET)) {
        net_selected = 2;
        line_ok(o, K_NET);
    }
    else {
        o_str(o, S_RANGE);
        o_whole(o);
    }
}

/* ---- the engine */
size_t IRAM_ATTR iblue_dcapi_handle(const char *req, size_t req_len, char *out, size_t out_cap) {
    struct out o;
    size_t pos = 0;
    int first = 1;
    int addressed = 1;
    char id_txt[11];
    size_t id_len = 0;
    const char *ops[8]; /* few and small: this runs in the Maple interrupt with little stack */
    size_t ops_len[8];
    uint32_t nops = 0;

    if (!out_cap) {
        return 0;
    }
    o.p = out;
    o.end = out + out_cap - 1;
    o.last_nl = out;
    o.cut = 0;
    while (pos < req_len && req[pos]) {
        const char *line = req + pos;
        size_t n = 0;
        size_t b;
        size_t w;

        while (pos + n < req_len && req[pos + n] && req[pos + n] != '\n') {
            n++;
        }
        pos += n;
        if (pos < req_len && req[pos] == '\n') {
            pos++;
        }
        while (n && (line[n - 1] == '\r' || line[n - 1] == ' ')) {
            n--;
        }
        while (n && (line[0] == ' ')) {
            line++;
            n--;
        }
        if (!n) {
            continue;
        }
        if (first) {
            first = 0;
            if (!eq(line, n, S_LEVEL)) {
                o_str(&o, S_LEVEL);
                o_str(&o, S_NL);
                o_str(&o, S_BADFIRST);
                o_whole(&o);
                goto done;
            }
            continue;
        }
        for (w = 0; w < n && line[w] != ' '; w++) {
        }
        b = w;
        while (b < n && line[b] == ' ') {
            b++;
        }
        if (eq(line, w, S_ID)) {
            id_len = n - b > 10 ? 10 : n - b;
            for (size_t i = 0; i < id_len; i++) {
                id_txt[i] = line[b + i];
            }
        }
        else if (eq(line, w, S_TO)) {
            size_t tl = n - b;

            addressed = eq(line + b, tl, S_SELF) || eq(line + b, tl, S_IBLUE) || eq(line + b, tl, S_NAME) || !tl;
        }
        else if (nops < 8) {
            ops[nops] = line;
            ops_len[nops++] = n;
        }
    }
    if (first) {
        o_str(&o, S_LEVEL);
        o_str(&o, S_NL);
        o_str(&o, S_BADFIRST);
        o_whole(&o);
        goto done;
    }
    o_str(&o, S_LEVEL);
    o_end_line(&o);
    if (id_len) {
        o_str(&o, S_ID);
        o_str(&o, S_SP);
        o_mem(&o, id_txt, id_len);
        o_end_line(&o);
    }
    if (!addressed) {
        o_str(&o, S_NOTME);
        o_whole(&o);
        goto done;
    }
    for (uint32_t i = 0; i < nops; i++) {
        const char *line = ops[i];
        size_t n = ops_len[i];
        size_t w = 0;
        size_t b;
        size_t e;

        while (w < n && line[w] != ' ') {
            w++;
        }
        b = w;
        while (b < n && line[b] == ' ') {
            b++;
        }
        e = b;
        while (e < n && line[e] != ' ') {
            e++;
        }
        if (eq(line, w, S_GET)) {
            /* "get events since=N" keeps its argument: the key is everything after "get " */
            op_get(&o, line + b, n - b);
        }
        else if (eq(line, w, S_SET)) {
            size_t v = e;

            while (v < n && line[v] == ' ') {
                v++;
            }
            op_set(&o, line + b, e - b, line + v, n - v);
        }
        else if (eq(line, w, S_CALL)) {
            if (eq(line + b, e - b, S_COMMIT) || eq(line + b, e - b, S_REVERT)) {
                int commit = eq(line + b, e - b, S_COMMIT);

                o_str(&o, S_OK);
                o_mem(&o, line + b, e - b);
                o_str(&o, S_SP);
                if (unsaved) {
                    if (iblue_dcapi_cfg_request(commit ? IBLUE_CFG_COMMIT : IBLUE_CFG_REVERT, 0, 0)) {
                        unsaved = 0;
                        o_str(&o, S_DONE);
                    }
                    else {
                        o_str(&o, S_BUSY + 1); /* dropped: still unsaved, try again */
                    }
                }
                else {
                    o_str(&o, S_NOTHING);
                }
                o_end_line(&o);
            }
            else {
                line_err(&o, line + b, e - b, S_UNKNOWN);
            }
        }
        else {
            line_err(&o, line, w, S_UNKNOWN);
        }
    }
done:
    if (o.cut) {
        o.p = o.last_nl; /* an answer that does not fit is cut at the last whole line */
    }
    *o.p = 0;
    return (size_t)(o.p - out);
}

void iblue_dcapi_unsaved_clear(void) {
    unsaved = 0;
}

void iblue_dcapi_init(const char *v) {
    size_t i = 0;

    while (v && v[i] && i < VERSION_MAX) {
        char c = v[i];

        version[i] = (c == ' ' || c == '\n' || c == '\r') ? '_' : c;
        i++;
    }
    version[i] = 0;
}

/* ---- the ports, from normal code */
void iblue_dcapi_port_count(uint32_t count) {
    port_cnt = (uint8_t)(count > IBLUE_DCAPI_PORTS ? IBLUE_DCAPI_PORTS : count);
}

void iblue_dcapi_port_set(uint32_t port, uint8_t state, uint8_t dev_mode, uint8_t acc_mode, uint8_t bt_type, uint8_t bt_subtype) {
    if (port >= IBLUE_DCAPI_PORTS) {
        return;
    }
    port_state[port] = state;
    port_mode[port] = dev_mode;
    port_acc[port] = acc_mode;
    port_bt_type[port] = bt_type;
    port_bt_sub[port] = bt_subtype;
}

void iblue_dcapi_port_layout(uint32_t port, int layout) {
    if (port < IBLUE_DCAPI_PORTS) {
        port_layout[port] = (uint8_t)(layout >= 0 && layout < IBLUE_LAYOUT_COUNT ? layout : 0xFF);
    }
}

size_t iblue_dcapi_port_status(uint8_t *out) {
    size_t n = 0;

    out[n++] = port_cnt;
    for (uint32_t p = 0; p < port_cnt && p < IBLUE_DCAPI_PORTS; p++) {
        out[n++] = port_state[p];
        out[n++] = port_mode[p];
        out[n++] = port_acc[p];
        out[n++] = port_bt_type[p];
        out[n++] = port_bt_sub[p];
    }
    return n;
}

/* a request longer than IBLUE_DCAPI_REQ_MAX is refused whole: a cut line could be applied with a changed meaning ("turbo=25" for "turbo=255") */
static size_t IRAM_ATTR too_long(char *out, size_t cap) {
    size_t n = 0;

    while (S_TOOLONG[n] && n + 1 < cap) {
        out[n] = S_TOOLONG[n];
        n++;
    }
    out[n] = 0;
    return n;
}

/* ---- Maple: text on the bus travels with every 4 bytes reversed */
size_t IRAM_ATTR iblue_dcapi_maple(const uint8_t *wire, size_t n, uint8_t *out_wire) {
    size_t len;
    size_t words;

    if (n > IBLUE_DCAPI_REQ_MAX) {
        len = too_long(maple_ans, sizeof(maple_ans));
    }
    else {
        for (size_t w = 0; w < n; w += 4) {
            for (size_t j = 0; j < 4; j++) {
                size_t i = w + 3 - j;

                maple_req[w + j] = i < n ? (char)wire[i] : 0;
            }
        }
        /* n may not be a multiple of 4 on a damaged frame: the reversal above reads inside the buffer only */
        len = iblue_dcapi_handle(maple_req, n, maple_ans, sizeof(maple_ans));
    }
    words = (len + 3) / 4;
    for (size_t w = 0; w < words * 4; w += 4) {
        for (size_t j = 0; j < 4; j++) {
            size_t i = w + 3 - j;

            out_wire[w + j] = i < len ? (uint8_t)maple_ans[i] : 0;
        }
    }
    return words;
}

void IRAM_ATTR iblue_dcapi_ext_caps(uint8_t *wire24) {
    static DRAM_ATTR const char hex[] = "0123456789abcdef";
    char txt[24];

    memset(txt, 0, sizeof(txt));
    txt[0] = 'd';
    txt[1] = 'c';
    txt[2] = '1';
    txt[3] = ' ';
    for (uint32_t i = 0; i < 8; i++) {
        txt[4 + i] = hex[(IBLUE_DCAPI_CAPS >> (28 - 4 * i)) & 0xF];
    }
    for (uint32_t w = 0; w < 24; w += 4) {
        for (uint32_t j = 0; j < 4; j++) {
            wire24[w + j] = (uint8_t)txt[w + 3 - j];
        }
    }
}

/* ---- BLE: chunks in, chunks out */
void iblue_dcapi_ble_write(const uint8_t *data, size_t len) {
    size_t n;

    if (len < 1) {
        return;
    }
    if (data[0] & 0x01) {
        ble_req_len = 0;
        ble_over = 0;
        ble_ans_len = 0;
        ble_pos = 0;
    }
    for (n = 1; n < len; n++) {
        if (ble_req_len < IBLUE_DCAPI_REQ_MAX) {
            ble_req[ble_req_len++] = (char)data[n];
        }
        else {
            ble_over = 1;
        }
    }
    if (data[0] & 0x02) {
        ble_ans_len = ble_over ? too_long(ble_ans, sizeof(ble_ans)) : iblue_dcapi_handle(ble_req, ble_req_len, ble_ans, sizeof(ble_ans));
        ble_pos = 0;
    }
}

size_t iblue_dcapi_ble_read(uint8_t *out, size_t max) {
    size_t n;

    if (max < 2) {
        return 0;
    }
    n = ble_ans_len - ble_pos;
    if (n > max - 1) {
        n = max - 1;
    }
    memcpy(out + 1, ble_ans + ble_pos, n);
    ble_pos += n;
    out[0] = ble_pos < ble_ans_len ? 1 : 0;
    return n + 1;
}

#endif /* CONFIG_BLUERETRO_IBLUE_DCAPI */
