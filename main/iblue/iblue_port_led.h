/*
 * iBlueControlMod: the four controller port LEDs. Each has its own PWM channel (LEDC high speed
 * channels 4 to 7, timer 3), so one LED can fade without the others. They
 *  - fade in quickly when a controller connects and out when it goes (fast at first, slow at the
 *    end, so it looks logarithmic), never brighter than CONFIG_BLUERETRO_IBLUE_LED_MAX_PCT, and
 *  - show the drops-on-water animation while the BIOS boot animation plays.
 * The pairing pulse and the button-hold flash of system/manager.c still take a pin over by
 * connecting it to another LEDC signal; set() takes it back.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_PORT_LED_H_
#define _IBLUE_PORT_LED_H_

#include <stdint.h>
#include <stdbool.h>
#include "sdkconfig.h"

#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED

/* pins: the port LED GPIOs A to D. Call once the port count is known. */
void iblue_port_led_init(const uint8_t *pins, uint32_t count);
/* The LED of this port should be on (a controller is connected) or off. Takes the pin back. */
void iblue_port_led_set(uint32_t index, bool on);
/* The console powered on `elapsed_ms` ago (the adapter was still booting): run the rest of the boot
 * animation. The port LEDs belong to it until it is over. */
void iblue_port_led_boot_anim(uint32_t elapsed_ms);
/* Play the whole boot animation now from its start (for trying the settings); not while one plays */
void iblue_port_led_play(void);
/* Something else routed a port LED pin (LEDC channel setup): give every pin back to its PWM channel */
void iblue_port_led_reclaim(void);
/* True while the boot animation runs */
bool iblue_port_led_busy(void);

#else
#define iblue_port_led_busy() false
#define iblue_port_led_reclaim()
#define iblue_port_led_play()
#endif

#endif /* _IBLUE_PORT_LED_H_ */
