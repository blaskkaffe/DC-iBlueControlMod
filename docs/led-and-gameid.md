# LEDs, boot animation and game id (test firmware)

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
- A single tap still opens the 60 s pairing window; the longer holds (3, 6, 10, 30 s) are as before.

## Port LEDs A to D

- They **never blink without a Bluetooth controller connected**: no pairing pulse, no flash while the button is held. The power LED shows pairing and connecting.
- A controller's LED **fades on quickly** when it connects (about a quarter of a second, fast at first and slow at the end, so it looks logarithmic) and fades out when it goes.
- With a controller connected, the pairing pulse on the next free port and the hold-flash work as before.
- Each LED has its own PWM channel (LEDC high speed 4 to 7, timer 3).

**Maximum brightness**: the power LED and the port LEDs never get brighter than `BLUERETRO_IBLUE_LED_MAX_PCT` percent of the PWM duty (default **80**). I applied it to both; say if you wanted only one of them.

## Boot animation: drops falling on water

The LEDs are taken over first thing at start-up (`sys_mgr_iblue_early()`, before the file system and Bluetooth start, which used to take seconds with the pins in their power-up state). When the console powers on the four port LEDs run the animation for the length of the BIOS boot animation, then show the real state. Details of the shape (ripples, the rising and falling water level, drop schedule) are in [led-behaviour.md](led-behaviour.md). Settings: `BLUERETRO_IBLUE_BOOT_ANIM` (on), `BLUERETRO_IBLUE_BOOT_ANIM_BOOT_MS` (700, the adapter's own start-up, a guess); the length of the whole animation (15 s from the console's power-on, **a guess: time your console**) and the look of the water are settings on the hosted web config, see "LED settings" below.

## Dreamcast game id

BlueRetro can keep one configuration per game: it learns the game's id from the console (N64, GameCube, PlayStation), and blueretro.io shows the id and can **save a configuration for the current game**. The Dreamcast never tells the adapter anything by itself, so **openMenu does**, the way it already does for VM2, VMU Pro, USB4Maple and Pico2Maple: before it starts a game it sends the game's product number to the memory card unit with **Maple command 0x21** (12 bytes of id, then optionally the game's name). The adapter answers with an acknowledge and makes the id BlueRetro's game id (`DC_<product>`, the first 10 characters) with the existing code (`gid_update_sys()`, `config_init(GAMEID_CFG)`): the saved configuration of that game is loaded if there is one, otherwise the normal one.

To be sent the id at all, openMenu has to recognise the adapter: it asks the memory card unit for the extended device info (Maple command 0x02) and looks at the first 16 bytes of the "extended" field. The adapter answers with `iBlueControlMod ` there (a real VMU leaves it empty). The openMenu fork of this family knows that string; stock openMenu does not (it knows VM2, VMU Pro, USB4Maple and Pico2Maple).

Rules, the same as Pico2Maple's: the id `DCBIOS` is ignored, an empty id means "no game" (back to the normal configuration), the same id again is not applied again, and only letters, digits, `-` and `_` are accepted (the id becomes part of a file name). Unlike the old mailbox this replaces, openMenu sends no empty id when it opens its own screens: the game configuration stays until the next game's id arrives or the adapter restarts.

How to use it: start a game from openMenu, open blueretro.io, connect, and the game id is shown; change the mapping and save it as a game configuration. The next time you start the game from openMenu it is loaded. Game configurations (`DC_*` files) **survive the factory reset**.

The frame, the byte order (every 4 bytes of text are reversed on the Maple bus) and the rules are in `main/iblue/iblue_gameid.h`. `sh main/tests/host/run.sh` tests the id handling on a PC (valid and invalid ids, repeats, padding, word order). **Status: written and host-tested; not built with ESP-IDF here and not tried on a console.** The old VMU block 200 mailbox was removed.

Needs: a Bluetooth controller connected (the board only answers on a port that has one) and the **VMU accessory on** for that port. A new configuration has the VMU on for port A; an existing configuration with the VMU off has to switch it on in blueretro.io once.

## Not verified

Nothing has run on a board: the build compiles in CI. Whether blocks 200 and up are really never used by a game or the BIOS file manager; whether the openMenu side starts the game before the adapter has applied the id (it reads the block back and retries); the colours of the sweep; how the fades look on your LEDs; the BIOS animation length.

## Game id on / off, Racing mode, per-game controller type

- **Game id on / off**: tap action 10 ("Game id on / off"), mappable to the single, double or triple tap on the hosted web config. Off: games still announce themselves but no game configuration is loaded and the normal configuration is back at once. On: the game that runs gets its configuration. Kept over restarts (NVS `iblue/gid_off`). The LED dips once for on and twice for off.
- **Racing** (output mode 4): function word `0x000700FE`, name "Racing Controller": A, B, Start, d-pad and three analog axes (stick X = steering, right trigger = accelerator, left trigger = brake), no X / Y. The condition packet is the pad's. **Not tried on a console.**
- **Per game**: the configuration BlueRetro saves for a game includes the output mode, so a game can run GamePadAlt (C, Z, D, second d-pad and stick) or Racing while others use the plain GamePad. Some games read the controller's capability mask and misbehave with `0x003FFFFF` (reported for Dolphin Blue), so GamePad stays the default. The hosted web config's Dreamcast presets page writes the type and the default mapping (L1 = Z, R1 = C, L2 / R2 = triggers, sticks as sticks, L3 / R3 unmapped) and can save them for the running game.

## Boot intro: fireworks
Before the drops: **sixteen extremely short blinks (24 ms) of single LEDs, slow at first and faster and faster (350 ms apart, each gap 0.8775 of the last, down to 49 ms), 2.5 s in all, then the boom: all four LEDs at full brightness for 0.15 s, fading out together over 0.35 s**, then the drops. The first blink is A: the chip lights A while it starts and the script takes over from a lit A (A is held lit from the moment the LED code starts until the animation does, 0.5 s at most). The order of the sparks is A C B D B A D C A B D A C D B A (never the same LED twice in a row). The intro is 3.0 s of the animation; the drops are an animation of their own that starts when it is over and ends with the whole animation (15 s after the console's power-on). The intro is skipped when less than 2 s of drops would be left (`BLUERETRO_IBLUE_BOOT_INTRO`). Spec in `iblue_anim_curve.h`, checked in `tests/host/test_anim.c`. Preview: `tests/host/anim_intro_frames.c` prints what the LEDs do (in the 20 ms frames of the LED task), `tests/host/render_intro.py` makes a video of it.

## LED settings (hosted web config, iblue.html)
Saved in NVS (`iblue/anim`) and read at start-up; changed over BLE (commands 0x47 get, 0x48 set, 0x49 play now, 0x4A defaults; the info command's feature bit 3 says the firmware has them). Fields, defaults and ranges: `struct iblue_anim_cfg` in `iblue_anim_curve.h` (the web page has the same table, a test compares them).

| Setting | Default | What it does |
|---|---|---|
| Length of the boot animation | 15 s | from the console's power-on, intro and drops |
| Brightness of a connected controller | 20 % | the port LED of a connected controller: a quarter of the 80 % it was |
| LED distance | 100 % | how far apart the LEDs sit in the water (100 % = one LED spacing) |
| Water density | 100 % | heavier water: ripples travel slower and die out sooner |
| Length between the outer walls | 400 % | the tank, in LED spacings (4.00): the ripples bounce back from the walls |
| Group position | 0 | the four LEDs shifted left (-) or right (+) in the tank |
| Water depth | 100 % | brightness of the water and its ripples (capped at the 80 % LED maximum) |
| Water level curve | 20 % to 42 %, rise at 30 % for 20 %, fade from 45 % | the general brightness over the drops: start level, raised level, where and how long it rises, where it starts to go down to nothing |

The animation brightness stays capped at `BLUERETRO_IBLUE_LED_MAX_PCT` (80 %); the controller LED brightness is its own setting. "Play now" runs the whole animation from its start so a setting can be seen at once.

## Start-up order of the LEDs
The chip lights the power LED and port A while it starts (hardware, before any firmware runs). The firmware never turns them off to turn them on again: **hardware on, kept on, the script takes over**. When the LED code starts (`sys_mgr_iblue_early()`, in `wl_init_task`): the port LEDs are set up first, A is created already lit (the intro begins with A), B, C and D go off, the boot animation starts; then NVS is opened for the saved colour and the power LED channels are created already at their final level (full, in the saved colour). `err_led_init` no longer claims GPIO14, the green pin of the RGB LED (it ran before the LED code and switched the green off). An OTA update cannot change the bootloader, so the first third of a second stays the chip's and the board's.

## Why the intro did not show (1af3ce6)
Both LED tasks ran at priority 4 on core 0, below `wl_init_task` (10), which mounts the file system and brings up Bluetooth and so keeps core 0 busy for seconds. The tasks were not scheduled until that was over (about 3 s), and the animation, which follows real time, then began in the middle of the drops. They run at priority 12 now (they sleep most of the time).

## Why only A and B lit (22a2bdd)
`sys_mgr_init` runs `gpio_config()` on the port LED pins after the intro has started; in ESP-IDF that turns a pin into a plain GPIO output and disconnects it from its LEDC channel. The pins went dark about 1.2 s in (after A and B), and the animation only came back when the start-up path attached them again, 2 to 3 s in. `iblue_port_led_reclaim()` now runs right after that loop. Also, `sys_mgr_iblue_pins_off()` leaves port A lit (it is lit by the chip anyway): the video showed a dip between the chip's A blink and the intro's A.

## Power LED during the boot animation
The power LED stays solid in its colour while the boot animation (intro and drops) plays: the pairing "breathing" and the connecting blink wait until it is over (button feedback and the tap dips still show). Before this the pairing window, which opens at power-on, made the power LED breathe from the start.

## Power LED: on at once, breathing starts at full
- At power-on the power LED comes on **fully** in its saved colour as soon as the firmware sets it up (no fade-in: the fade stuttered while the file system and Bluetooth started).
- It stays solid through the whole boot animation. When the pairing window is open at the end, the **breathing starts at full brightness** (where the solid LED was), goes down to 5 % and comes back up. Before, the wave ran on a clock of its own, so it joined at a random point and the LED jumped.
