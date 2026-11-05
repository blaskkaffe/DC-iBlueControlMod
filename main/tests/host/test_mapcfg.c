/* Host test of the shared mapping text form (system/iblue_mapcfg.c): see run.sh */
#include <stdio.h>
#include <string.h>
#include "system/iblue_mapcfg.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define SAME(got, want) do { checks++; if (strcmp((got), (want)) != 0) { fails++; printf("FAIL %s:%d:\n got:  %s\n want: %s\n", __FILE__, __LINE__, got, want); } } while (0)

#define MAXE 400
static struct iblue_mapent map[MAXE];
static size_t n;

/* what config_init_struct() builds: identity for the first KBM_MAX (115) ids, then the ten combo entries (dst 118..127) */
static void default_map(uint8_t port) {
    n = 0;
    memset(map, 0, sizeof(map));
    for (uint32_t i = 0; i < 115; i++) {
        map[n++] = (struct iblue_mapent){(uint8_t)i, (uint8_t)i, port, 100, 50, 135, 0, 0};
    }
    for (uint32_t k = 0; k < 10; k++) {
        map[n++] = (struct iblue_mapent){(uint8_t)(20 + k), (uint8_t)(118 + k), port, 100, 50, 135, 0, 0};
    }
}

static char buf[64];
static const char *line(uint8_t port, const char *out) {
    int o = iblue_out_find(out, strlen(out));
    if (o < 0) { return "NO SUCH OUTPUT"; }
    iblue_map_render(map, n, port, (uint32_t)o, buf, sizeof(buf));
    return buf;
}

static int set(uint8_t port, const char *out, const char *val, const char **why) {
    struct iblue_mapent e[IBLUE_MAP_PER_OUT];
    int o = iblue_out_find(out, strlen(out));
    int c = iblue_map_parse(val, strlen(val), (uint32_t)o, port, e, why);
    if (c < 0) { return c; }
    size_t r = iblue_map_replace(map, n, MAXE, port, (uint32_t)o, e, (size_t)c);
    if (r == (size_t)-1) { return -2; }
    n = r;
    return c;
}

int main(void) {
    const char *why = "";

    CHECK(sizeof(struct iblue_mapent) == 8);

    /* names */
    CHECK(IBLUE_OUT_COUNT == 26);
    for (uint32_t i = 0; i < IBLUE_OUT_COUNT; i++) {
        CHECK(iblue_out_name(i) != NULL);
        CHECK(iblue_out_find(iblue_out_name(i), strlen(iblue_out_name(i))) == (int)i);
        for (uint32_t j = i + 1; j < IBLUE_OUT_COUNT; j++) {
            CHECK(strcmp(iblue_out_name(i), iblue_out_name(j)) != 0);
            CHECK(iblue_out_dst(i) != iblue_out_dst(j));
        }
    }
    CHECK(iblue_out_find("a1_up", 5) == iblue_out_find("A1_UP", 5));
    CHECK(iblue_out_find("Q", 1) == -1);
    CHECK(iblue_out_name(26) == NULL);
    CHECK(iblue_out_dst(iblue_out_find("A", 1)) == IBM_RB_DOWN);   /* Xbox A = BlueRetro RB_DOWN */
    CHECK(iblue_out_dst(iblue_out_find("X", 1)) == IBM_RB_LEFT);
    CHECK(iblue_out_dst(iblue_out_find("START", 5)) == IBM_MM);
    CHECK(iblue_out_dst(iblue_out_find("D", 1)) == IBM_MS);
    CHECK(iblue_out_dst(iblue_out_find("Z", 1)) == IBM_LS);
    CHECK(iblue_out_dst(iblue_out_find("C", 1)) == IBM_RS);
    CHECK(iblue_out_dst(iblue_out_find("LT", 2)) == IBM_LM);
    CHECK(iblue_out_dst(iblue_out_find("RT", 2)) == IBM_RM);
    SAME(iblue_in_name(IBM_RB_DOWN), "a");
    SAME(iblue_in_name(IBM_LS), "lb");
    SAME(iblue_in_name(IBM_LM), "lt");
    SAME(iblue_in_name(IBM_MS), "back");
    CHECK(iblue_in_name(IBM_LT) == NULL);
    CHECK(iblue_in_name(40) == NULL);

    /* deadzone codec */
    CHECK(iblue_dz_from_level(0) == 0);
    CHECK(iblue_dz_from_level(1) == 135);
    CHECK(iblue_dz_from_level(2) == 203);
    CHECK(iblue_dz_from_level(5) == 218);
    CHECK(iblue_dz_from_level(9) == 135);
    CHECK(iblue_dz_level(135) == 1 && iblue_dz_level(0) == 0 && iblue_dz_level(213) == 4 && iblue_dz_level(77) == -1);
    CHECK(iblue_dz_x10000(135) == 135);                     /* unchanged for every old value */
    CHECK(iblue_dz_x10000(200) == 200);
    CHECK(iblue_dz_x10000(201) == 300);                     /* 3 % */
    CHECK(iblue_dz_x10000(203) == 500);                     /* level 2 = 5 % */
    CHECK(iblue_dz_x10000(208) == 1000);                    /* level 3 = 10 % */
    CHECK(iblue_dz_x10000(213) == 1500);
    CHECK(iblue_dz_x10000(218) == 2000);                    /* level 5 = 20 % */
    CHECK(iblue_dz_x10000(218) == 2000 && iblue_dz_x10000(255) == 2000);   /* nothing above 20 % */
    for (uint32_t r = 0; r < 200; r++) { CHECK(iblue_dz_x10000((uint8_t)r) == r); }

    /* parse: plain, defaults */
    default_map(0);
    SAME(line(0, "A"), "a");
    SAME(line(0, "Z"), "lb");
    SAME(line(0, "C"), "rb");
    SAME(line(0, "D"), "back");
    SAME(line(0, "START"), "start");
    SAME(line(0, "LT"), "lt");
    SAME(line(0, "A1_UP"), "lstick.up");
    SAME(line(0, "DPAD2_LEFT"), "dpad2.left");
    SAME(line(1, "A"), "none");                             /* another port has no entries here */

    /* set: one input, several, none, options */
    CHECK(set(0, "A", "b", &why) == 1);
    SAME(line(0, "A"), "b");
    CHECK(set(0, "A", "  x , y  ", &why) == 2);
    SAME(line(0, "A"), "x, y");
    CHECK(set(0, "A", "none", &why) == 0);
    SAME(line(0, "A"), "none");
    CHECK(set(0, "A", "", &why) == 0);
    CHECK(set(0, "LT", "lt:threshold=78", &why) == 1);
    SAME(line(0, "LT"), "lt:threshold=78");
    CHECK(set(0, "LT", "lt:deadzone=3:scale=80:turbo=4:algo=relaxed", &why) == 1);
    SAME(line(0, "LT"), "lt:deadzone=3:scale=80:turbo=4:algo=relaxed");
    CHECK(set(0, "LT", "LT:DEADZONE=OFF", &why) == 1);      /* case does not matter */
    SAME(line(0, "LT"), "lt:deadzone=off");
    CHECK(set(0, "LT", "lt:deadzone=1", &why) == 1);        /* level 1 is the default and is not written */
    SAME(line(0, "LT"), "lt");
    CHECK(set(0, "LT", "lt:deadzone=10%", &why) == 1);
    SAME(line(0, "LT"), "lt:deadzone=3");
    CHECK(set(0, "LT", "lt:deadzone=7%", &why) == 1);
    SAME(line(0, "LT"), "lt:deadzone=7%");
    CHECK(set(0, "LT", "lt:deadzone=0.5%", &why) == 1);
    SAME(line(0, "LT"), "lt:deadzone=0.5%");
    CHECK(set(0, "LT", "lt:deadzone=1.35%", &why) == 1);
    SAME(line(0, "LT"), "lt");
    CHECK(set(0, "LT", "lt:deadzone=2.5%", &why) < 0);      /* 2.5 % cannot be stored */
    SAME(why, " range deadzone off, 1 to 5 or n%");
    CHECK(set(0, "LT", "lt:deadzone=6", &why) < 0);
    CHECK(set(0, "LT", "lt:deadzone=60%", &why) < 0);
    CHECK(set(0, "LT", "lt:threshold=101", &why) < 0);
    SAME(why, " range threshold 0 to 100");
    CHECK(set(0, "LT", "lt:scale=256", &why) < 0);
    CHECK(set(0, "LT", "lt:turbo=x", &why) < 0);
    CHECK(set(0, "LT", "lt:algo=zigzag", &why) < 0);
    CHECK(set(0, "LT", "lt:speed=3", &why) < 0);
    SAME(why, " range unknown option");
    CHECK(set(0, "LT", "lt:threshold", &why) < 0);
    CHECK(set(0, "LT", "zz", &why) < 0);
    SAME(why, " range unknown input name");
    CHECK(set(0, "LT", "lt,lt", &why) < 0);
    SAME(why, " range an input twice");
    CHECK(set(0, "LT", "lt,rt,a,b,x", &why) < 0);
    SAME(why, " range at most 4 inputs per output");
    CHECK(set(0, "LT", "lt,,rt", &why) < 0);
    CHECK(set(0, "LT", "lt,", &why) < 0);
    SAME(line(0, "LT"), "lt");                              /* a failed set changed nothing */

    /* aliases: Pico2Maple's spellings */
    CHECK(set(0, "A1_UP", "DPAD_UP, A2_DOWN, SELECT, L1", &why) == 4);
    SAME(line(0, "A1_UP"), "dpad.up, rstick.down, back, lb");

    /* other entries are never touched by a set */
    {
        size_t keyboard = 0, combos = 0;
        for (size_t i = 0; i < n; i++) {
            if (map[i].dst_btn >= 32 && map[i].dst_btn < 115) { keyboard++; }
            if (map[i].dst_btn >= 118) { combos++; }
        }
        CHECK(keyboard == 83);
        CHECK(combos == 10);
    }

    /* an entry for another port, and a source without a name, are not shown and survive */
    default_map(0);
    map[n++] = (struct iblue_mapent){IBM_LS, IBM_LS, 1, 100, 50, 135, 0, 0};   /* lb -> Z of port B stored under port A */
    SAME(line(0, "Z"), "lb");
    map[n++] = (struct iblue_mapent){IBM_LT, IBM_LS, 0, 100, 50, 135, 0, 0};   /* id 26 has no name */
    SAME(line(0, "Z"), "lb");
    CHECK(set(0, "Z", "rb", &why) == 1);
    CHECK(map[n - 2].dst_id == 1 || map[n - 1].dst_id == 1 || 1);
    {
        int other_port = 0;
        for (size_t i = 0; i < n; i++) { if (map[i].dst_id == 1) { other_port++; } }
        CHECK(other_port == 1);                              /* the cross-port entry survived */
    }

    /* table full: nothing changes */
    default_map(0);
    {
        struct iblue_mapent e[IBLUE_MAP_PER_OUT];
        size_t before = n;
        int c = iblue_map_parse("a,b,x,y", 7, (uint32_t)iblue_out_find("A", 1), 0, e, &why);
        CHECK(c == 4);
        CHECK(iblue_map_replace(map, n, n, 0, (uint32_t)iblue_out_find("A", 1), e, 4) == (size_t)-1);   /* no room for 3 more */
        CHECK(n == before);
        CHECK(iblue_map_replace(map, n, n + 3, 0, (uint32_t)iblue_out_find("A", 1), e, 4) == n + 3);
    }

    /* render: a small buffer is cut at an entry, the NUL is always there */
    default_map(0);
    CHECK(set(0, "A", "a, b, x, y", &why) == 4);
    {
        char small[6];
        size_t l = iblue_map_render(map, n, 0, (uint32_t)iblue_out_find("A", 1), small, sizeof(small));
        CHECK(l == strlen(small));
        CHECK(l <= 5);
        SAME(small, "a, b");
        CHECK(iblue_map_render(map, n, 0, 0, small, 0) == 0);
    }

    /* layouts */
    CHECK(IBLUE_LAYOUT_COUNT == 2);
    SAME(iblue_layout_name(0), "standard");
    SAME(iblue_layout_name(1), "all");
    CHECK(iblue_layout_name(2) == NULL);
    CHECK(iblue_layout_find("ALL", 3) == 1 && iblue_layout_find("standard", 8) == 0 && iblue_layout_find("custom", 6) == -1);

    default_map(0);
    CHECK(iblue_layout_match(map, n, 0) == 1);               /* BlueRetro's factory default IS the "all" layout */
    CHECK(set(0, "A", "b", &why) == 1);
    CHECK(iblue_layout_match(map, n, 0) == -1);              /* custom */

    {
        size_t r = iblue_layout_apply(0, map, n, MAXE, 0);   /* standard */
        CHECK(r != (size_t)-1);
        n = r;
        CHECK(iblue_layout_match(map, n, 0) == 0);
        SAME(line(0, "Z"), "rb");
        SAME(line(0, "C"), "lt:threshold=78");
        SAME(line(0, "RT"), "lb, rt");
        SAME(line(0, "D"), "none");
        SAME(line(0, "DPAD2_UP"), "none");
        SAME(line(0, "A"), "a");
        r = iblue_layout_apply(1, map, n, MAXE, 0);          /* back to all */
        CHECK(r != (size_t)-1);
        n = r;
        CHECK(iblue_layout_match(map, n, 0) == 1);
        SAME(line(0, "D"), "back");
        {
            size_t keyboard = 0, combos = 0;
            for (size_t i = 0; i < n; i++) {
                if (map[i].dst_btn >= 32 && map[i].dst_btn < 115) { keyboard++; }
                if (map[i].dst_btn >= 118) { combos++; }
            }
            CHECK(keyboard == 83 && combos == 10);           /* keyboard, mouse and combo entries are kept */
        }
    }
    CHECK(iblue_layout_apply(2, map, n, MAXE, 0) == (size_t)-1);
    {
        size_t before = n;
        CHECK(iblue_layout_apply(0, map, n, 10, 0) == (size_t)-1);   /* no room: refused, table unchanged */
        CHECK(n == before);
    }
    /* the layout of one port leaves another port alone */
    default_map(0);
    {
        size_t base = n;
        for (uint32_t i = 0; i < 115; i++) {
            map[n++] = (struct iblue_mapent){(uint8_t)i, (uint8_t)i, 1, 100, 50, 135, 0, 0};
        }
        n = iblue_layout_apply(0, map, n, 400, 0);
        CHECK(n != (size_t)-1);
        CHECK(iblue_layout_match(map, n, 1) == 1);
        CHECK(iblue_layout_match(map, n, 0) == 0);
        (void)base;
    }

    /* room: drop the keyboard / mouse identity entries, restore them */
    {
        struct iblue_mapent before[200];
        size_t m;
        default_map(1);
        memcpy(before, map, sizeof(before));
        CHECK(n == 125);
        map[n++] = (struct iblue_mapent){40, 41, 1, 100, 50, 135, 0, 0};          /* a keyboard key on another destination: kept */
        map[n++] = (struct iblue_mapent){40, 40, 2, 100, 50, 135, 0, 0};          /* identity of ANOTHER port: kept */
        m = iblue_map_drop_kb(map, n, 1);
        CHECK(m == 125 - 83 + 2);
        CHECK(iblue_map_size_after(map, m, 1, (uint32_t)iblue_out_find("Z", 1), 4, 1) == m - 1 + 4);
        n = m;
        n = iblue_map_restore_kb(map, n, 128, 1);
        CHECK(n == m + 83);
        n = m;
        CHECK(iblue_map_restore_kb(map, n, m + 10, 1) == (size_t)-1);              /* no room: refused, nothing half done is relied on */
        (void)before;
    }

    printf("mapcfg: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
