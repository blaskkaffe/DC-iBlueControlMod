/*
 * iBlueControlMod: count the taps of the button. Portable C (no ESP-IDF), tested on a PC.
 * Start it right after a first tap has ended and feed it the button state every few ms:
 *  - another press within TAP_GAP_MS is the next tap (up to three),
 *  - a second press kept down for TAP_HOLD_MS is tap + hold,
 *  - when no further tap comes, the count so far is final.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_TAP_H_
#define _IBLUE_TAP_H_

#include <stdint.h>

#define IBLUE_TAP_GAP_MS 400   /* how long to wait for the next tap */
#define IBLUE_TAP_HOLD_MS 600  /* a second press kept this long is tap + hold */
#define IBLUE_TAP_MAX 3

enum {
    IBLUE_TAP_EV_NONE = 0,
    IBLUE_TAP_EV_DONE,       /* final: `taps` taps */
    IBLUE_TAP_EV_HOLD_START, /* the second press is kept down: tap + hold begins */
    IBLUE_TAP_EV_HOLD_END,   /* ... and the button was released */
};

struct iblue_tap {
    uint8_t taps;   /* taps counted so far */
    uint8_t phase;
    uint32_t ms;
};

/* After the first tap has ended */
void iblue_tap_start(struct iblue_tap *t);
/* One step of dt_ms with the button pressed or not. Returns an IBLUE_TAP_EV_*. */
int iblue_tap_step(struct iblue_tap *t, int pressed, uint32_t dt_ms);

#endif /* _IBLUE_TAP_H_ */
