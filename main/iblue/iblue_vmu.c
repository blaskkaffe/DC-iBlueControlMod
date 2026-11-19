/* iBlueControlMod: formatting a blank Dreamcast memory card. See iblue_vmu.h. */

#include <string.h>
#include "iblue/iblue_vmu.h"

#define BLOCK 512
#define FAT_OFF (IBLUE_VMU_TAIL - 2 * BLOCK)  /* block 254 inside the tail */
#define ROOT_OFF (IBLUE_VMU_TAIL - BLOCK)     /* block 255 inside the tail */

#define FAT_FREE 0xFFFC
#define FAT_END 0xFFFA
#define DIR_FIRST 253
#define DIR_BLOCKS 13

/* A byte of the normal (dump) order lives at the mirrored place inside its group of 4 */
static void put8(uint8_t *base, uint32_t off, uint8_t v) {
    base[(off & ~3u) | (3u - (off & 3u))] = v;
}

static void put16(uint8_t *base, uint32_t off, uint16_t v) {
    put8(base, off, (uint8_t)(v & 0xFF));
    put8(base, off + 1, (uint8_t)(v >> 8));
}

int iblue_vmu_is_formatted(const uint8_t *tail) {
    for (uint32_t i = 0; i < 16; i++) {
        if (tail[ROOT_OFF + i] != 0x55) { /* 16 equal bytes: the same in both byte orders */
            return 0;
        }
    }
    return 1;
}

void iblue_vmu_format_tail(uint8_t *tail) {
    uint8_t *fat = tail + FAT_OFF;
    uint8_t *root = tail + ROOT_OFF;
    /* the date a card was formatted, BCD: century, year, month, day, hour, minute, second, day of the week (0 = Monday) */
    static const uint8_t date[8] = { 0x20, 0x26, 0x01, 0x01, 0x00, 0x00, 0x00, 0x03 };
    static const uint16_t info[12] = { 255, 0, 255, 254, 1, DIR_FIRST, DIR_BLOCKS, 0, 200, 31, 0, 128 };

    memset(tail, 0, IBLUE_VMU_TAIL);

    for (uint32_t i = 0; i < 256; i++) {
        uint16_t v = FAT_FREE;

        if (i >= 254 || i == DIR_FIRST - DIR_BLOCKS + 1) {
            v = FAT_END;                /* the FAT, the root, and the last block of the directory */
        }
        else if (i > DIR_FIRST - DIR_BLOCKS + 1 && i <= DIR_FIRST) {
            v = (uint16_t)(i - 1);      /* the directory runs downwards */
        }
        put16(fat, i * 2, v);
    }

    for (uint32_t i = 0; i < 16; i++) {
        put8(root, i, 0x55);
    }
    for (uint32_t i = 0; i < 8; i++) {
        put8(root, 0x30 + i, date[i]);
    }
    for (uint32_t i = 0; i < 12; i++) {
        put16(root, 0x40 + i * 2, info[i]);
    }
}
