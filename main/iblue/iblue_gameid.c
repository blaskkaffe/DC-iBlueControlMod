/* iBlueControlMod: Dreamcast game id mailbox. See iblue_gameid.h. */

#include <string.h>
#include <esp_attr.h>
#include "adapter/adapter.h"
#include "iblue/iblue_gameid.h"

#ifdef CONFIG_BLUERETRO_IBLUE_GAMEID

#define ID_MAX 16
#define ID_USED 10 /* "DC_" + 10 characters fit in the 13 bytes of a game id feedback */

static DRAM_ATTR uint8_t box[IBLUE_GAMEID_BLOCK_SIZE];
static DRAM_ATTR uint32_t last_seq;
static DRAM_ATTR uint8_t inited;
static DRAM_ATTR volatile uint8_t enabled = 1;  /* off: the id is remembered but no game configuration is loaded */
static DRAM_ATTR uint8_t cur_id[ID_USED + 1];   /* the id the console last announced, NUL ended */
static DRAM_ATTR uint8_t cur_n;

static uint32_t IRAM_ATTR rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void IRAM_ATTR wr32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static uint32_t IRAM_ATTR crc32(const uint8_t *data, uint32_t len) {
    uint32_t crc = 0xFFFFFFFF;

    while (len--) {
        crc ^= *data++;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (0 - (crc & 1)));
        }
    }
    return ~crc;
}

static void IRAM_ATTR init_box(void) {
    memset(box, 0, sizeof(box));
    memcpy(box, "iBGI", 4);
    box[4] = 1;
    wr32(box + 28, crc32(box, 28));
    inited = 1;
}

int IRAM_ATTR iblue_gameid_owns(uint32_t block) {
    return block == IBLUE_GAMEID_BLOCK;
}

void IRAM_ATTR iblue_gameid_read(uint8_t *dst) {
    if (!inited) {
        init_box();
    }
    memcpy(dst, box, sizeof(box));
}

/* Hand the game id (or no game, n = 0) to BlueRetro: gid_update_sys() and config_init(GAMEID_CFG) */
static void IRAM_ATTR queue_id(const uint8_t *id, uint32_t n) {
    struct raw_fb fb = {0};

    fb.header.type = FB_TYPE_SYS_ID;
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

int IRAM_ATTR iblue_gameid_enabled(void) {
    return enabled;
}

/* Off: back to the normal configuration at once. On: the game that is running gets its configuration. */
void IRAM_ATTR iblue_gameid_set_enabled(int on) {
    on = on ? 1 : 0;
    if (on == enabled) {
        return;
    }
    enabled = (uint8_t)on;
    queue_id(cur_id, on ? cur_n : 0);
}

void IRAM_ATTR iblue_gameid_write(uint32_t phase, const uint8_t *src128) {
    uint32_t n = 0;

    if (!inited) {
        init_box();
    }
    if (phase > 3) {
        return;
    }
    memcpy(&box[phase * 128], src128, 128);
    if (phase != 3) {
        return; /* the block is complete after its fourth part */
    }
    /* Check the whole block before anything is done with it */
    if (memcmp(box, "iBGI", 4) != 0 || box[4] != 1 || crc32(box, 28) != rd32(box + 28)) {
        init_box(); /* a game or the BIOS formatting the card, or half a write: not for us */
        return;
    }
    if (rd32(box + 8) == last_seq && last_seq != 0) {
        return; /* same request again */
    }
    for (uint32_t i = 0; i < ID_MAX; i++) {
        uint8_t c = box[12 + i];

        if (c == 0) {
            break;
        }
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '_')) {
            init_box();
            return; /* the id becomes part of a file name */
        }
        n++;
    }
    last_seq = rd32(box + 8);
    if (n > ID_USED) {
        n = ID_USED;
    }
    memcpy(cur_id, &box[12], n);
    cur_id[n] = 0;
    cur_n = (uint8_t)n;
    if (enabled) {
        queue_id(cur_id, n);
    }
    /* Keep the block as it is: it is what a read gives back */
    wr32(box + 28, crc32(box, 28));
}

#endif /* CONFIG_BLUERETRO_IBLUE_GAMEID */
