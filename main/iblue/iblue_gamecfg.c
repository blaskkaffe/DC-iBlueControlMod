/* iBlueControlMod: built-in controller setups for games. See iblue_gamecfg.h. */

#include <string.h>
#include "iblue/iblue_gamecfg.h"

struct game {
    const char *number; /* upper case, no dashes */
    int profile;
};

static const struct game games[] = {
    { "HDR0063", IBLUE_GAME_SAMBA }, /* Samba de Amigo, Japan */
    { "MK51092", IBLUE_GAME_SAMBA }, /* Samba de Amigo, USA and Europe (-50) */
    { "HDR0131", IBLUE_GAME_SAMBA }, /* Samba de Amigo Ver. 2000, Japan */
};

int iblue_game_profile(const char *gid) {
    char norm[16];
    size_t n = 0;

    if (!gid) {
        return IBLUE_GAME_NONE;
    }
    if (gid[0] == 'D' && gid[1] == 'C' && gid[2] == '_') {
        gid += 3;
    }
    for (; *gid && n < sizeof(norm) - 1; gid++) {
        char c = *gid;

        if (c == '-' || c == '_' || c == ' ') {
            continue;
        }
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        }
        norm[n++] = c;
    }
    norm[n] = 0;
    for (size_t i = 0; i < sizeof(games) / sizeof(games[0]); i++) {
        size_t l = strlen(games[i].number);

        if (n >= l && !memcmp(norm, games[i].number, l)) {
            return games[i].profile;
        }
    }
    return IBLUE_GAME_NONE;
}

int iblue_game_setup(int profile, int joycons, int wiimotes, int motion_on) {
    (void)wiimotes; /* no motion from a Wii Remote yet: it plays as an ordinary pad */
    if (profile == IBLUE_GAME_SAMBA && joycons > 0 && motion_on) {
        return IBLUE_SETUP_MOTION;
    }
    return IBLUE_SETUP_NONE;
}

static void put(struct iblue_mapent **o, size_t *n, size_t max, uint8_t src, uint8_t dst) {
    if (*n < max) {
        struct iblue_mapent *e = &(*o)[(*n)++];

        e->src_btn = src;
        e->dst_btn = dst;
        e->dst_id = 0;
        e->perc_max = 100;
        e->perc_threshold = 50;
        e->perc_deadzone = 135;
        e->turbo = 0;
        e->algo = 0;
    }
}

/* The maracas report 255 at the top, a pad reports 0 for up: the Y axes swap places. Shake left = B, shake right = A, the left
 * maraca's button on top is C, the right one's is Start (docs/samba-de-amigo.md). */
size_t iblue_game_map(int profile, int motion, uint32_t input, struct iblue_mapent *out, size_t max) {
    size_t n = 0;

    if (profile != IBLUE_GAME_SAMBA || input >= IBLUE_GAMECFG_INPUTS) {
        return 0;
    }
    if (motion) {
        /* the Joy-Con's motion is its right stick, its shake is the left trigger button */
        if (input == 0) { /* left Joy-Con: left maraca (the sticks of the pad) */
            put(&out, &n, max, IBM_RX_LEFT, IBM_LX_LEFT);
            put(&out, &n, max, IBM_RX_RIGHT, IBM_LX_RIGHT);
            put(&out, &n, max, IBM_RY_UP, IBM_LY_DOWN);
            put(&out, &n, max, IBM_RY_DOWN, IBM_LY_UP);
            put(&out, &n, max, IBM_LT, IBM_RB_RIGHT);    /* shake = B */
            put(&out, &n, max, IBM_LM, IBM_RS);          /* ZL = C */
        }
        else { /* right Joy-Con: right maraca (the second stick of the pad) */
            put(&out, &n, max, IBM_RX_LEFT, IBM_RX_LEFT);
            put(&out, &n, max, IBM_RX_RIGHT, IBM_RX_RIGHT);
            put(&out, &n, max, IBM_RY_UP, IBM_RY_DOWN);
            put(&out, &n, max, IBM_RY_DOWN, IBM_RY_UP);
            put(&out, &n, max, IBM_LT, IBM_RB_DOWN);     /* shake = A */
            put(&out, &n, max, IBM_MM, IBM_MM);          /* Plus = Start */
            put(&out, &n, max, IBM_RM, IBM_MM);          /* ZR = Start */
        }
        return n;
    }
    if (input == 0) {
        put(&out, &n, max, IBM_LX_LEFT, IBM_LX_LEFT);
        put(&out, &n, max, IBM_LX_RIGHT, IBM_LX_RIGHT);
        put(&out, &n, max, IBM_RX_LEFT, IBM_RX_LEFT);
        put(&out, &n, max, IBM_RX_RIGHT, IBM_RX_RIGHT);
        put(&out, &n, max, IBM_LY_DOWN, IBM_LY_UP);
        put(&out, &n, max, IBM_LY_UP, IBM_LY_DOWN);
        put(&out, &n, max, IBM_RY_DOWN, IBM_RY_UP);
        put(&out, &n, max, IBM_RY_UP, IBM_RY_DOWN);
        put(&out, &n, max, IBM_LS, IBM_RB_RIGHT);        /* L1 = shake left (B) */
        put(&out, &n, max, IBM_RS, IBM_RB_DOWN);         /* R1 = shake right (A) */
        put(&out, &n, max, IBM_LM, IBM_RS);              /* L2 = C */
        put(&out, &n, max, IBM_RM, IBM_MM);              /* R2 = Start */
        put(&out, &n, max, IBM_MM, IBM_MM);              /* Start = Start */
    }
    return n;
}
