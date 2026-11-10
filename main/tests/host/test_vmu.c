/* Formatting a blank Dreamcast memory card (iblue_vmu.c). The card is checked the way a Dreamcast program reads it: in the
 * normal byte order (every group of 4 bytes of the stored form turned around), following the FAT from the root block. */
#include <stdio.h>
#include <string.h>
#include "iblue/iblue_vmu.h"

static int fails, checks;
#define CHECK(c, msg) do { checks++; if (!(c)) { fails++; printf("FAIL: %s\n", msg); } } while (0)

static uint8_t tail[IBLUE_VMU_TAIL], img[IBLUE_VMU_TAIL];

static uint16_t rd16(uint32_t off) { return (uint16_t)(img[off] | (img[off + 1] << 8)); }

int main(void) {
    const uint32_t fat = IBLUE_VMU_TAIL - 1024, root = IBLUE_VMU_TAIL - 512;
    uint32_t n, b, free_blocks = 0;

    memset(tail, 0xA5, sizeof(tail)); /* what the RAM held */
    CHECK(!iblue_vmu_is_formatted(tail), "rubbish is not a formatted card");
    memset(tail, 0, sizeof(tail));
    CHECK(!iblue_vmu_is_formatted(tail), "an empty card is not formatted");
    memset(tail, 0xFF, sizeof(tail));
    CHECK(!iblue_vmu_is_formatted(tail), "an erased flash is not formatted");

    iblue_vmu_format_tail(tail);
    CHECK(iblue_vmu_is_formatted(tail), "formatted after formatting");
    for (uint32_t i = 0; i < IBLUE_VMU_TAIL; i++) { img[i] = tail[(i & ~3u) | (3u - (i & 3u))]; }

    /* root block */
    for (uint32_t i = 0; i < 16; i++) { CHECK(img[root + i] == 0x55, "root: the mark"); }
    CHECK(img[root + 0x10] == 0, "root: no custom colour");
    CHECK(img[root + 0x30] == 0x20 && img[root + 0x32] >= 0x01 && img[root + 0x32] <= 0x12, "root: a BCD date");
    CHECK(rd16(root + 0x40) == 255 && rd16(root + 0x44) == 255, "root: size and its own place");
    CHECK(rd16(root + 0x46) == 254 && rd16(root + 0x48) == 1, "root: the FAT is block 254, one block");
    CHECK(rd16(root + 0x4A) == 253 && rd16(root + 0x4C) == 13, "root: the directory starts at 253, 13 blocks");
    CHECK(rd16(root + 0x50) == 200, "root: 200 blocks for the user");
    CHECK(rd16(root + 0x52) == 31 && rd16(root + 0x56) == 128, "root: the two constants a Dreamcast writes");

    /* FAT: the directory chain, as a program follows it */
    n = 0;
    for (b = rd16(root + 0x4A); b != 0xFFFA && n < 300; b = rd16(fat + b * 2)) {
        CHECK(b >= 241 && b <= 253, "directory block in its area");
        n++;
    }
    CHECK(n == 13 && b == 0xFFFA, "the directory chain is 13 blocks long and ends");
    CHECK(rd16(fat + 254 * 2) == 0xFFFA && rd16(fat + 255 * 2) == 0xFFFA, "FAT and root are marked as used");
    for (uint32_t i = 0; i < 200; i++) { if (rd16(fat + i * 2) == 0xFFFC) { free_blocks++; } }
    CHECK(free_blocks == 200, "all 200 user blocks are free");
    for (uint32_t i = 200; i < 241; i++) { CHECK(rd16(fat + i * 2) == 0xFFFC, "the unused area is free"); }

    /* everything else is empty: the directory has no files */
    n = 0;
    for (uint32_t i = 0; i < fat; i++) { n += img[i] != 0; }
    CHECK(n == 0, "the directory blocks in the tail are empty");

    /* a formatted card with files is left alone by the check (only the 16 bytes count) */
    tail[100] = 0x12;
    CHECK(iblue_vmu_is_formatted(tail), "a card in use stays formatted");
    tail[root + 5] = 0x54;
    CHECK(!iblue_vmu_is_formatted(tail), "one wrong byte in the mark");

    printf("vmu: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
