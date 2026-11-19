/*
 * iBlueControlMod: built-in controller setups for games that need one (today: Samba de Amigo and Samba de Amigo Ver. 2000, the
 * only games that use the maracas). Portable C, host-tested (main/tests/host/test_gamecfg.c).
 *
 * When openMenu announces a game (Maple command 0x21, see iblue_gameid.h), there is no configuration saved for that game, and a
 * controller that can be a maraca is connected, the adapter uses the built-in setup of the game: port A becomes the maracas
 * controller and the inputs are mapped for it. Without such a controller nothing changes (an ordinary pad plays the game as an
 * ordinary pad). A configuration saved for the game (blueretro.io, the web page, the DC API commit) always wins.
 *
 * A controller that can be a maraca (iblue_game_setup()): a Joy-Con while "Joy-Con motion" is on. A Wii Remote is known to the
 * rule but gives no setup yet: the firmware does not read its motion (docs/samba-de-amigo.md, Wii Remote).
 *
 * The game is recognised by its product number as it stands in the disc's header. The numbers come from the retail catalogues
 * (segaretro.org, retailer listings), not from reading the discs, and the way a PAL number is padded in the header is not known:
 * the match therefore ignores case, dashes and anything after the number (the PAL "-50" may be glued on).
 *    Samba de Amigo           Japan HDR-0063    USA MK-51092    Europe MK-51092-50
 *    Samba de Amigo Ver. 2000 Japan HDR-0131    (no release outside Japan; the English fan patch keeps the number)
 * Extra numbers are added to the table in iblue_gamecfg.c.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_GAMECFG_H_
#define _IBLUE_GAMECFG_H_

#include <stddef.h>
#include <stdint.h>
#include "iblue/iblue_mapcfg.h"

enum {
    IBLUE_GAME_NONE = 0,
    IBLUE_GAME_SAMBA,
};

/* What a built-in setup gives the first input (the one that drives port A) and the second one (it drives port A too) */
#define IBLUE_GAMECFG_INPUTS 2

/* gid: the game id as BlueRetro holds it, "DC_" and the product number, or just the number. Returns IBLUE_GAME_*. */
int iblue_game_profile(const char *gid);

/* Which setup the profile gets with the controllers that are connected now */
enum {
    IBLUE_SETUP_NONE = 0,   /* leave the configuration alone */
    IBLUE_SETUP_MOTION,     /* Joy-Cons as maracas: the left one is the first input, the right one the second */
};
int iblue_game_setup(int profile, int joycons, int wiimotes, int motion_on);

/* The mapping entries of input 0 or 1 for the profile, written to out (at most max). motion: 1 = Joy-Cons with motion mode on
 * (the right Joy-Con is the second input), 0 = one pad whose two sticks are the two maracas (the web page's preset, never set
 * automatically). Entries have the default scale, threshold and deadzone of BlueRetro and drive port 0. Returns the number of
 * entries; 0 for an input that has none. */
size_t iblue_game_map(int profile, int motion, uint32_t input, struct iblue_mapent *out, size_t max);

#endif /* _IBLUE_GAMECFG_H_ */
