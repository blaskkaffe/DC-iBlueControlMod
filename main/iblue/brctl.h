/*
 * BlueRetro control channel over the Dreamcast Maple bus
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BRCTL_H_
#define _BRCTL_H_

#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"
#include "iblue/brctl_proto.h"

#ifdef CONFIG_BLUERETRO_BRCTL
struct raw_fb;

/* Main core */
void brctl_init(void);
void brctl_status_update(void);
void brctl_exec(struct raw_fb *fb_data);

/* Wired core (Maple ISR): IRAM, no flash access */
uint32_t brctl_read(uint32_t param, uint32_t len, void *dst);
bool brctl_write(uint32_t param, const void *src, uint32_t len);
void brctl_queue_op(const uint32_t *op_and_args, uint32_t words);
#endif

#endif /* _BRCTL_H_ */
