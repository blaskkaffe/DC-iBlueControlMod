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

When the console powers on the four port LEDs run the animation for the length of the BIOS boot animation, then show the real state. Details of the shape (ripples, the rising and falling water level, drop schedule) are in [led-behaviour.md](led-behaviour.md). Settings: `BLUERETRO_IBLUE_BOOT_ANIM` (on), `BLUERETRO_IBLUE_BOOT_ANIM_MS` (**6000, a guess: time your console and set it**), `BLUERETRO_IBLUE_BOOT_ANIM_BOOT_MS` (700, the adapter's own start-up, also a guess).

## Dreamcast game id

BlueRetro can keep one configuration per game: it learns the game's id from the console (N64, GameCube, PlayStation), and blueretro.io shows the id and can **save a configuration for the current game**. The Dreamcast never tells the adapter anything by itself, so **openMenu does**, the way it already does for VM2, VMU Pro, USB4Maple and Pico2Maple: before it starts a game it sends the game's product number to the memory card unit with **Maple command 0x21** (12 bytes of id, then optionally the game's name). The adapter answers with an acknowledge and makes the id BlueRetro's game id (`DC_<product>`, the first 10 characters) with the existing code (`gid_update_sys()`, `config_init(GAMEID_CFG)`): the saved configuration of that game is loaded if there is one, otherwise the normal one.

To be sent the id at all, openMenu has to recognise the adapter: it asks the memory card unit for the extended device info (Maple command 0x02) and looks at the first 16 bytes of the "extended" field. The adapter answers with `iBlueControlMod ` there (a real VMU leaves it empty). The openMenu fork of this family knows that string; stock openMenu does not (it knows VM2, VMU Pro, USB4Maple and Pico2Maple).

Rules, the same as Pico2Maple's: the id `DCBIOS` is ignored, an empty id means "no game" (back to the normal configuration), the same id again is not applied again, and only letters, digits, `-` and `_` are accepted (the id becomes part of a file name). Unlike the old mailbox this replaces, openMenu sends no empty id when it opens its own screens: the game configuration stays until the next game's id arrives or the adapter restarts.

How to use it: start a game from openMenu, open blueretro.io, connect, and the game id is shown; change the mapping and save it as a game configuration. The next time you start the game from openMenu it is loaded. Game configurations (`DC_*` files) **survive the factory reset**.

The frame, the byte order (every 4 bytes of text are reversed on the Maple bus) and the rules are in `main/system/iblue_gameid.h`. `sh main/tests/host/run.sh` tests the id handling on a PC (valid and invalid ids, repeats, padding, word order). **Status: written and host-tested; not built with ESP-IDF here and not tried on a console.** The old VMU block 200 mailbox was removed.

Needs: a Bluetooth controller connected (the board only answers on a port that has one) and the **VMU accessory on** for that port. A new configuration has the VMU on for port A; an existing configuration with the VMU off has to switch it on in blueretro.io once.

## Not verified

Nothing has run on a board: the build compiles in CI. Whether blocks 200 and up are really never used by a game or the BIOS file manager; whether the openMenu side starts the game before the adapter has applied the id (it reads the block back and retries); the colours of the sweep; how the fades look on your LEDs; the BIOS animation length.
