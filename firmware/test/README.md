# Test firmware (not tested on hardware yet)

| File | Branch / commit | Status |
|---|---|---|
| `BlueRetro_iBlueControlMod_unified_test_0c440c8.bin` | `unified` @ 0c440c8 (hw2 `dreamcast_iblue`, built with ESP-IDF 030c995, no hardware tag) | Boots in the ESP32 emulator, but has not been tried on an adapter, a console or with Samba de Amigo |

The baseline tested on hardware is still `../BlueRetro_iBlueControlMod_RGB_v1.bin`. Hold the button for 30 s to return to the vendor firmware.

## What's in it, compared with v1

- **LED behaviour:**
  - The port LEDs don't blink unless a Bluetooth controller is connecting; the power LED blinks while one connects.
  - Orange, blue or a custom colour: a double tap moves to the next colour, and holding on the second tap sweeps through colours and saves the one you stop on.
  - A fast fade-in, capped at 80 % brightness.
- **Taps:** a single tap and a triple tap do nothing by default. You can map them in the hosted web config.
- **Web config:** the redesigned one-page web config from the integration branch (Device, LEDs, Buttons, Controllers, Games, Update, Advanced), with the LED boot animation settings.
- **Game ID:** the adapter answers Maple command `0x21` like VM2, VMU Pro and Pico2Maple, and reports `iBlueControlMod ` in its extended device info. openMenu sends the game ID when a game starts.
- **DC API (`dc1`):** a text interface over Maple command `0x22` and BLE `0x51`. It covers port modes, mapping (`port.X.map.<OUTPUT>`, layouts), deadzone, and `sys.commit` / `sys.revert`. See `docs/dcapi.md`.
- **Port status:** BLE command `0x50`, the command DC-DreamDash's iBlue module reads (feature bit `0x20`).
- **Samba de Amigo:**
  - A **Maracas Controller** output mode (the console sees Sega's maracas: `0x003C090F`, "Maracas Controller").
  - **Joy-Con motion:** two Joy-Cons are the two maracas. Tilt sets the height, turning sets the sideways position, and a shake presses the shake. Calibrate by holding poses. BLE `0x4B`–`0x4F`.
  - A **Racing Controller** output mode (the maracas mode builds on it).
  - All three can be set in the web config (Controllers), or from openMenu with `set port.A.mode maracas`. See `docs/samba-de-amigo.md`.

### Fixed in these builds (first time the code went through ESP-IDF)

- The image didn't fit the ESP32's instruction RAM (4.6 KB over). The DC API runs in the controller-port interrupt, so it has to live there. The fix leaves the other consoles' drivers out of the iBlue build, since the board can only be a Dreamcast adapter. A different system chosen in the config is now ignored on this board.
- The game ID check, which runs in that interrupt, read a constant from flash. It could have crashed the adapter if a game ID arrived during a flash write, such as a VMU save. The constant is in RAM now.
- The Joy-Con motion code from the integration branch didn't compile: it added a second handler for a reply BlueRetro already handles. Each build now has exactly one (517de0d).

## What to check on hardware

1. **Install and boot.** Flash it over OTA on blueretro.io and confirm the LED comes on orange.
2. **Pairing.** Pair a controller and play something. Check that rumble and the VMU still work. The extended-info reply is longer now, so confirm the BIOS and games still see the VMU normally.
3. **LED gestures.** Try the double tap, and the second-tap hold to pick a colour.
4. **Game ID.** Needs the openMenu fork, which knows the `iBlueControlMod ` marker. Launch a game, then connect blueretro.io: it should show the game ID `DC_<product number>`. A configuration saved for that game should load the next time it starts.
5. **VMU save during a game ID.** Save to the VMU from inside a game, to exercise flash writes while the bus is busy.
6. **Joy-Cons without motion.** With Motion mode off (the default), Joy-Cons should connect and play exactly as before.
7. **Samba de Amigo, sticks.** In Controllers, apply "Samba de Amigo maracas" to port A with any pad. Does the game see the maracas, and which stick direction gives which height?
8. **Samba de Amigo, Joy-Cons.** In Controllers, turn Motion mode on, reconnect the left and then the right Joy-Con, apply "Samba de Amigo (Joy-Con motion)" to port A, and calibrate Forward, Up and Out. Watch the Live view, then try the game.
9. **DreamDash.** With the DC-DreamDash `unified` iBlue module, the Pi should show which controller is on which port.

Check the download against `SHA256SUMS.txt`.
