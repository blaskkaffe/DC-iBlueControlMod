# BlueRetro on iBlueControlMod (internal Dreamcast mod)

These notes come from reverse engineering the vendor firmware
`SEGA_DC_24V01_iBlueControlMod.bin` (566,224 bytes, ESP32 app image, built
Jan 27 2024 with ESP-IDF v5.1.1-dirty on Windows).
SHA-256 `7d95dd7d8043e542759c8f7ce788764f3306eae94539ad8eec62fe23e1bf87fa`. This is the same
file as the one in [VDfreesince1983/Ibluecontrolmod-Factory-Firmware](https://github.com/VDfreesince1983/Ibluecontrolmod-Factory-Firmware).

## What others have already found

- [BlueRetro discussion #1183](https://github.com/darthcloud/BlueRetro/discussions/1183) covers these AliExpress "iBlueControlMod" boards.
  - zeus86 showed the boards run BlueRetro (his GameCube dump says "Jan 15 2024", "v5.1.1-dirty").
  - darthcloud (the BlueRetro author) compared GPIO init logs and got this pin mapping: **0→16, 2→15, 12→2, 15→12, 16→17**.
    The disassembly here confirms that mapping independently. It also finds three changes the log comparison missed:
    the error LED moving from 17 to 14, sense P1/P2 being swapped, and the GPIO13/14/25 behaviour.
  - zeus86 found the port shields are tied to ground, so wired/wireless auto-switching does not work as it does on official HW2.
  - One user (bdavez) bricked a unit by flashing the HW1 build. They recovered it by OTA-flashing the vendor Dreamcast firmware through blueretro.io,
    then running a factory reset from the vendor app. Their reset/pair button still didn't work afterwards.
  - The vendor firmware can be OTA-flashed through blueretro.io. The vendor's own update tool reportedly failed.
- The vendor's product page (e.g. [Handheld Legend](https://handheldlegend.com/products/sega-dc-internal-bluetooth-wireless-controller-adapter)) warns that
  installing BlueRetro firmware "will damage this product". The button timings it lists match this disassembly:
  0–3 s "console reset (all four LEDs solid)", 4–6 s disconnect, 7–9 s pairing, 10 s+ factory reset.

## How the comparison was done

1. I parsed the ESP32 app image (7 segments, entry point `0x4008159c`, 8 MB / 80 MHz / DIO flash header).
2. I matched the strings against upstream history. The base is **BlueRetro v1.9 (`be6b1bd`, 2023-10-15)**.
   - The firmware still has `rtype: %ld dtype: %ld sub: %ld`, which was removed on Oct 24.
   - It has the OUYA quirk from Oct 7 and the `# BT_ATT_OP_*` logs that were removed on Oct 22.
   - It does not have the fw_name or keepalive code from November.
3. I rebuilt upstream v1.9 for `hw2/dreamcast` with IDF v5.1.1, using the pre-5.2 `startup.c`, `FREERTOS_HZ=1000` and 8 MB / 80 MHz flash.
4. I matched every function in the rebuilt ELF against the vendor image, with PC-relative literals and calls masked.
   - **2,229 of 2,554 functions are byte-identical.**
   - The rest are either alignment noise, logging changes, or the hardware changes listed below.
5. I compared the data tables. Only `led_list`, `sense_list`, `sw_thresholds` and the `power_off_pin`/`current_pulse_led` initial values differ.

Unchanged:
- **The Maple bus driver.** `maple_tx`, `maple_rx`, `maple_init` and `maple_port_cfg` are identical.
- **The Maple pin tables.** Ports are 21/22, 3/5, 18/23 and 26/27.
- **The DC adapter code**, the config format, OTA and SPIFFS.

## Hardware differences (all in `main/system/manager.c` and `main/system/led.c`)

| Function | BlueRetro HW2 | iBlueControlMod |
|---|---|---|
| Boot / pair button | GPIO0 | **GPIO16** (input, pull-up) |
| Error LED PWM (LEDC HS ch0) | GPIO17 | **GPIO14** |
| GPIO17 | error LED | **output held low**, never toggled (`power_off_pin`) |
| POWER_OFF pin | GPIO16 | GPIO17 (never pulsed) |
| GPIO13 | POWER_ON (20 ms pulse) | "power on" drives it high and holds it. It also gets LEDC LED patterns, so it is probably a status LED |
| GPIO14 | RESET (open drain, 500 ms low) | still pulsed low for 500 ms on a short press. It also gets the error-LED PWM and LED patterns. **Unclear**: the vendor docs call a short press "console reset", so GPIO14 might also reach the reset line. Measure it |
| Port LEDs P1..P4 | 2, 4, 12, 15 | **15, 4, 2, 12** |
| Port sense P1..P4 | 35, 36, 32, 33 | **36, 35**, 32, 33 (P1/P2 swapped) |
| External/internal detect | samples GPIO13+14 | **removed**: always internal, power sense on GPIO39 |
| Button thresholds | 1 s / 3 s / 6 s | **3 s / 6 s / 10 s** |
| Boot-time button check | immediate, only if power off | **1 s delay**, no power check |
| Deep sleep | plain | **GPIO25 and GPIO15 driven low + `gpio_hold_en` + `gpio_deep_sleep_hold_en`** |
| `err_led_set/clear/pulse` | drive the error LED | **no-ops**. The status LED is driven from the system manager instead |
| GPIO25 | unused | driven high by the button LED pattern and latched low in deep sleep (function unknown) |

The vendor added two new LED helpers. Both are reproduced byte-for-byte in the fork:
- `iblue_status_led_pairing()` (vendor `0x40096290`): GPIO14 gets HS ch0 inverted (breathing), and GPIO13 gets LS ch1.
- `iblue_status_led_connected()` (vendor `0x400962b8`): GPIO13 is driven as a GPIO low, and GPIO14 gets LS ch1.

Software-only differences that are not needed for compatibility:
- Many `printf` calls became `ESP_LOGI`, plus some extra debug logs.
- The supervision-timeout `printf` was removed.
- The build sets `CONFIG_FREERTOS_HZ=1000`.

### Why stock BlueRetro misbehaves on this board

- Stock firmware drives **PWM on GPIO17**, which the vendor keeps held low.
- It drives **GPIO16 as a push-pull output**, but GPIO16 is the button on this board.
- It reads the button on GPIO0. On this board GPIO0 is only the boot-mode pin on the debug header.
- It treats GPIO13/14 as console power/reset and may mark the adapter "external".
- It maps sense P1/P2 and port LEDs to the wrong pins.

## The fork

The patch is gated behind a new Kconfig option, `CONFIG_BLUERETRO_HW_IBLUECONTROLMOD`, and a build config `configs/hw2/dreamcast_iblue`. It sits on top of final upstream BlueRetro (`e1a9831`, Dec 2025); use IDF `030c995` (v5.5-dev), which BlueRetroRoot pins.

- Upstream added `hw_config`. The fork sets `power_pin_is_hold=1`, `hotplug=1`, `sw_io0_hold_thres_ms={3000,6000,10000}`, and forces `external_adapter=0`.
- (The vendor's own v1.9 base was used to find the pin changes: rebuilt with the patch, 2,266 functions match the vendor image byte-for-byte. No v1.9 build is kept or shipped.)

Build:

```sh
cp configs/hw2/dreamcast_iblue sdkconfig
BR_HW= BR_SYS=_dreamcast_iblue idf.py reconfigure build
# Leave BR_HW empty: blueretro.io's OTA page refuses an image whose header says
# "hw2" when the running firmware (the vendor one) doesn't, and vice versa.
```

## Flashing without bricking

**Back up first.** The board has a 6-pin debug header next to the antenna. Pinout per zeus86 in discussion #1183, given as module pin / GPIO:

| Header pin | Signal |
|---|---|
| 1 | VCC 3V3 |
| 2 | module pin 25 = GPIO0 (boot mode) |
| 3 | EN |
| 4 | module pin 34 = GPIO3 / RXD0 |
| 5 | module pin 35 = GPIO1 / TXD0 |
| 6 | GND (square pad) |

Wiring: adapter RX to header pin 5, adapter TX to header pin 4, and GPIO0 to GND to enter download mode. Read the whole flash before anything else:

```sh
esptool.py -p PORT -b 921600 read_flash 0 0x800000 iblue_full_backup.bin
```

That backup holds the vendor bootloader, the partition table and the factory app, so it restores everything.
Check your partition table with `esptool.py read_flash 0x8000 0xc00 pt.bin` and then `gen_esp32part.py pt.bin`.

**Prefer OTA through the BlueRetro web config.** OTA only writes the next `ota_x` slot. The vendor app stays in `factory` and the bootloader is never touched.

**Factory reset recovers from a bad OTA app**, but only if that app boots far enough to run the system manager with the button on GPIO16. The fork keeps this path. Holding the button erases `otadata`, so the bootloader falls back to whatever is in the `factory` partition.
That is most likely the vendor app, but confirm it from your backup:
- Hold the button for 30 s. Upstream only wipes `otadata` after 30 s; between 10 and 30 s it only resets the config.
  A factory reset sent over Bluetooth from the web config also only resets the config.
- If the app doesn't boot at all, use UART through the debug header, or OTA from blueretro.io if Bluetooth still comes up (that is how bdavez recovered).

**Keep the image small and the flash header compatible.** The app must fit the OTA slot. Upstream slots are 1 MB; the fork images are about 607 KB to 620 KB. Keep the 8 MB flash-size header the vendor used, so the partition table is never seen as larger than the flash.

**Do not use `idf.py flash` blindly.** It also writes the bootloader, partition table and `ota_data_initial`. Over UART, write only the app to `ota_0`:

```sh
esptool.py write_flash 0x110000 app.bin   # only if your partition table matches upstream (check the backup first)
```

Then select it with otatool. Or restore your full backup.

**Check on the hardware first.** The pin roles come from code. Confirm them before relying on them, especially that GPIO17 is safe to hold low, what GPIO25 does, and whether GPIO14 reaches the console reset line.
The fork copies the vendor's behaviour on all of these pins exactly, so it does nothing the vendor firmware doesn't already do.

## Optional features (on by default in `configs/hw2/dreamcast_iblue`)

Use `configs/hw2/dreamcast_iblue_vendor` for the build that is byte-identical to the vendor-replica fork (all of these off).

| Option | Default | What it does |
|---|---|---|
| `CONFIG_BLUERETRO_IBLUE_RGB_LED` | y | PWM colour driver for the common-anode RGB LED (R=GPIO25, G=GPIO14, B=GPIO13, configurable). One colour: solid when on, breathing while pairing, blinking 2/4/8 Hz through the 3/6/10 s button stages. GPIO13/14 are no longer driven as "power"/"reset". |
| `CONFIG_BLUERETRO_IBLUE_LED_ORANGE` / `_BLUE` | orange | Default colour. **Double-tap** the button to toggle orange/blue; the choice is saved in NVS (namespace `iblue`, key `led_col`). |
| `CONFIG_BLUERETRO_IBLUE_PAIR_WINDOW_S` | 60 | Pairing (BT inquiry, blinking LEDs) only for this many seconds after boot/console power-on, or after a **short tap** (which now (re)opens the window instead of the vendor's no-op "reset" pulse). Paired controllers can reconnect any time; BLE reconnect uses passive scanning. 0 = unlimited (upstream behaviour). |
| `CONFIG_BLUERETRO_IBLUE_DC_MENU_COLORS` | y | PS4/PS5 light bar per port: A orange, B green, C blue, D magenta. |
