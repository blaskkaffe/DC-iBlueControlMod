/*
 * iBlueControlMod: Dreamcast game id. openMenu knows which game it starts; the Dreamcast itself tells
 * the adapter nothing. openMenu writes the product number into one reserved block of the adapter's
 * emulated VMU before it starts the game, and the adapter makes it BlueRetro's game id: the same
 * mechanism N64, GameCube and PlayStation use. Then blueretro.io shows the game id and can save a
 * configuration for that game ("DC_<product>"); when the same game starts again, that configuration
 * is loaded; games without one use the normal configuration. The block is never stored as VMU data.
 *
 * Block layout (512 bytes, little endian):
 *    0  "iBGI"
 *    4  u16 version (1)
 *    6  u16 reserved
 *    8  u32 sequence (openMenu counts up; the same id again is applied again)
 *   12  char id[16]  letters, digits, - and _, NUL padded; "" = no game (back to the normal config)
 *   28  u32 CRC32 of bytes 0 to 27
 * Reading the block returns the same layout with the id the adapter applied last, so openMenu can
 * tell that the adapter has the feature and read back what it has. Only the first 10 characters of
 * the id are used (the game id feedback of BlueRetro holds 13 bytes: "DC_" and 10).
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_GAMEID_H_
#define _IBLUE_GAMEID_H_

#include <stdint.h>
#include "sdkconfig.h"

#define IBLUE_GAMEID_BLOCK 200
#define IBLUE_GAMEID_BLOCK_SIZE 512

#ifdef CONFIG_BLUERETRO_IBLUE_GAMEID

/* Called from the Maple interrupt (IRAM, no flash access) */
int iblue_gameid_owns(uint32_t block);
void iblue_gameid_read(uint8_t *dst);                                   /* 512 bytes */
void iblue_gameid_write(uint32_t phase, const uint8_t *src128);        /* phase 0..3, 128 bytes each */
/* On by default. Off: games announce themselves but no game configuration is loaded (and the normal one is
 * back at once); on again: the game that runs gets its configuration. Kept over restarts by
 * iblue_gameid_ctl.c. */
int iblue_gameid_enabled(void);
void iblue_gameid_set_enabled(int on);

#else
#define iblue_gameid_owns(block) 0
#define iblue_gameid_read(dst)
#define iblue_gameid_write(phase, src)
#define iblue_gameid_enabled() 0
#define iblue_gameid_set_enabled(on)
#endif

#endif /* _IBLUE_GAMEID_H_ */
