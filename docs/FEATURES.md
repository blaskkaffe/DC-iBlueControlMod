# Features of this fork: what exists, where it is, how far it got

Status words: **seen** = tried on a real board (by the owner, mostly through videos); **built** = compiles in CI, not run; **branch** = written, on another branch, not in this build; **planned** = decided, not written; **dropped**.
Everything below was last reviewed with the owner on 2026-11-01. Where something is only "built", nothing about it has been seen on hardware.

## Firmware (ESP32, `main/`; the fork's own code is in `main/iblue/`, its options in `main/iblue/Kconfig.iblue`)

| Feature | What it does | Code | Status |
|---|---|---|---|
| Baseline firmware | the hardware-tested RGB build on the newest BlueRetro, kept as it is as the base | `firmware/BlueRetro_iBlueControlMod_RGB_v1.bin` | seen |
| Port LEDs | as the vendor firmware: lit with a Bluetooth controller connected, dark otherwise; the LED of the port the next controller gets pulses smoothly while the pairing window is open (2026-11-24: the boot animation, its settings and the fade-in were removed) | `manager.c`, `led.c` | **built** (step 9), not tried |
| Power LED | orange (default), blue, custom; fully on at start; blinks while a controller connects; pairing breath starts at full | `iblue_led.c` | **seen**. The blue flash at power-on is the chip's (before any firmware runs): it may need hiding, it is acceptable |
| Colour taps | double tap = next colour; tap and hold on the second tap sweeps the hue, release locks a custom colour | `iblue_tap.c`, `iblue_led.c` | **seen** |
| Start-up order | the power LED is taken over first thing in its saved colour; the error LED channel leaves GPIO14 (green) alone and starts on the port 1 LED | `manager.c`, `led.c` | built |
| Tap actions | single, double, triple tap, each mappable (nothing, pair, stop pairing, next colour, disconnect, game id on/off); the single tap opens the pairing window by default (2026-11-24), the triple does nothing | `iblue_gesture.c` | **seen** (the new single tap default: built); the list of actions needs changes and settings (see TODO) |
| Dreamcast game id | openMenu writes the game's product number into one reserved VMU block, the adapter makes it BlueRetro's game id, so a game can have its own configuration (`DC_<id>`) | `iblue_gameid.c`, openMenu `claude/gameid` | built, **not tested** (needs the openMenu side and the VMU accessory on) |
| Game id switch | a tap action turns game ids on and off (the LED dips once for on, twice for off) | `iblue_gameid_ctl.c` | **seen** |
| Maracas output mode (Samba de Amigo, phase 1: the two sticks are the two maracas, L1 / R1 shake, L2 = C, R2 / Start = Start; a preset on the web page) | `maple.c`, `docs/samba-de-amigo.md` | built, **not tried with the game** |
| Joy-Con motion as maracas: IMU parsed, height / sideways / shake, calibration by poses (web page and Minus / Home hold), preset "Samba de Amigo (Joy-Con motion)" | `iblue_motion.c`, `iblue_motion_ctl.c`, `sw.c`, `docs/samba-de-amigo.md` | built, maths host tested, **not tried on hardware** |
| Racing output mode | the Dreamcast racing controller (`0x000700FE`: A, B, Start, d-pad, steering, accelerator, brake) | `maple.c` | built, not tried on a console |

## Web config (`web/`, published on GitHub Pages from `gh-pages`)

One page and one Bluetooth connection (redesigned 2026-11): a menu of categories (Device, Buttons, Controllers, Games, Update, Advanced; the LEDs page went with the boot animation), light and dark, usable on a phone. The iBlue settings are panels (`web/panels/`); the BlueRetro pages (mapping, update, files, VMU, N64 pak, debug) are shown inside it on the same connection. A **Dreamcast only** setting hides the other consoles. See `web/README.md`. Status: works against a mocked adapter (71 checks); not tried on a real adapter.

## On other branches

| Feature | Where | Status |
|---|---|---|
| Control channel `brctl`: a private Maple-bus channel from openMenu to the adapter (status, whole config, per-game config) | `claude/restructure` | branch, **needs testing** |
| openMenu Controllers screen (remap, type, rumble, VMU, per-button) on `brctl` | openMenu `claude/iblue-controllers` | branch, needs `brctl` |
| Hotkey Home + a face button, per-game settings from openMenu | `claude/restructure` | branch; to be replaced by the customizable combos below |
| Plain HW1 / HW2 builds with these features (`dreamcast_openmenu` configs) | `claude/restructure` | branch, **keep for now** (not sure it is needed) |

