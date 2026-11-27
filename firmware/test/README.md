# Test firmware

| File | Branch / commit | Status |
|---|---|---|
| `BlueRetro_iBlueControlMod_unified_test_0c440c8.bin` | `unified` @ 0c440c8 (hw2 `dreamcast_iblue`, built with ESP-IDF 030c995, no hardware tag) | **Flashed on an adapter (2026-11-19):** boots, the LED comes on orange, a Bluetooth pad pairs and plays, rumble and VMU work. **Joy-Con motion does not work as it should:** in the maracas setup the two Joy-Cons end up on separate ports instead of being the two maracas of one port. Not tried with Samba de Amigo |
| `BlueRetro_iBlueControlMod_step1_b32d9c0.bin` | `stepwise` @ b32d9c0 = 0c440c8 + step 1 | DC API: Joy-Con motion keys and live heights (`motion.*`, from ab2c57d, fefbfa8), plus a fix: an absent Joy-Con's height read a flash constant in the controller-port interrupt. Not tried on hardware |
| `BlueRetro_iBlueControlMod_step2_0268639.bin` | + step 2 | DC API also on the controller unit, no VMU needed (6186f13). Not tried on hardware |
| `BlueRetro_iBlueControlMod_step3_e9807db.bin` | + step 3 | Samba de Amigo sets up the maracas by itself when openMenu reports the game and a Joy-Con with motion is connected; web steps for the Joy-Con preset (Samba half of e495c0d). Not tried on hardware |
| `BlueRetro_iBlueControlMod_step4_7a4de33.bin` | + step 4 | VMU: format a blank card, store a write only when it arrived intact (else ask the console to resend), answer the beeper (VMU half of e495c0d). Not tried on hardware |
| `BlueRetro_iBlueControlMod_step5_e85d9fe.bin` | + step 5 | DC API in keyboard and mouse mode too (6c32142). The code of `unified` today plus the step 1 fix. Not tried on hardware |
| `BlueRetro_iBlueControlMod_step6_1eb3e4a.bin` | + step 6 | **Port fix:** the adapter claims no port at start-up and never answers on a port with a physical controller (a wired pad on D did not work on 0c440c8). Not tried on hardware |
| `BlueRetro_iBlueControlMod_step7_228fa84.bin` | + step 7 | The DreamPi Bluetooth link removed (BLE DC API `0x51`), as the idea is dropped. Port D: tried on step 6, **still does not work**; the port and sense code match the vendor firmware (checked in its machine code), so the cause is not found yet. Not tried on hardware |
| `BlueRetro_iBlueControlMod_step8_3472411.bin` | + step 8 | Stored hardware overrides (settings section "hw") are ignored, as in the vendor firmware: the last port-related difference to it. Port D's pins, sense test and driver code are otherwise the same in vendor, v1.9 and this build. Not tried on hardware |
| `BlueRetro_iBlueControlMod_step9_95cf304.bin` | + step 9 | **The boot animation is gone** (with its settings, the web config's LEDs page and BLE 0x47-0x4A). The port LEDs work as in the vendor firmware: lit with a controller, and the LED of the port the next controller gets (port 1 while none is connected) **pulses smoothly while pairing is open**. New controllers pair only then: 60 s at start-up and console power-on, after a **single tap** (now the default) or a 6 s hold, or from openMenu with `call pair.start`; paired controllers reconnect at any time. Not tried on hardware |
| `BlueRetro_iBlueControlMod_step10_425c5c8.bin` | + step 10 | **Port check** for port D: Device > Port check in the test web config (`https://blaskkaffe.github.io/DC-iBlueControlMod/test/`) measures the controller port pins on the board and gives a report to send (`docs/port-check.md`). Otherwise as step 9. Not tried on hardware |
| `BlueRetro_iBlueControlMod_step11_8599637.bin` | + step 11 | **Port pins from a known state**: at start-up every Maple and sense pin is set from scratch, including the RTC side of RTC pads (port D's GPIO26 / GPIO27, the sense pins GPIO32 / GPIO33), which a restart or OTA update does not reset and the firmware used to leave alone; the latches deep sleep puts on the LED pins are released. See `docs/port-pins.md`. Port check as step 10. Not tried on hardware |
| `BlueRetro_iBlueControlMod_step12_667e328.bin` | + step 12 | **A port with a physical controller is let go of at once**, also while the memory card has writes waiting (a port update used to wait for the card, for ever if storing failed, so a Bluetooth controller kept answering on D after a pad was plugged in). Port check 2: which Bluetooth controller is on which port, the ports the adapter should answer on, held-back updates, answers during the check. Not tried on hardware |
| `BlueRetro_iBlueControlMod_step13_21fc900.bin` | + step 13 | **Maple sniffer** in the port check (port check 3): listens 500 ms on every port the adapter does not answer on, never sends, and reports the frames from the console and from the controller per port (`docs/port-check.md`). Not tried on hardware |

Each step builds on the one before (branch `stepwise`, from 0c440c8). Try them in order; if one breaks something, the change of that step did it. All of them: built with ESP-IDF 030c995, no warnings, host tests pass, no flash reads from the controller-port interrupt, boot in the ESP32 emulator.

Running on hardware: `../BlueRetro_iBlueControlMod_RGB_v1.bin` (fully tested) and `BlueRetro_iBlueControlMod_unified_test_0c440c8.bin` (the basics, see above). The 30 s hold does **not** reliably return to the vendor firmware (on this board it only rebooted); the sure way back is the USB-serial header.

## Results on hardware (0c440c8, 2026-11-19)

| Check | Result |
|---|---|
| Install over OTA and boot, LED orange | works |
| Bluetooth pad pairs and plays | works |
| Rumble and VMU | work |
| Two Joy-Cons in maracas mode (Joy-Con motion) | **does not work as it should**: the two Joy-Cons land on separate ports; they should be the left and right maraca of one port |
| A wired controller on port D | **does not work** on any build up to step 9 (2026-11-25); the vendor firmware's port and sense code is the same as ours. Step 10 adds a port check that measures the pins on the board (`docs/port-check.md`); step 11 sets the pins from scratch. A Bluetooth controller works on D (2026-11-25), so the adapter's pins reach the console. **Port check on step 11 (2026-11-25):** the adapter answers on no port, D's pins are input off / output off / pull-up only, RTC side clean (digital, no DAC, touch or hold); the console's polling reaches D's pins like every port, but a pad in D answers nothing (D shows the ~70 edges of an empty port, a pad in A 318) although its sense pin reads plugged. So the pad gets power and ground but not the data lines: the iBlue's port D socket, pins 1 and 5, or their traces. Not a firmware fault |
| LED gestures, game ID, VMU save during a game ID, Samba de Amigo | not reported yet |

## What's in it, compared with v1

- **LED behaviour:**
  - The port LEDs don't blink unless a Bluetooth controller is connecting; the power LED blinks while one connects. (Step 9: the port LEDs are the vendor firmware's again, with a smooth pairing pulse.)
  - Orange, blue or a custom colour: a double tap moves to the next colour, and holding on the second tap sweeps through colours and saves the one you stop on.
  - A fast fade-in, capped at 80 % brightness.
- **Taps:** a single tap and a triple tap do nothing by default (step 9: the single tap opens pairing). You can map them in the hosted web config.
- **Web config:** the redesigned one-page web config from the integration branch (Device, LEDs, Buttons, Controllers, Games, Update, Advanced), with the LED boot animation settings (removed in step 9).
- **Game ID:** the adapter answers Maple command `0x21` like VM2, VMU Pro and Pico2Maple, and reports `iBlueControlMod ` in its extended device info. openMenu sends the game ID when a game starts.
- **DC API (`dc1`):** a text interface over Maple command `0x22` (BLE `0x51` removed in step 7). It covers port modes, mapping (`port.X.map.<OUTPUT>`, layouts), deadzone, and `sys.commit` / `sys.revert`. See `docs/dcapi.md`.
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
