/* iBlueControlMod: game id on/off from the button (see iblue_gameid.h). */

#ifndef _IBLUE_GAMEID_CTL_H_
#define _IBLUE_GAMEID_CTL_H_

#include "sdkconfig.h"

#ifdef CONFIG_BLUERETRO_IBLUE_GAMEID
void iblue_gameid_ctl_init(void);
int iblue_gameid_ctl_toggle(void); /* returns the new state: 1 on */
#else
#define iblue_gameid_ctl_init()
#define iblue_gameid_ctl_toggle() 0
#endif

#endif
