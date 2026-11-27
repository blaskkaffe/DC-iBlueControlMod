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

/* Sniffer: listen on ports the adapter does not answer on, never send. Each frame seen there (from the console or from
 * a controller) goes into a ring of MAPLE_SNIFF_MAX entries. maple_sniff(mask) starts it on those ports (0 stops it). */
#define MAPLE_SNIFF_MAX 128
struct maple_sniff_frame {
    uint32_t ccount;    /* core 1 cycle count when it ended (240 per us) */
    uint8_t port;       /* bit 0-1 port, bit 4-7 bits lost at the start (0-8; 8 = a whole byte) */
    uint8_t sender;     /* address of the sender: low 6 bits 0 = the console, 0x20 a controller, 0x01-0x10 its sub-devices */
    uint8_t recipient;
    uint8_t cmd;
    uint8_t len;        /* words after the header, from the bit count */
    uint8_t hdr_len;    /* the length byte of the header */
    uint16_t bits;
};
extern struct maple_sniff_frame maple_sniff_ring[MAPLE_SNIFF_MAX];
extern volatile uint32_t maple_sniff_cnt;   /* frames logged since the start (the ring keeps the last MAPLE_SNIFF_MAX) */
extern volatile uint32_t maple_sniff_host[4];  /* frames from the console, per port */
extern volatile uint32_t maple_sniff_dev[4];   /* frames from a controller or its sub-devices, per port */
void maple_sniff(uint16_t mask);
#endif

#endif /* _MAPLE_H_ */
