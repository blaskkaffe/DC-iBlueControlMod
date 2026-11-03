# Features of this fork: what exists, where it is, how far it got

Status words: **seen** = tried on a real board (by the owner, mostly through videos); **built** = compiles in CI, not run; **branch** = written, on another branch, not in this build; **planned** = decided, not written; **dropped**.
Everything below was last reviewed with the owner on 2026-11-01. Where something is only "built", nothing about it has been seen on hardware.

## Firmware (ESP32, `main/`; the fork's own code is in `main/iblue/`, its options in `main/iblue/Kconfig.iblue`)

| Feature | What it does | Code | Status |
|---|---|---|---|
| Baseline firmware | the hardware-tested RGB build on the newest BlueRetro, kept as it is as the base | `firmware/BlueRetro_iBlueControlMod_RGB_v1.bin` | seen |
| Port LEDs | never blink without a Bluetooth controller; a connected controller's LED fades in logarithmically | `iblue_port_led.c` | behaviour **seen**, brightness (20 %, a setting) **built** |
| Power LED | orange (default), blue, custom; fully on at start; blinks while a controller connects; solid during the boot animation; pairing breath starts at full | `iblue_led.c` | **seen**. The blue flash at power-on is the chip's (before any firmware runs): it may need hiding, it is acceptable |
| Colour taps | double tap = next colour; tap and hold on the second tap sweeps the hue, release locks a custom colour | `iblue_tap.c`, `iblue_led.c` | **seen** |
| Boot animation (drops) | four LEDs as a line through water: drops, ripples, mirror images from the walls, a water level curve | `iblue_anim_curve.c` | **seen** |
| Intro | the scanner (A, B, C, D) was seen; the **fireworks** intro that replaced it (16 short sparks, faster and faster, then the boom) is **built** | `iblue_anim_curve.c` | scanner seen, fireworks built |
| Animation settings | length (15 s), controller LED brightness, LED distance, water density, tank length, group position, depth, water level curve; set on the web page, "Save and play" | `iblue_anim_cfg.c`, BLE 0x47 to 0x4A | built, not tried |
| Start-up order | the chip's lit power LED and port A stay on and the script takes over (no off in between); LED tasks above the start-up task; the error LED leaves GPIO14 alone | `manager.c`, `iblue_port_led.c`, `led.c` | built, looks right in videos |
| Tap actions | single, double, triple tap, each mappable (nothing, pair, stop pairing, next colour, disconnect, profile 1 to 4, next profile, game id on/off); single and triple do nothing by default | `iblue_gesture.c` | **seen**; the list of actions needs changes and settings (see TODO) |
| Profiles | four slots, each a whole configuration (mapping, output mode, accessories, global settings), saved and loaded from the web page or by a tap | `iblue_profile.c` | built (how to use: below) |
| Dreamcast game id | openMenu writes the game's product number into one reserved VMU block, the adapter makes it BlueRetro's game id, so a game can have its own configuration (`DC_<id>`) | `iblue_gameid.c`, openMenu `claude/gameid` | built, **not tested** (needs the openMenu side and the VMU accessory on) |
| Game id switch | a tap action turns game ids on and off (the LED dips once for on, twice for off) | `iblue_gameid_ctl.c` | **seen** |
| Maracas output mode (Samba de Amigo, phase 1: the two sticks are the two maracas, L1 / R1 shake, L2 = C, R2 / Start = Start; a preset on the web page) | `maple.c`, `docs/samba-de-amigo.md` | built, **not tried with the game** |
| Racing output mode | the Dreamcast racing controller (`0x000700FE`: A, B, Start, d-pad, steering, accelerator, brake) | `maple.c` | built, not tried on a console |

## Web config (`web/`, published on GitHub Pages from `gh-pages`)

One page and one Bluetooth connection (redesigned 2026-11): a menu of categories (Device, LEDs, Buttons, Controllers, Games, Update, Advanced), light and dark, usable on a phone. The iBlue settings are panels (`web/panels/`); the BlueRetro pages (mapping, update, files, VMU, N64 pak, debug) are shown inside it on the same connection. A **Dreamcast only** setting hides the other consoles. See `web/README.md`. Status: works against a mocked adapter (71 checks); not tried on a real adapter.

## On other branches

| Feature | Where | Status |
|---|---|---|
| Control channel `brctl`: a private Maple-bus channel from openMenu to the adapter (status, whole config, profiles, per-game config) | `claude/restructure` | branch, **needs testing** |
| openMenu Controllers screen (remap, type, rumble, VMU, per-button) on `brctl` | openMenu `claude/iblue-controllers` | branch, needs `brctl` |
| Hotkey Home + a face button for profiles, per-game settings from openMenu | `claude/restructure` | branch; to be replaced by the customizable combos below |
| Plain HW1 / HW2 builds with these features (`dreamcast_openmenu` configs) | `claude/restructure` | branch, **keep for now** (not sure it is needed) |

## How to use profiles

A profile is a named copy of the **whole** configuration. Set the adapter up (for example with the 6 button preset on the presets page), then open the iBlueControlMod page, type a name under Profiles and press **Save current here** on a slot. Later **Load** puts that configuration back (and keeps it after a restart). To switch without a computer, map a tap (double or triple, on the same page) to "Load profile N" or "Load the next saved profile". Profiles survive a factory reset. A profile is a manual switch; a game's own configuration (game id) loads by itself when the game starts.
