/*
 * iBlueControlMod: configuration profiles. A profile is a named copy of the whole BlueRetro configuration
 * (what blueretro.io edits: every port's mapping, output mode, accessories, global settings) kept in flash
 * as /fs/prof_<slot>. The hosted web config saves, loads and deletes them (BLE commands 0x43 to 0x46); a tap
 * gesture can load one. They survive a factory reset.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_PROFILE_H_
#define _IBLUE_PROFILE_H_

#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"

#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED

#define IBLUE_PROFILES 4
#define IBLUE_PROFILE_NAME_MAX 15

enum {
    IBLUE_PROF_OK = 0,
    IBLUE_PROF_EMPTY = 1,   /* nothing in that slot */
    IBLUE_PROF_BAD_ARG = 2,
    IBLUE_PROF_FAILED = 3,  /* flash or memory */
};

/* Is the slot used, and its name (NUL terminated, at least IBLUE_PROFILE_NAME_MAX + 1 bytes) */
bool iblue_profile_info(uint32_t slot, char *name);
/* The current configuration into the slot, with this name ("" gives "Profile <n>") */
uint32_t iblue_profile_save(uint32_t slot, const char *name);
/* Make the profile the configuration (and keep it, as a web config save does) */
uint32_t iblue_profile_load(uint32_t slot);
uint32_t iblue_profile_delete(uint32_t slot);
/* The next used slot after the one loaded last, wrapping round */
uint32_t iblue_profile_next(void);

#endif

#endif /* _IBLUE_PROFILE_H_ */
