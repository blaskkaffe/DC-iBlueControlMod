# Test firmware (not tested on hardware yet)

| File | Branch / commit | Status |
|---|---|---|
| `BlueRetro_iBlueControlMod_unified_test_838cbf9.bin` | `unified` @ 838cbf9 (hw2 `dreamcast_iblue`, built with ESP-IDF 030c995, no hardware tag) | **First ESP-IDF build of `unified`.** Boots in the ESP32 emulator, but has not been tried on an adapter or a console |

The baseline tested on hardware is still `../BlueRetro_iBlueControlMod_RGB_v1.bin`. Hold the button for 30 s to return to the vendor firmware.

## What's in it, compared with v1

- **LED behaviour:**
  - The port LEDs don't blink unless a Bluetooth controller is connecting; the power LED blinks while one connects.
  - Orange, blue or a custom colour: a double tap moves to the next colour, and holding on the second tap sweeps through colours and saves the one you stop on.
  - A fast fade-in, capped at 80 % brightness.
- **Taps:** a single tap and a triple tap do nothing by default. You can map them in the hosted web config.
- **Game ID:** the adapter answers Maple command `0x21` like VM2, VMU Pro and Pico2Maple, and reports `iBlueControlMod ` in its extended device info. openMenu sends the game ID when a game starts.
- **DC API (`dc1`):** a text interface over Maple command `0x22` and BLE `0x51`. It covers port modes, mapping (`port.X.map.<OUTPUT>`, layouts), deadzone, and `sys.commit` / `sys.revert`. See `docs/dcapi.md`.
- **Port status:** BLE command `0x50`.

### Fixed in this build (first time it went through ESP-IDF)

- The image didn't fit the ESP32's instruction RAM (4.6 KB over). The DC API runs in the controller-port interrupt, so it has to live there. The fix leaves the other consoles' drivers out of the iBlue build, since the board can only be a Dreamcast adapter. A different system chosen in the config is now ignored on this board.
- The game ID check, which runs in that interrupt, read a constant from flash. It could have crashed the adapter if a game ID arrived during a flash write, such as a VMU save. The constant is in RAM now.

## What to check on hardware

1. **Install and boot.** Flash it over OTA on blueretro.io and confirm the LED comes on orange.
2. **Pairing.** Pair a controller and play something. Check that rumble and the VMU still work. The game should see the VMU normally, because the extended-info reply is longer now.
3. **LED gestures.** Try the double tap, and the second-tap hold to pick a colour.
4. **Game ID.** Needs the openMenu fork, which knows the `iBlueControlMod ` marker. Launch a game, then connect blueretro.io: it should show the game ID `DC_<product number>`. A configuration saved for that game should load the next time it starts.
5. **VMU save during a game ID.** Save to the VMU from inside a game, to exercise flash writes while the bus is busy.

Check the download against `SHA256SUMS.txt`.
