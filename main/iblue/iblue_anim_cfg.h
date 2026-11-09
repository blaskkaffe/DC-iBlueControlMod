/*
 * iBlueControlMod: the settings of the boot animation and of the port LEDs, kept in NVS. They are read and
 * written by the hosted web config (BLE commands 0x47 to 0x4A). The maths that uses them is
 * iblue_anim_curve.c; the fields, their ranges and what they do are in iblue_anim_curve.h.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_ANIM_CFG_H_
#define _IBLUE_ANIM_CFG_H_

#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "iblue/iblue_anim_curve.h"

#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED

/* Load the saved settings (NVS must be up) and give them to the animation maths. Defaults if there are none. */
void iblue_anim_cfg_init(void);
/* Apply, clamp and save. */
void iblue_anim_cfg_save(const struct iblue_anim_cfg *c);
/* Back to the defaults (saved). */
void iblue_anim_cfg_reset(void);

#else
#define iblue_anim_cfg_init()
#endif

#endif /* _IBLUE_ANIM_CFG_H_ */
