/*
 * Copyright (c) 2019-2022, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _MAPLE_H_
#define _MAPLE_H_

void maple_init(uint32_t package);
void maple_port_cfg(uint16_t mask);
#ifdef CONFIG_BLUERETRO_HW_IBLUECONTROLMOD
/* the two Maple pins of a port; the port check's counters (see maple.c) */
void maple_port_pins(uint32_t port, uint8_t *pin0, uint8_t *pin1);
extern volatile uint32_t maple_diag_rx[4];
extern volatile uint32_t maple_diag_tx[4];
extern volatile uint32_t maple_diag_abort[4];
#endif

#endif /* _MAPLE_H_ */
