/*
 * iBlueControlMod: Dreamcast game id. openMenu knows which game it starts; the Dreamcast itself tells
 * the adapter nothing. Before it starts a game, openMenu sends the game id to every VM2-class device on
 * the controller ports (VM2, VMU Pro, USB4Maple, Pico2Maple) with Maple command 0x21 ("33") addressed to
 * the memory card unit. The adapter answers it the same way, and makes the id BlueRetro's game id: the
 * same mechanism N64, GameCube and PlayStation use. Then blueretro.io shows the game id and can save a
 * configuration for that game ("DC_<product>"); when the same game starts again, that configuration is
 * loaded; games without one use the normal configuration.
 *
 * The frame (payload). On the wire the four bytes of every 32-bit word come in reverse order compared to
 * the Dreamcast's memory, so text is word-reversed ("Prod" travels as "dorP"; maple.c stores its own
 * strings that way). In the sender's memory the payload is:
 *    0  u32 function code of the memory card (0x00000002 in this file's ID_VMU_MEM)
 *    4  char id[12]    the disc's product number, NUL padded (openMenu: strncpy of 12 bytes)
 *   16  char name[128] the game's name (optional; the frame is then 36 words instead of 4); not used here
 * The answer is an acknowledge.
 *
 * To be sent the id at all, openMenu has to recognise the device: it asks for the extended device info
 * (Maple command 0x02) and looks at the first 16 bytes of the "extended" field. The adapter's memory card
 * unit answers with IBLUE_EXT_NAME there (see maple.c).
 *
 * Rules (same as the device that taught the format, Pico2Maple): the id "DCBIOS" is ignored, an empty id
 * means "no game" (back to the normal configuration), and the same id again is not applied again. The id
 * becomes part of a file name, so only letters, digits, - and _ are accepted; anything else is ignored.
 * Only the first 10 characters are used (the game id feedback of BlueRetro holds 13 bytes: "DC_" and 10).
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_GAMEID_H_
#define _IBLUE_GAMEID_H_

#include <stdint.h>
#include "sdkconfig.h"

#define IBLUE_GAMEID_CMD 0x21
#define IBLUE_GAMEID_LEN 12
/* what openMenu looks for in the first 16 bytes of the extended device info */
#define IBLUE_EXT_NAME "iBlueControlMod "

#ifdef CONFIG_BLUERETRO_IBLUE_GAMEID

/* Called from the Maple interrupt (IRAM, no flash access).
 * set: raw = the 12 id characters in reading order.
 * set_wire: the 12 bytes as they arrive on the Maple bus (every 4 bytes reversed). */
void iblue_gameid_set(const uint8_t *raw);
void iblue_gameid_set_wire(const uint8_t *wire);
/* The 16 bytes of IBLUE_EXT_NAME as they have to be sent on the Maple bus (every 4 bytes reversed) */
void iblue_gameid_ext_name(uint8_t *wire16);
/* The id applied last, NUL terminated; "" = no game (read by the DC API; runs in the Maple interrupt too) */
const char *iblue_gameid_current(void);
/* On by default. Off: games announce themselves but no game configuration is loaded (and the normal one is back at once); on again:
 * the game that runs gets its configuration. Kept over restarts by iblue_gameid_ctl.c. */
int iblue_gameid_enabled(void);
void iblue_gameid_set_enabled(int on);

#else
#define iblue_gameid_set(raw)
#define iblue_gameid_set_wire(wire)
#define iblue_gameid_ext_name(wire16)
#define iblue_gameid_current() ""
#define iblue_gameid_enabled() 0
#define iblue_gameid_set_enabled(on)
#endif

#endif /* _IBLUE_GAMEID_H_ */
