/* iBlueControlMod: the shared text form of a controller mapping. See iblue_mapcfg.h. */

#include <string.h>
#include <esp_attr.h>
#include "system/iblue_mapcfg.h"

/* ---- names, in RAM (this code runs in the Maple interrupt as well) */

/* input names by BlueRetro generic id; "" = no name. The Xbox column of the hosted web config (web/utils/constants.js) is the source. */
static DRAM_ATTR const char in_names[IBM_PAD_IDS][14] = {
    "lstick.left", "lstick.right", "lstick.down", "lstick.up",
    "rstick.left", "rstick.right", "rstick.down", "rstick.up",
    "dpad.left", "dpad.right", "dpad.down", "dpad.up",
    "dpad2.left", "dpad2.right", "dpad2.down", "dpad2.up",
    "x", "b", "a", "y",
    "start", "back", "guide", "share",
    "lt", "lb", "", "lstick.click",
    "rt", "rb", "", "rstick.click",
};

/* Pico2Maple's and BlueRetro's spellings, read as the same inputs */
static DRAM_ATTR const struct { char name[12]; uint8_t id; } in_alias[] = {
    {"a1_left", IBM_LX_LEFT}, {"a1_right", IBM_LX_RIGHT}, {"a1_down", IBM_LY_DOWN}, {"a1_up", IBM_LY_UP},
    {"a2_left", IBM_RX_LEFT}, {"a2_right", IBM_RX_RIGHT}, {"a2_down", IBM_RY_DOWN}, {"a2_up", IBM_RY_UP},
    {"dpad_left", IBM_LD_LEFT}, {"dpad_right", IBM_LD_RIGHT}, {"dpad_down", IBM_LD_DOWN}, {"dpad_up", IBM_LD_UP},
    {"dpad2_left", IBM_RD_LEFT}, {"dpad2_right", IBM_RD_RIGHT}, {"dpad2_down", IBM_RD_DOWN}, {"dpad2_up", IBM_RD_UP},
    {"select", IBM_MS}, {"menu", IBM_MM}, {"view", IBM_MS}, {"l1", IBM_LS}, {"r1", IBM_RS}, {"l2", IBM_LM}, {"r2", IBM_RM},
};

static DRAM_ATTR const struct { char name[12]; uint8_t dst; } outs[IBLUE_OUT_COUNT] = {
    {"A", IBM_RB_DOWN}, {"B", IBM_RB_RIGHT}, {"X", IBM_RB_LEFT}, {"Y", IBM_RB_UP},
    {"START", IBM_MM}, {"D", IBM_MS}, {"Z", IBM_LS}, {"C", IBM_RS}, {"LT", IBM_LM}, {"RT", IBM_RM},
    {"A1_UP", IBM_LY_UP}, {"A1_DOWN", IBM_LY_DOWN}, {"A1_LEFT", IBM_LX_LEFT}, {"A1_RIGHT", IBM_LX_RIGHT},
    {"A2_UP", IBM_RY_UP}, {"A2_DOWN", IBM_RY_DOWN}, {"A2_LEFT", IBM_RX_LEFT}, {"A2_RIGHT", IBM_RX_RIGHT},
    {"DPAD_UP", IBM_LD_UP}, {"DPAD_DOWN", IBM_LD_DOWN}, {"DPAD_LEFT", IBM_LD_LEFT}, {"DPAD_RIGHT", IBM_LD_RIGHT},
    {"DPAD2_UP", IBM_RD_UP}, {"DPAD2_DOWN", IBM_RD_DOWN}, {"DPAD2_LEFT", IBM_RD_LEFT}, {"DPAD2_RIGHT", IBM_RD_RIGHT},
};

static DRAM_ATTR const char algo_names[6][12] = {"linear", "aggressive", "relaxed", "wide", "scurve", "passthrough"};
static DRAM_ATTR const uint8_t dz_levels[6] = {0, 135, 203, 208, 213, 218};

#define DSTR(name, text) static DRAM_ATTR const char name[] = text
DSTR(W_NONE, "none");
DSTR(W_OFF, "off");
DSTR(W_INPUT, " range unknown input name");
DSTR(W_OPTION, " range unknown option");
DSTR(W_DZ, " range deadzone off, 1 to 5 or n%");
DSTR(W_THR, " range threshold 0 to 100");
DSTR(W_SCALE, " range scale 0 to 255");
DSTR(W_TURBO, " range turbo 0 to 255");
DSTR(W_ALGO, " range algo linear aggressive relaxed wide scurve passthrough");
DSTR(W_MANY, " range at most 4 inputs per output");
DSTR(W_DUP, " range an input twice");
DSTR(W_EMPTY, " range empty input");
DSTR(W_OPT_DEADZONE, "deadzone");
DSTR(W_OPT_THRESHOLD, "threshold");
DSTR(W_OPT_SCALE, "scale");
DSTR(W_OPT_TURBO, "turbo");
DSTR(W_OPT_ALGO, "algo");
DSTR(W_EQ, "=");
DSTR(W_COLON, ":");
DSTR(W_COMMA, ", ");
DSTR(W_PCT, "%");
DSTR(W_DOT, ".");

/* ---- small helpers (no libc beyond memcpy) */
static char IRAM_ATTR lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

static int IRAM_ATTR eq_ci(const char *s, size_t n, const char *lit) {
    size_t i = 0;

    while (i < n && lit[i] && lower(s[i]) == lower(lit[i])) {
        i++;
    }
    return i == n && !lit[i];
}

static size_t IRAM_ATTR cstr_len(const char *s) {
    size_t n = 0;

    while (s[n]) {
        n++;
    }
    return n;
}

static void IRAM_ATTR trim(const char **s, size_t *n) {
    while (*n && (**s == ' ' || **s == '\t')) {
        (*s)++;
        (*n)--;
    }
    while (*n && ((*s)[*n - 1] == ' ' || (*s)[*n - 1] == '\t')) {
        (*n)--;
    }
}

/* decimal, digits only; 0 = ok */
static int IRAM_ATTR to_uint(const char *s, size_t n, uint32_t *v) {
    uint32_t r = 0;

    if (!n || n > 4) {
        return 1;
    }
    for (size_t i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return 1;
        }
        r = r * 10 + (uint32_t)(s[i] - '0');
    }
    *v = r;
    return 0;
}

/* ---- names */
const char * IRAM_ATTR iblue_out_name(uint32_t i) {
    return i < IBLUE_OUT_COUNT ? outs[i].name : 0;
}

uint8_t IRAM_ATTR iblue_out_dst(uint32_t i) {
    return i < IBLUE_OUT_COUNT ? outs[i].dst : 0xFF;
}

int IRAM_ATTR iblue_out_find(const char *s, size_t n) {
    for (uint32_t i = 0; i < IBLUE_OUT_COUNT; i++) {
        if (eq_ci(s, n, outs[i].name)) {
            return (int)i;
        }
    }
    return -1;
}

const char * IRAM_ATTR iblue_in_name(uint8_t src) {
    return (src < IBM_PAD_IDS && in_names[src][0]) ? in_names[src] : 0;
}

static int IRAM_ATTR in_find(const char *s, size_t n) {
    for (uint32_t i = 0; i < IBM_PAD_IDS; i++) {
        if (in_names[i][0] && eq_ci(s, n, in_names[i])) {
            return (int)i;
        }
    }
    for (uint32_t i = 0; i < sizeof(in_alias) / sizeof(in_alias[0]); i++) {
        if (eq_ci(s, n, in_alias[i].name)) {
            return in_alias[i].id;
        }
    }
    return -1;
}

/* ---- deadzone */
uint8_t IRAM_ATTR iblue_dz_from_level(int level) {
    return (level >= 0 && level <= 5) ? dz_levels[level] : 135;
}

int IRAM_ATTR iblue_dz_level(uint8_t raw) {
    for (int i = 0; i < 6; i++) {
        if (dz_levels[i] == raw) {
            return i;
        }
    }
    return -1;
}

uint32_t IRAM_ATTR iblue_dz_x10000(uint8_t raw) {
    return raw <= 200 ? raw : (uint32_t)(raw - 198) * 100;
}

/* ---- parse */
static int IRAM_ATTR parse_deadzone(const char *v, size_t n, uint8_t *raw) {
    uint32_t x;

    if (eq_ci(v, n, W_OFF)) {
        *raw = 0;
        return 0;
    }
    if (n && v[n - 1] == '%') {
        /* n% or n.m% or n.mm%: the percent of the axis range */
        uint32_t whole = 0, frac = 0, digits = 0, hund;
        size_t i = 0;

        n--;
        while (i < n && v[i] >= '0' && v[i] <= '9' && i < 3) {
            whole = whole * 10 + (uint32_t)(v[i] - '0');
            i++;
        }
        if (!i) {
            return 1;
        }
        if (i < n) {
            if (v[i] != '.') {
                return 1;
            }
            i++;
            while (i < n && digits < 2 && v[i] >= '0' && v[i] <= '9') {
                frac = frac * 10 + (uint32_t)(v[i] - '0');
                digits++;
                i++;
            }
            if (i != n || !digits) {
                return 1;
            }
            if (digits == 1) {
                frac *= 10;
            }
        }
        hund = whole * 100 + frac;
        if (hund <= 200) {
            *raw = (uint8_t)hund;
            return 0;
        }
        if (frac || whole < 3 || whole > 57) {
            return 1;
        }
        *raw = (uint8_t)(whole + 198);
        return 0;
    }
    if (to_uint(v, n, &x) || x > 5) {
        return 1;
    }
    *raw = dz_levels[x];
    return 0;
}

static int IRAM_ATTR parse_option(const char *s, size_t n, struct iblue_mapent *e, const char **why) {
    size_t k = 0;
    uint32_t x;

    while (k < n && s[k] != '=') {
        k++;
    }
    if (k == n) {
        *why = W_OPTION;
        return 1;
    }
    {
        const char *v = s + k + 1;
        size_t vn = n - k - 1;

        trim(&v, &vn);
        if (eq_ci(s, k, W_OPT_DEADZONE)) {
            if (parse_deadzone(v, vn, &e->perc_deadzone)) {
                *why = W_DZ;
                return 1;
            }
        }
        else if (eq_ci(s, k, W_OPT_THRESHOLD)) {
            if (to_uint(v, vn, &x) || x > 100) {
                *why = W_THR;
                return 1;
            }
            e->perc_threshold = (uint8_t)x;
        }
        else if (eq_ci(s, k, W_OPT_SCALE)) {
            if (to_uint(v, vn, &x) || x > 255) {
                *why = W_SCALE;
                return 1;
            }
            e->perc_max = (uint8_t)x;
        }
        else if (eq_ci(s, k, W_OPT_TURBO)) {
            if (to_uint(v, vn, &x) || x > 255) {
                *why = W_TURBO;
                return 1;
            }
            e->turbo = (uint8_t)x;
        }
        else if (eq_ci(s, k, W_OPT_ALGO)) {
            int found = -1;

            for (int i = 0; i < 6; i++) {
                if (eq_ci(v, vn, algo_names[i])) {
                    found = i;
                }
            }
            if (found < 0) {
                *why = W_ALGO;
                return 1;
            }
            e->algo = (uint8_t)found;
        }
        else {
            *why = W_OPTION;
            return 1;
        }
    }
    return 0;
}

int IRAM_ATTR iblue_map_parse(const char *val, size_t len, uint32_t out, uint8_t port, struct iblue_mapent *ents, const char **why) {
    int cnt = 0;
    size_t pos = 0;

    trim(&val, &len);
    if (!len || eq_ci(val, len, W_NONE)) {
        return 0;
    }
    while (pos <= len) {
        size_t end = pos;
        const char *tok;
        size_t tn, colon = 0;
        int src;
        struct iblue_mapent *e;

        while (end < len && val[end] != ',') {
            end++;
        }
        tok = val + pos;
        tn = end - pos;
        trim(&tok, &tn);
        if (!tn) {
            *why = W_EMPTY;
            return -1;
        }
        while (colon < tn && tok[colon] != ':') {
            colon++;
        }
        {
            const char *nm = tok;
            size_t nn = colon;

            trim(&nm, &nn);
            src = in_find(nm, nn);
        }
        if (src < 0) {
            *why = W_INPUT;
            return -1;
        }
        if (cnt >= IBLUE_MAP_PER_OUT) {
            *why = W_MANY;
            return -1;
        }
        for (int i = 0; i < cnt; i++) {
            if (ents[i].src_btn == src) {
                *why = W_DUP;
                return -1;
            }
        }
        e = &ents[cnt];
        e->src_btn = (uint8_t)src;
        e->dst_btn = iblue_out_dst(out);
        e->dst_id = port;
        e->perc_max = 100;
        e->perc_threshold = 50;
        e->perc_deadzone = 135;
        e->turbo = 0;
        e->algo = 0;
        while (colon < tn) {
            size_t os = colon + 1, oe = os;
            const char *o;
            size_t on;

            while (oe < tn && tok[oe] != ':') {
                oe++;
            }
            o = tok + os;
            on = oe - os;
            trim(&o, &on);
            if (parse_option(o, on, e, why)) {
                return -1;
            }
            colon = oe;
        }
        cnt++;
        pos = end + 1;
    }
    return cnt;
}

/* ---- render */
struct wr {
    char *p;
    char *end; /* one before the end: the NUL fits */
    int full;
};

static void IRAM_ATTR w_mem(struct wr *w, const char *s, size_t n) {
    if ((size_t)(w->end - w->p) < n) {
        w->full = 1;
        return;
    }
    memcpy(w->p, s, n);
    w->p += n;
}

static void IRAM_ATTR w_str(struct wr *w, const char *s) {
    w_mem(w, s, cstr_len(s));
}

static void IRAM_ATTR w_uint(struct wr *w, uint32_t v) {
    char t[5];
    uint32_t n = 0;

    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < sizeof(t));
    while (n) {
        w_mem(w, &t[--n], 1);
    }
}

static void IRAM_ATTR w_deadzone(struct wr *w, uint8_t raw) {
    int level = iblue_dz_level(raw);

    w_str(w, W_OPT_DEADZONE);
    w_str(w, W_EQ);
    if (level == 0) {
        w_str(w, W_OFF);
    }
    else if (level > 0) {
        w_uint(w, (uint32_t)level);
    }
    else if (raw > 200) {
        w_uint(w, (uint32_t)raw - 198);
        w_str(w, W_PCT);
    }
    else {
        w_uint(w, raw / 100);
        if (raw % 100) {
            w_str(w, W_DOT);
            w_uint(w, (raw % 100) / 10);
            if (raw % 10) {
                w_uint(w, raw % 10);
            }
        }
        w_str(w, W_PCT);
    }
}

size_t IRAM_ATTR iblue_map_render_ex(const struct iblue_mapent *map, size_t n, uint8_t port, uint32_t out, char *buf, size_t cap, int *cut) {
    struct wr w;
    uint8_t dst = iblue_out_dst(out);
    uint32_t cnt = 0;

    if (cut) {
        *cut = 0;
    }
    if (!cap) {
        if (cut) {
            *cut = 1;
        }
        return 0;
    }
    w.p = buf;
    w.end = buf + cap - 1;
    w.full = 0;
    for (size_t i = 0; i < n; i++) {
        const struct iblue_mapent *e = &map[i];
        char *mark = w.p;

        if (e->dst_id != port || e->dst_btn != dst || e->src_btn >= IBM_PAD_IDS || !iblue_in_name(e->src_btn)) {
            continue;
        }
        if (cnt) {
            w_str(&w, W_COMMA);
        }
        w_str(&w, iblue_in_name(e->src_btn));
        if (e->perc_deadzone != 135) {
            w_str(&w, W_COLON);
            w_deadzone(&w, e->perc_deadzone);
        }
        if (e->perc_threshold != 50) {
            w_str(&w, W_COLON);
            w_str(&w, W_OPT_THRESHOLD);
            w_str(&w, W_EQ);
            w_uint(&w, e->perc_threshold > 100 ? 100 : e->perc_threshold); /* the parser takes 0 to 100 only */
        }
        if (e->perc_max != 100) {
            w_str(&w, W_COLON);
            w_str(&w, W_OPT_SCALE);
            w_str(&w, W_EQ);
            w_uint(&w, e->perc_max);
        }
        if (e->turbo) {
            w_str(&w, W_COLON);
            w_str(&w, W_OPT_TURBO);
            w_str(&w, W_EQ);
            w_uint(&w, e->turbo);
        }
        if (e->algo && e->algo < 6) { /* a value no tool of ours writes is left out */
            w_str(&w, W_COLON);
            w_str(&w, W_OPT_ALGO);
            w_str(&w, W_EQ);
            w_str(&w, algo_names[e->algo]);
        }
        if (w.full) {
            w.p = mark; /* cut at an entry */
            break;
        }
        cnt++;
    }
    if (!cnt && !w.full) {
        w_str(&w, W_NONE);
    }
    if (cut) {
        *cut = w.full;
    }
    *w.p = 0;
    return (size_t)(w.p - buf);
}

size_t IRAM_ATTR iblue_map_render(const struct iblue_mapent *map, size_t n, uint8_t port, uint32_t out, char *buf, size_t cap) {
    return iblue_map_render_ex(map, n, port, out, buf, cap, 0);
}

/* ---- replace */
static int IRAM_ATTR is_pad_entry_of(const struct iblue_mapent *e, uint8_t port, uint8_t dst) {
    return e->dst_id == port && e->dst_btn == dst && e->src_btn < IBM_PAD_IDS && iblue_in_name(e->src_btn);
}

size_t IRAM_ATTR iblue_map_replace(struct iblue_mapent *map, size_t n, size_t max, uint8_t port, uint32_t out, const struct iblue_mapent *ents, size_t nents) {
    uint8_t dst = iblue_out_dst(out);
    size_t keep = 0, w = 0;

    for (size_t i = 0; i < n; i++) {
        if (!is_pad_entry_of(&map[i], port, dst)) {
            keep++;
        }
    }
    if (keep + nents > max) {
        return (size_t)-1;
    }
    for (size_t i = 0; i < n; i++) {
        if (!is_pad_entry_of(&map[i], port, dst)) {
            if (w != i) {
                map[w] = map[i];
            }
            w++;
        }
    }
    for (size_t i = 0; i < nents; i++) {
        map[w++] = ents[i];
    }
    return w;
}

/* ---- layouts */
static int IRAM_ATTR is_dc_dst(uint8_t dst) {
    for (uint32_t i = 0; i < IBLUE_OUT_COUNT; i++) {
        if (outs[i].dst == dst) {
            return 1;
        }
    }
    return 0;
}

static int IRAM_ATTR layout_owns(const struct iblue_mapent *e, uint8_t port) {
    return e->dst_id == port && e->src_btn < IBM_PAD_IDS && iblue_in_name(e->src_btn) && is_dc_dst(e->dst_btn);
}

struct preset {
    char out[12];
    char val[30];
};

/* all: BlueRetro's own default for a Dreamcast pad (the bumpers are Z and C, Back is D). Same as Pico2Maple's "all buttons" layout. */
#define PRESET_COMMON \
    {"A1_UP", "lstick.up"}, {"A1_DOWN", "lstick.down"}, {"A1_LEFT", "lstick.left"}, {"A1_RIGHT", "lstick.right"}, \
    {"A2_UP", "rstick.up"}, {"A2_DOWN", "rstick.down"}, {"A2_LEFT", "rstick.left"}, {"A2_RIGHT", "rstick.right"}, \
    {"DPAD_UP", "dpad.up"}, {"DPAD_DOWN", "dpad.down"}, {"DPAD_LEFT", "dpad.left"}, {"DPAD_RIGHT", "dpad.right"}
static DRAM_ATTR const struct preset preset_all[] = {
    {"A", "a"}, {"B", "b"}, {"X", "x"}, {"Y", "y"}, {"START", "start"}, {"D", "back"}, {"Z", "lb"}, {"C", "rb"},
    {"LT", "lt"}, {"RT", "rt"},
    PRESET_COMMON,
    {"DPAD2_UP", "dpad2.up"}, {"DPAD2_DOWN", "dpad2.down"}, {"DPAD2_LEFT", "dpad2.left"}, {"DPAD2_RIGHT", "dpad2.right"},
};
/* standard: the Pico2Maple "standard" layout as read from the Microsap reconstruction (NOT checked on a Pico2Maple): the right bumper is Z,
 * a fully pulled left trigger is C, the left bumper drives the right trigger as a digital full press. D is free. */
static DRAM_ATTR const struct preset preset_standard[] = {
    {"A", "a"}, {"B", "b"}, {"X", "x"}, {"Y", "y"}, {"START", "start"}, {"Z", "rb"}, {"C", "lt:threshold=78"},
    {"LT", "lt"}, {"RT", "lb, rt"},
    PRESET_COMMON,
};
static DRAM_ATTR const char layout_names[IBLUE_LAYOUT_COUNT][9] = {"standard", "all"};

const char * IRAM_ATTR iblue_layout_name(uint32_t i) {
    return i < IBLUE_LAYOUT_COUNT ? layout_names[i] : 0;
}

int IRAM_ATTR iblue_layout_find(const char *s, size_t n) {
    for (uint32_t i = 0; i < IBLUE_LAYOUT_COUNT; i++) {
        if (eq_ci(s, n, layout_names[i])) {
            return (int)i;
        }
    }
    return -1;
}

static const struct preset * IRAM_ATTR preset_rows(uint32_t layout, size_t *count) {
    if (layout == 0) {
        *count = sizeof(preset_standard) / sizeof(preset_standard[0]);
        return preset_standard;
    }
    *count = sizeof(preset_all) / sizeof(preset_all[0]);
    return preset_all;
}

/* a preset row into entries; returns the count (-1 never happens: the tables are fixed and tested) */
static int IRAM_ATTR preset_row_ents(const struct preset *r, uint8_t port, struct iblue_mapent *ents, uint32_t *out) {
    const char *why;
    int o = iblue_out_find(r->out, cstr_len(r->out));

    if (o < 0) {
        return -1;
    }
    *out = (uint32_t)o;
    return iblue_map_parse(r->val, cstr_len(r->val), (uint32_t)o, port, ents, &why);
}

size_t IRAM_ATTR iblue_layout_apply(uint32_t layout, struct iblue_mapent *map, size_t n, size_t max, uint8_t port) {
    size_t rows, keep = 0, w = 0, add = 0;
    const struct preset *p;
    struct iblue_mapent ents[IBLUE_MAP_PER_OUT];
    uint32_t out;

    if (layout >= IBLUE_LAYOUT_COUNT) {
        return (size_t)-1;
    }
    p = preset_rows(layout, &rows);
    for (size_t i = 0; i < n; i++) {
        if (!layout_owns(&map[i], port)) {
            keep++;
        }
    }
    for (size_t r = 0; r < rows; r++) {
        int c = preset_row_ents(&p[r], port, ents, &out);

        if (c < 0) {
            return (size_t)-1;
        }
        add += (size_t)c;
    }
    if (keep + add > max) {
        return (size_t)-1;
    }
    for (size_t i = 0; i < n; i++) {
        if (!layout_owns(&map[i], port)) {
            map[w++] = map[i];
        }
    }
    for (size_t r = 0; r < rows; r++) {
        int c = preset_row_ents(&p[r], port, ents, &out);

        for (int k = 0; k < c; k++) {
            map[w++] = ents[k];
        }
    }
    return w;
}

static int IRAM_ATTR same_entry(const struct iblue_mapent *a, const struct iblue_mapent *b) {
    return a->src_btn == b->src_btn && a->dst_btn == b->dst_btn && a->perc_max == b->perc_max && a->perc_threshold == b->perc_threshold &&
           a->perc_deadzone == b->perc_deadzone && a->turbo == b->turbo && a->algo == b->algo;
}

/* The port equals a preset when, for every output, it has exactly the preset's entries (in any order): compared as entries, not as text */
int IRAM_ATTR iblue_layout_match(const struct iblue_mapent *map, size_t n, uint8_t port) {
    for (uint32_t l = 0; l < IBLUE_LAYOUT_COUNT; l++) {
        size_t rows;
        const struct preset *p = preset_rows(l, &rows);
        int same = 1;

        for (uint32_t o = 0; o < IBLUE_OUT_COUNT && same; o++) {
            struct iblue_mapent ents[IBLUE_MAP_PER_OUT];
            uint8_t dst = iblue_out_dst(o);
            size_t have = 0;
            int c = 0;
            int found[IBLUE_MAP_PER_OUT] = {0, 0, 0, 0};

            for (size_t r = 0; r < rows; r++) {
                if (eq_ci(p[r].out, cstr_len(p[r].out), outs[o].name)) {
                    uint32_t dummy;

                    c = preset_row_ents(&p[r], port, ents, &dummy);
                    break;
                }
            }
            for (size_t i = 0; i < n && same; i++) {
                int hit = 0;

                if (!is_pad_entry_of(&map[i], port, dst)) {
                    continue;
                }
                have++;
                for (int k = 0; k < c; k++) {
                    if (!found[k] && same_entry(&map[i], &ents[k])) {
                        found[k] = 1;
                        hit = 1;
                        break;
                    }
                }
                same = hit;
            }
            same = same && have == (size_t)(c > 0 ? c : 0);
        }
        if (same) {
            return (int)l;
        }
    }
    return -1;
}
