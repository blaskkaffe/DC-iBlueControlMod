/* iBlueControlMod: tap counting. See iblue_tap.h. */

#include "iblue/iblue_tap.h"

enum { PH_GAP = 0, PH_PRESS, PH_HOLD };

void iblue_tap_start(struct iblue_tap *t) {
    t->taps = 1;
    t->phase = PH_GAP;
    t->ms = 0;
}

int iblue_tap_step(struct iblue_tap *t, int pressed, uint32_t dt_ms) {
    switch (t->phase) {
        case PH_GAP:
            if (pressed) {
                t->phase = PH_PRESS;
                t->ms = 0;
                return IBLUE_TAP_EV_NONE;
            }
            t->ms += dt_ms;
            return t->ms >= IBLUE_TAP_GAP_MS ? IBLUE_TAP_EV_DONE : IBLUE_TAP_EV_NONE;
        case PH_PRESS:
            if (pressed) {
                t->ms += dt_ms;
                if (t->taps == 1 && t->ms >= IBLUE_TAP_HOLD_MS) {
                    t->phase = PH_HOLD;
                    return IBLUE_TAP_EV_HOLD_START;
                }
                return IBLUE_TAP_EV_NONE;
            }
            t->taps++; /* released: that was another tap */
            if (t->taps >= IBLUE_TAP_MAX) {
                return IBLUE_TAP_EV_DONE;
            }
            t->phase = PH_GAP;
            t->ms = 0;
            return IBLUE_TAP_EV_NONE;
        default: /* PH_HOLD */
            return pressed ? IBLUE_TAP_EV_NONE : IBLUE_TAP_EV_HOLD_END;
    }
}
