# Bluetooth LE configuration commands: the reserved numbers

The adapter's config service (`56830f56-5180-fab0-314b-2fa176799a00`, command characteristic `...9a07`) takes one command byte, optionally data, and answers by reading the characteristic back. BlueRetro uses some numbers itself; the iBlueControlMod additions live in **0x40 to 0x59**. **This file is the only table.** Change it first, then the code, and give every new command a number from here, never one that another branch or another repository already assumes.

Why this exists: two branches and one other repository each picked `0x4B` (Joy-Con motion settings; "port status" in DC-DreamDash's iBlue module) and both used feature bit 3 (LED / animation settings; port status). Neither was on `unified` yet, so the clash was settled here before the code was ported.

## Commands

| Number | Name | In this build | Where else it exists |
|---|---|---|---|
| 0x01-0x07 | BlueRetro: ABI version, firmware version, address, game id, config source, file, firmware name | yes | upstream |
| 0x10-0x14 | BlueRetro: default config, game id config, open/close dir, delete file | yes | upstream |
| 0x37-0x39 | BlueRetro: deep sleep, reset, factory reset | yes | upstream |
| **0x40** | `GET_IBLUE_INFO` (`iBLU`, version, features, counts) | yes | |
| **0x41-0x42** | `GET_GESTURES` / `SET_GESTURES` (tap actions) | yes | |
| **0x43-0x46** | `GET_PROFILE`, `PROFILE_SAVE`, `PROFILE_LOAD`, `PROFILE_DELETE` | yes | |
| **0x47-0x4A** | LED / boot animation settings: get, set, play now, defaults | no, **reserved** | branch `claude/integration` |
| **0x4B-0x4F** | Joy-Con motion (maracas): get settings, set settings, pose, status, forget calibration | no, **reserved** | branch `claude/integration` |
| **0x50** | `GET_PORT_STATUS` (read only; answer below) | **written** (with `CONFIG_BLUERETRO_IBLUE_DCAPI`), not built with ESP-IDF | DC-DreamDash `claude/iblue-ports` used `0x4B` for it: must use `0x50` |
| **0x51** | `API`: the DC API text tunnel. Write `0x51`, a flags byte (bit 0 = first chunk, bit 1 = last chunk), then up to 20 bytes of request text; after the last chunk read: byte 0 = 1 when more of the answer follows (read again), 0 = last chunk, then up to 22 bytes of answer text. See [dcapi.md](dcapi.md) | **written** (`CONFIG_BLUERETRO_IBLUE_DCAPI`), host-tested engine, not built with ESP-IDF | |
| **0x52-0x59** | more port / status commands | **reserved** | |
| 0x5A | BlueRetro: OTA end | yes | upstream |
| 0xA5, 0xDE | BlueRetro: OTA start, OTA abort | yes | upstream |

Numbers not listed (0x08-0x0F, 0x15-0x36, 0x3A-0x3F, 0x5B-0xA4, 0xA6-0xDD, 0xDF-0xFF) are BlueRetro's or free; check upstream before using any.

## Feature bits (byte 5 of the `GET_IBLUE_INFO` answer)

| Bit | Value | Meaning | In this build |
|---|---|---|---|
| 0 | 0x01 | tap gestures | yes |
| 1 | 0x02 | profiles | yes |
| 2 | 0x04 | Dreamcast game id from openMenu (Maple command `0x21`) | yes (with `BLUERETRO_IBLUE_GAMEID`) |
| 3 | 0x08 | LED / animation settings (0x47-0x4A) | reserved (`claude/integration`) |
| 4 | 0x10 | Joy-Con motion (0x4B-0x4F) | reserved (`claude/integration`) |
| **5** | **0x20** | **port status (0x50)** | written, not built |
| **6** | 0x40 | the DC API tunnel (0x51) | written, not built |
| 7 | 0x80 | reserved | |

A client must check the bit before using a command and must ignore bits it does not know.

## `GET_PORT_STATUS` (0x50), as DC-DreamDash's module expects it

Read only. Answer: `[count, then per port: state, dev_mode, acc_mode, bt type, bt subtype]`, five bytes per port. `state`: 0 empty, 1 Bluetooth (a controller with finished HID setup is mapped to the port), 2 wired (the port's sense line is pulled to ground, the same test the firmware uses to skip that port when it hands out Bluetooth slots). `dev_mode` and `acc_mode` are the configured output mode and accessory mode (BlueRetro's `out_cfg`). **Written, not built or tried**: the state comes from a snapshot that `manager.c` refreshes (see [dcapi.md](dcapi.md)). The iBlue is a HW2 build, so the port sense inputs are read like on BlueRetro's own HW2 hardware (`sense_port_is_empty()` in `manager.c`).

## What the other repository has to change when its module is ported

DC-DreamDash's iBlue module (branch `claude/iblue-ports`, `modules/ibluecontrol/netswitch_iblue_proto.py`) has to be ported with two edits: `CMD_GET_PORT_STATUS = 0x4B` becomes `0x50`, and the port-status feature test `f & 8` becomes `f & 0x20` (and its simulated adapter and tests follow). Its docs speak of firmware branch `claude/port-status`, which does not exist.
