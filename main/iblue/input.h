/*
 * iBlueControlMod: look at every converted Bluetooth report (live buttons for openMenu's
 * press-to-learn). Called from adapter_bridge().
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_INPUT_H_
#define _IBLUE_INPUT_H_

#include <stdint.h>
#include "sdkconfig.h"

#ifdef CONFIG_BLUERETRO_BRCTL
#include "adapter/adapter.h"

/* out_idx: the Dreamcast port the controller is assigned to. */
void iblue_input(uint8_t out_idx, struct wireless_ctrl *ctrl);
/* Buttons held on the controller of that port (generic ids 0..31), 0 if idle or gone */
uint32_t iblue_input_live(uint8_t port);
#else
#define iblue_input(out_idx, ctrl)
#endif

#endif /* _IBLUE_INPUT_H_ */
