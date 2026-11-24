# LEDs, pairing and game id (test firmware)

What this firmware changes compared with `main`. Everything is for the iBlueControlMod RGB build (`configs/hw2/dreamcast_iblue`); `dreamcast_iblue_vendor` keeps the factory behaviour (none of this).

## Power LED (RGB) and colours

| When | The power LED |
|---|---|
| a controller is **connecting** (found, not ready yet, at most 20 s) | blinks in the chosen colour (3 Hz) |
| searching for a controller (pairing) | breathes in the chosen colour |
| button held | blinks faster at each hold stage |
| otherwise | solid in the chosen colour, **fading in quickly** when it settles (fast at first, slow at the end) |

Three colours: **orange** (default), **blue** and **custom**.
- **Double tap**: tap, then tap again quickly: next colour (orange, blue, custom, orange ...). The custom colour is skipped until you have set one.
- **Set the custom colour**: tap, then tap again and **keep holding the second tap** for more than 0.6 s. The LED sweeps through all the colours (a full circle in 10 s, starting at red); **release when the colour you want shows up**: it locks in as the custom colour and is selected. It is kept over power cycles.
- A single tap opens the 60 s pairing window (the default; it can be mapped on the web config); the longer holds (3, 6, 10, 30 s) are as before.

## Pairing and the port LEDs A to D (as the vendor firmware, 2026-11-24)

The port LEDs work as in the vendor firmware and BlueRetro: a port with a connected Bluetooth controller is lit, an empty one is dark. There is no boot animation any more (it was removed with its settings, BLE commands and web page).

- **New controllers pair only while the pairing window is open**, and then the LED of the port the next controller gets **pulses smoothly** (port 1 while no controller is connected; the next free port otherwise). The power LED breathes at the same time.
- The window opens for 60 s (`CONFIG_BLUERETRO_IBLUE_PAIR_WINDOW_S`) at start-up, when the console is switched on, with a **single tap** of the button (6 s hold too), or from openMenu with the DC API `call pair.start` (`call pair.stop` closes it, `get pair.state` says `open` or `closed`; see `dcapi.md`). When it closes the adapter stops searching and the pulse stops.
- **Controllers that are already paired reconnect at any time**, also with the window closed (the adapter keeps listening for them; a Bluetooth LE controller through a passive scan).
- The pulse is BlueRetro's own (LEDC high speed channel 0, `err_led_task` in `led.c`); on the iBlue it starts on the port 1 LED (GPIO15), because the pin BlueRetro uses for it, GPIO14, is the green of the RGB LED.
- While the button is held, the port LEDs only flash with a Bluetooth controller connected; the power LED shows the hold stages.

**Maximum brightness**: the power LED never gets brighter than `BLUERETRO_IBLUE_LED_MAX_PCT` percent of the PWM duty (default **80**). The port LEDs have BlueRetro's brightness.

Coming from an earlier test build: the saved animation settings (NVS `iblue/anim`) are erased at start-up, and a saved tap map that is the old default (single tap nothing, double tap next colour, triple tap nothing) gets the new default; a map you changed yourself is kept.

**Not tried on hardware** (step 9).

## Dreamcast game id

BlueRetro can keep one configuration per game: it learns the game's id from the console (N64, GameCube, PlayStation), and blueretro.io shows the id and can **save a configuration for the current game**. The Dreamcast never tells the adapter anything by itself, so **openMenu does**, the way it already does for VM2, VMU Pro, USB4Maple and Pico2Maple: before it starts a game it sends the game's product number to the memory card unit with **Maple command 0x21** (12 bytes of id, then optionally the game's name). The adapter answers with an acknowledge and makes the id BlueRetro's game id (`DC_<product>`, the first 10 characters) with the existing code (`gid_update_sys()`, `config_init(GAMEID_CFG)`): the saved configuration of that game is loaded if there is one, otherwise the normal one.

To be sent the id at all, openMenu has to recognise the adapter: it asks the memory card unit for the extended device info (Maple command 0x02) and looks at the first 16 bytes of the "extended" field. The adapter answers with `iBlueControlMod ` there (a real VMU leaves it empty). The openMenu fork of this family knows that string; stock openMenu does not (it knows VM2, VMU Pro, USB4Maple and Pico2Maple).

Rules, the same as Pico2Maple's: the id `DCBIOS` is ignored, an empty id means "no game" (back to the normal configuration), the same id again is not applied again, and only letters, digits, `-` and `_` are accepted (the id becomes part of a file name). Unlike the old mailbox this replaces, openMenu sends no empty id when it opens its own screens: the game configuration stays until the next game's id arrives or the adapter restarts.

How to use it: start a game from openMenu, open blueretro.io, connect, and the game id is shown; change the mapping and save it as a game configuration. The next time you start the game from openMenu it is loaded. Game configurations (`DC_*` files) **survive the factory reset**.

The frame, the byte order (every 4 bytes of text are reversed on the Maple bus) and the rules are in `main/iblue/iblue_gameid.h`. `sh main/tests/host/run.sh` tests the id handling on a PC (valid and invalid ids, repeats, padding, word order). **Status: written and host-tested; not built with ESP-IDF here and not tried on a console.** The old VMU block 200 mailbox was removed.

Needs: a Bluetooth controller connected (the board only answers on a port that has one) and the **VMU accessory on** for that port. A new configuration has the VMU on for port A; an existing configuration with the VMU off has to switch it on in blueretro.io once.

## Not verified

Nothing has run on a board: the build compiles in CI. Whether blocks 200 and up are really never used by a game or the BIOS file manager; whether the openMenu side starts the game before the adapter has applied the id (it reads the block back and retries); the colours of the sweep; how the fades look on your LEDs.

## Game id on / off, Racing mode, per-game controller type

- **Game id on / off**: tap action 10 ("Game id on / off"), mappable to the single, double or triple tap on the hosted web config. Off: games still announce themselves but no game configuration is loaded and the normal configuration is back at once. On: the game that runs gets its configuration. Kept over restarts (NVS `iblue/gid_off`). The LED dips once for on and twice for off.
- **Racing** (output mode 4): function word `0x000700FE`, name "Racing Controller": A, B, Start, d-pad and three analog axes (stick X = steering, right trigger = accelerator, left trigger = brake), no X / Y. The condition packet is the pad's. **Not tried on a console.**
- **Per game**: the configuration BlueRetro saves for a game includes the output mode, so a game can run GamePadAlt (C, Z, D, second d-pad and stick) or Racing while others use the plain GamePad. Some games read the controller's capability mask and misbehave with `0x003FFFFF` (reported for Dolphin Blue), so GamePad stays the default. The hosted web config's Dreamcast presets page writes the type and the default mapping (L1 = Z, R1 = C, L2 / R2 = triggers, sticks as sticks, L3 / R3 unmapped) and can save them for the running game.

## Start-up order of the LEDs
The chip lights the power LED while it starts (hardware, before any firmware runs). The firmware takes the power LED over first thing (`sys_mgr_iblue_early()`): NVS is opened for the saved colour and the power LED channels are created already at their final level (full, in the saved colour). `err_led_init` does not claim GPIO14, the green pin of the RGB LED. An OTA update cannot change the bootloader, so the first third of a second stays the chip's and the board's.

## Power LED: on at once, breathing starts at full
- At power-on the power LED comes on **fully** in its saved colour as soon as the firmware sets it up (no fade-in: the fade stuttered while the file system and Bluetooth started).
- When the pairing window is open, the **breathing starts at full brightness** (where the solid LED was), goes down to 5 % and comes back up. Before, the wave ran on a clock of its own, so it joined at a random point and the LED jumped.
