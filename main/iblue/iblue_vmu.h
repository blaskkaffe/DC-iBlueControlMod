/*
 * iBlueControlMod: the Dreamcast memory card (VMU) image: formatting a blank card. Portable C, host-tested
 * (main/tests/host/test_vmu.c).
 *
 * BlueRetro keeps the card as 128 KB in RAM (32 buffers of 4 KB, memory_card.c) and in the file mc.bin, in the byte order of the
 * Maple bus: every group of 4 bytes is reversed compared to a normal VMU dump (.bin of an emulator, Pico2Maple's files). The
 * functions here work on that bus order.
 *
 * A new card used to be whatever was in the RAM at the first start, and nothing formatted it: the Dreamcast's own file manager can
 * format a card, but programs made with KallistiOS (openMenu) cannot, they just find no file system and do not save. Now a card
 * whose root block does not start with the 16 bytes 0x55 of a formatted card is formatted at start-up (as Pico2Maple does).
 *
 * The layout written (the same a Dreamcast writes, facts of the format, 512 byte blocks):
 *   block 255 root: 16 x 0x55, no custom colour, a date, then 16 bit little endian: 255, 0, 255 (root), 254 (FAT), 1 (FAT size),
 *                   253 (directory, growing down), 13 (directory size), 0 (icon), 200 (user blocks), 31, 0, 128
 *   block 254 FAT:  256 entries of 16 bit: 0xFFFC free (0..240), the directory chain 253 -> 252 ... -> 241 -> 0xFFFA (end),
 *                   0xFFFA for the FAT and the root block
 *   everything else zero
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_VMU_H_
#define _IBLUE_VMU_H_

#include <stdint.h>

#define IBLUE_VMU_SIZE (128 * 1024)
#define IBLUE_VMU_TAIL 4096 /* the last 4 KB of the card hold blocks 248..255: the top of the directory, the FAT and the root */

/* tail: the last IBLUE_VMU_TAIL bytes of the card, bus byte order. 1: the root block carries the mark of a formatted card. */
int iblue_vmu_is_formatted(const uint8_t *tail);
/* Write an empty file system into the tail (the caller zeroes the rest of the card). */
void iblue_vmu_format_tail(uint8_t *tail);

#endif /* _IBLUE_VMU_H_ */
