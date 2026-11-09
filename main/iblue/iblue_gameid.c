/* iBlueControlMod: Dreamcast game id (Maple command 0x21). See iblue_gameid.h. */

#include <string.h>
#include <esp_attr.h>
#include "adapter/adapter.h"
#include "iblue/iblue_gameid.h"

#ifdef CONFIG_BLUERETRO_IBLUE_GAMEID

#define ID_USED 10 /* "DC_" + 10 characters fit in the 13 bytes of a game id feedback */

/* The id applied last, NUL terminated; "" = no game. */
static DRAM_ATTR char applied[IBLUE_GAMEID_LEN + 1];
static DRAM_ATTR volatile uint8_t enabled = 1; /* off: the id is remembered but no game configuration is loaded */

/* Hand the game id (or no game, n = 0) to BlueRetro */
static void IRAM_ATTR queue_id(const char *id, uint32_t n) {
    struct raw_fb fb = {0};

    if (n > ID_USED) {
        n = ID_USED;
    }
    fb.header.type = FB_TYPE_SYS_ID; /* the game id path of BlueRetro: gid_update_sys() and config_init(GAMEID_CFG) */
    if (n) {
        fb.data[0] = 'D';
        fb.data[1] = 'C';
        fb.data[2] = '_';
        memcpy(&fb.data[3], id, n);
        fb.header.data_len = (uint8_t)(3 + n);
    }
    else {
        fb.header.data_len = 0; /* no game: the normal configuration */
    }
    adapter_q_fb(&fb);
}

int iblue_gameid_enabled(void) {
    return enabled;
}

/* Off: the normal configuration is back at once; on: the game that runs (if any) gets its configuration again */
void iblue_gameid_set_enabled(int on) {
    enabled = on ? 1 : 0;
    queue_id(on ? applied : "", on ? strlen(applied) : 0);
}

void IRAM_ATTR iblue_gameid_set(const uint8_t *raw) {
    char id[IBLUE_GAMEID_LEN + 1];
    uint32_t n = 0;
    uint32_t i;

    while (n < IBLUE_GAMEID_LEN && raw[n]) {
        id[n] = (char)raw[n];
        n++;
    }
    while (n && id[n - 1] == ' ') {
        n--; /* some senders pad with spaces */
    }
    id[n] = 0;

    if (n == 6 && memcmp(id, "DCBIOS", 6) == 0) {
        return; /* not a game: the BIOS menu */
    }
    for (i = 0; i < n; i++) {
        char c = id[i];

        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '_')) {
            return; /* the id becomes part of a file name; a corrupt frame ends up here too */
        }
    }
    for (i = 0; i <= n; i++) {
        if (applied[i] != id[i]) {
            break;
        }
    }
    if (i > n) {
        return; /* the same id again */
    }
    for (i = 0; i <= n; i++) {
        applied[i] = id[i];
    }

    if (enabled) {
        queue_id(id, n);
    }
}

const char *IRAM_ATTR iblue_gameid_current(void) {
    return applied;
}

void IRAM_ATTR iblue_gameid_set_wire(const uint8_t *wire) {
    uint8_t raw[IBLUE_GAMEID_LEN];

    for (uint32_t w = 0; w < IBLUE_GAMEID_LEN; w += 4) {
        for (uint32_t j = 0; j < 4; j++) {
            raw[w + j] = wire[w + 3 - j];
        }
    }
    iblue_gameid_set(raw);
}

void IRAM_ATTR iblue_gameid_ext_name(uint8_t *wire16) {
    static DRAM_ATTR const char name[17] = IBLUE_EXT_NAME; /* in RAM: this runs in the Maple interrupt */

    for (uint32_t w = 0; w < 16; w += 4) {
        for (uint32_t j = 0; j < 4; j++) {
            wire16[w + j] = (uint8_t)name[w + 3 - j];
        }
    }
}

#endif /* CONFIG_BLUERETRO_IBLUE_GAMEID */
