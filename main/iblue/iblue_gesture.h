/*
 * iBlueControlMod: what the single, double and triple tap of the button do. The actions can be chosen on the
 * hosted web config (BLE command 0x42) and are kept in NVS. A tap followed by a second press that is held
 * (tap + hold) is not mappable: it sets the custom LED colour.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_GESTURE_H_
#define _IBLUE_GESTURE_H_

#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"

#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED

/* Gestures, one more tap each */
enum {
    IBLUE_GEST_TAP = 0,
    IBLUE_GEST_DOUBLE,
    IBLUE_GEST_TRIPLE,
    IBLUE_GEST_MAX,
};

/* Actions (the numbers are what the web config sends, do not renumber) */
enum {
    IBLUE_ACT_NONE = 0,
    IBLUE_ACT_PAIR,          /* open the pairing window and search for a controller */
    IBLUE_ACT_STOP_PAIRING,  /* stop searching */
    IBLUE_ACT_NEXT_COLOUR,   /* orange, blue, custom */
    IBLUE_ACT_DISCONNECT,    /* disconnect all controllers */
    IBLUE_ACT_PROFILE_1,     /* load profile 1 ... 4 */
    IBLUE_ACT_PROFILE_2,
    IBLUE_ACT_PROFILE_3,
    IBLUE_ACT_PROFILE_4,
    IBLUE_ACT_NEXT_PROFILE,  /* the next saved profile, round and round */
    IBLUE_ACT_TOGGLE_GAMEID, /* game id on / off (the LED dips once for on, twice for off) */
    IBLUE_ACT_MAX,
};

void iblue_gesture_init(void);
uint8_t iblue_gesture_get(uint32_t gesture);
/* Returns false (nothing changed) if an action does not exist */
bool iblue_gesture_set(const uint8_t map[IBLUE_GEST_MAX]);
void iblue_gesture_run(uint32_t gesture);

#endif

#endif /* _IBLUE_GESTURE_H_ */
