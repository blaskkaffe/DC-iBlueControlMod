/* iBlueControlMod: the controller port pins in a known state at start-up.
 *
 * The firmware cannot count on the chip's reset state: the adapter is powered while the console is off, so it rarely
 * sees a power-on reset, and a software reset (restart, OTA update, the button holds, a wake from deep sleep) leaves
 * the RTC registers as they were: the RTC pad mux, its pulls, pad holds, the DACs (GPIO25 / GPIO26), the touch sensor
 * (GPIO27, GPIO32, GPIO33, ...) and the 32 kHz crystal pads (GPIO32 / GPIO33). Port D's two Maple pins (GPIO26,
 * GPIO27) and the sense pins of C and D (GPIO32, GPIO33) are such pads; ports A to C's Maple pins are not.
 *
 * iblue_pin_clean() sets one pin as if nothing had touched it: analog functions off, RTC mux off (digital), every
 * hold off, GPIO function, no pulls, no sleep settings, output off, plain GPIO output signal, push-pull, no interrupt,
 * input on or off. The normal pin set-up (maple_port_cfg, the sense inputs) then starts from there. */
#ifndef _IBLUE_PINS_H_
#define _IBLUE_PINS_H_

#include <stdint.h>

void iblue_pin_clean(uint32_t pin, int input);
/* the hold of the digital pads (set for deep sleep) */
void iblue_pins_release_holds(void);

#endif /* _IBLUE_PINS_H_ */
