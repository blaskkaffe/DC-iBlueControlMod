# Controllers: which ones BlueRetro accepts, what they call themselves, and how that compares with Pico2Maple / Microsap

Read from the code on 2026-11-03 (`main/bluetooth/hidp/hidp.c`, `main/adapter/adapter.h`) and the Pico2Maple decompilation. Nothing here was tried on hardware by the author of this doc.

## What BlueRetro (this firmware) accepts

A Bluetooth controller is recognised by the **name it announces** (the Bluetooth remote name); anything else is parsed as a generic HID gamepad, keyboard or mouse. The table is the one in `hidp.c`, with what the adapter calls the device:

| Name the controller announces | BlueRetro type / subtype | In plain words (for openMenu) |
|---|---|---|
| `PLAYSTATION(R)3` | PS3 | DualShock 3 / Sixaxis |
| `Wireless Controller` | PS (default) | DualShock 4 |
| `DualSense Wireless Controller` | PS / PS5 | DualSense (and Edge) |
| `Xbox Wireless Controller` | generic HID | Xbox One S/X, Series |
| `Nintendo RVL-CNT-01` | Wii | Wii remote (the extension, Nunchuck / Classic / Classic Pro, is detected) |
| `Nintendo RVL-CNT-01-UC` | Wii / Wii U Pro | Wii U Pro controller |
| `Pro Controller` | Switch | Switch Pro Controller |
| `Joy-Con (L)`, `Joy-Con (R)` | Switch / left, right Joy-Con | Joy-Con |
| `SNES Controller`, `HVC Controller`, `NES Controller`, `N64 Controller`, `MD/Gen Control Pad` | Switch / SNES, NES, N64, MD | Nintendo Switch Online pads |
| `Hyperkin Pad` | Switch / Admiral | Hyperkin Admiral |
| `DeviceName` | Switch 2 | Switch 2 controller |
| `8BitDo NES30 Arcade`, `8BitDo Arcade Stick`, `8BitDo NGC Modkit`, `8BitDo N30 Modkit`, `8BitDo GBros Adapter`, `8Bitdo N64 GamePad`, `8BitDo N64 Modkit`, `8BitDo NEOGEO GP`, `8BitDo M30 gamepad`, `8BitDo M30 Modkit`, `8BitDo S30 Modkit` | generic HID with a quirk | 8BitDo pads and mod kits |
| `Retro Bit Bluetooth Controller`, `Joy Controller` (RF Warrior), `BlueN64 Gamepad`, `OUYA Game Controller` | generic HID with a quirk | retro and clone pads |
| anything else | generic HID | gamepad, keyboard or mouse by what its HID report says (Steam Controller, Stadia and many others work this way) |

The web page's mapping lists name the same families (PS3, PS4 / PS5, Wiimote variants, Wii U / Switch Pro, Switch NES / SNES / MD / N64 / Joy-Con, Xbox One S / X|S, Steam, 8BitDo SN30 / SF30 and M30, Stadia, Exlene GameCube).

## What openMenu can be told today

The control channel on `claude/restructure` (`brctl`) reports for each port the **type and subtype numbers** above (`port_bt_type`, `port_bt_subtype`), not the name string. openMenu can turn them into the words in the last column. For controllers that fall under "anything else" the number is generic HID and the announced name would be needed; adding the name (about 16 characters per port) to the status is a small change on the adapter side.

## Compared with Pico2Maple (and Microsap)

| | BlueRetro here | Pico2Maple | Microsap |
|---|---|---|---|
| Link | Bluetooth only (the ESP32 has no USB) | USB (wired pads and dongles), no Bluetooth in the USB build | USB and Bluetooth (Bluepad32) |
| DualShock 3 and clones | yes, by name | yes, 56 USB IDs | via Bluetooth |
| DualShock 4, DualSense (+ Edge) | yes | yes (3 drivers) | yes |
| Switch Pro | yes | yes | yes |
| Joy-Con, Switch Online pads (NES, SNES, N64, MD), Hyperkin | **yes** | no | no |
| Wii remote, Nunchuck, Classic, Wii U Pro | **yes** | no | no |
| Xbox | yes (Bluetooth) | yes (XInput, wired) | yes (Bluetooth, verified) |
| Steam Controller | yes (generic HID) | yes, two drivers | via Bluetooth |
| 8BitDo | yes, many models by name | three models | via Bluetooth |
| Other retro, clone, OUYA pads | yes (the quirk list) | a generic table of about 100 HID layouts | generic |
| Logitech G29 steering wheel (the Racing layout) | no | **yes** | no |
| Keyboard, mouse | yes (Bluetooth) | yes (USB boot protocol) | yes |
| Output types | pad, pad alt, keyboard, mouse, racing, maracas (this fork) | pad, pad alt, twin stick, racing, keyboard, mouse | the same |

So the two cover the same mainstream pads (PlayStation, Switch Pro, Xbox, Steam, 8BitDo, generic). BlueRetro adds the Nintendo family (Wii, Joy-Con, Switch Online) and a long list of retro Bluetooth pads; Pico2Maple adds wired USB pads and the G29 wheel.
