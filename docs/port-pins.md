# The controller port pins: chip defaults and what the firmware sets

The pins come from the vendor firmware (checked in its machine code) and are the same in BlueRetro v1.9 for this board. "Pin 0" carries the Maple interrupt; "pin 1" is the other data line.

## Chip reset state (ESP32 datasheet, IO_MUX "at reset")

| Port | Maple pin 0 | Maple pin 1 | Sense |
|---|---|---|---|
| A | GPIO21: input on, no pull; digital pad | GPIO22: input on, no pull; digital pad | GPIO36: input only, no pulls; RTC pad |
| B | GPIO3: UART0 RX function, input on, **pull-up**; digital pad | GPIO5: input on, **pull-up** (strapping pin); digital pad | GPIO35: input only, no pulls; RTC pad |
| C | GPIO18: input on, no pull; digital pad | GPIO23: input on, no pull; digital pad | GPIO32: **input off**; RTC pad (32 kHz crystal P, touch 9) |
| D | GPIO26: **input off**; **RTC pad** (DAC 2, ADC2 ch 9) | GPIO27: input on, no pull; **RTC pad** (touch 7, ADC2 ch 7) | GPIO33: **input off**; RTC pad (32 kHz crystal N, touch 8) |

Port D is the only port whose Maple pins are both RTC pads. An RTC pad has a second set of controls in the RTC domain: a mux that cuts the pad off from the digital side, its own pulls, a hold latch, and the analog parts (DAC, touch sensor, crystal oscillator), which drive the pad when on.

The reset state only lasts while the chip starts (well under a second): the firmware sets every pin below. And the RTC domain is **not** reset by a software reset (restart, OTA update, the button holds, waking from deep sleep), only by a power-on reset. The adapter is powered while the console is off, so it rarely gets one; what an earlier firmware left in the RTC registers can stay.

## What the firmware sets

| State | Maple pins | Sense pins |
|---|---|---|
| Start (from step 11, `iblue_pins.c`) | DAC / touch / crystal off, RTC mux off (digital), every hold off, GPIO function, no pulls, no sleep settings, output off, plain GPIO output, push-pull, no interrupt | the same, input on |
| Port the adapter does not answer on (`gpio_reset_iram`) | GPIO function, input off, output off, push-pull, internal pull-up on (~45 kΩ; ports A to C in IO_MUX, port D in its RTC pad registers), no interrupt; from step 11 also RTC mux off and hold off | - |
| Port the adapter answers on (`maple_port_cfg`) | input on, no pulls, falling-edge interrupt on pin 0; driven push-pull only while it sends an answer, then input again | - |
| Always | - | input, no pulls (the board pulls them up: empty reads high, a controller pulls it low) |

Before step 11 the firmware set the GPIO side of the pins but never the RTC side of an RTC pad: it relied on the reset state there. ESP-IDF's own `gpio_config` does set it (`rtc_gpio_deinit`); BlueRetro's IRAM copy of it (`gpio.c`) did not. Step 11 adds that to `gpio_config_iram` and sets the port pins from scratch at start-up, and releases the latches deep sleep sets on the LED pins.

A port the adapter does not answer on loads each data line with one ~45 kΩ pull-up and nothing else, the same on all four ports: that is not enough to stop a controller from working. The Bluetooth test on port D (2026-11-25: a Bluetooth controller works there) shows the adapter's GPIO26 / GPIO27 reach the console. The part of a physical controller's path that test does not cover is the iBlue's own port D socket and its traces to where the adapter's pins join them.

## Releasing a port (step 12)

The system manager applies a new port set only while the memory card has nothing waiting to be stored (`mc_get_state()`), as upstream BlueRetro does; if storing keeps failing it would wait for ever, and a Bluetooth controller would keep answering on a port a physical controller was plugged into. From step 12 a port whose sense input says a physical controller is there is let go of at once, card busy or not; the rest of the update still waits.
