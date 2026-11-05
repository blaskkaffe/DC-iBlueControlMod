# DC API (`dc1`) in the iBlueControlMod firmware

The adapter's end of the control language shared by the DC- projects. The design and the decisions are in the repository [DC-UnifiedEcosystem](https://github.com/blaskkaffe/DC-UnifiedEcosystem) (`docs/04-control-channel.md`); this page is what the firmware builds.

**Status: written. The engine (`main/system/iblue_dcapi.c`) is host-tested (`sh main/tests/host/run.sh`, 82 checks, sanitizers on). The Maple and BLE glue and the Kconfig option are not built with ESP-IDF and nothing has run on an adapter or a console.** Option: `CONFIG_BLUERETRO_IBLUE_DCAPI` (default on; needs the game id option).

## What it answers

A request is lines of text: `dc1`, an optional `id N`, an optional `to <who>` (this device answers for none, `self`, `iblue`, `iBlueControlMod`), then `get <key>`, `get <prefix>*`, `set <key> <value>`, `call <action>`. The answer is `dc1`, the echoed `id`, and one `ok <key> <value>` / `err <key> <code> [why]` line per key. Unknown keys and verbs give their own `err ... unknown` line and nothing else is spoiled.

| Key | |
|---|---|
| `hello.kind` `hello.name` `hello.version` `hello.api` `hello.caps` | `adapter`, `iBlueControlMod`, the firmware version, `1`, `api,net,port,game` |
| `game.id` | read only: the game id from openMenu (Maple `0x21`); `err game.id unavailable no game` when none |
| `net.selected` | `dcnow` or `dcnet`; `set` by the Pi (over BLE) or openMenu (over Maple), read back; not stored; `err ... unavailable not set` until someone sets it. Nothing in the firmware uses it yet (the LED colour by network, IB-P2, will) |
| `port.<A-D>.state` | `empty`, `bt` (a Bluetooth controller with finished HID setup is mapped to the port) or `wired` (the port's sense input says something is plugged in; a physical controller wins over a Bluetooth one). The adapter never learns **what** the wired device is, that is for openMenu to find out |
| `port.<A-D>.mode` `.acc` | the output mode (`normal`, `dual`, `keyboard`, `mouse`) and accessory mode (`none`, `vmu`, `rumble`, `both`). **`set` works**: it is applied at once, and because a console only looks at what is on a port when it enumerates it, the wired side restarts (the console sees the controllers leave and come back, like a hot-plug). Not stored until `call sys.commit` |
| `port.<A-D>.name` | read only: the product name the console reads in the device info (`Dreamcast Controller` for normal and dual stick, `Keyboard`, `Dreamcast Mouse`) |
| `port.<A-D>.modes` `.accs` | read only: the values `set` accepts on this device (`normal,dual,keyboard,mouse` and `none,vmu,rumble,both`). Ports the board does not have answer `unavailable no such port` |
| `sys.store` | `commit`: changes are live and stored by `call sys.commit` (a device that stores every change itself says `auto`) |
| `sys.unsaved` | `yes` while a live change waits to be stored, else `no`. `call sys.commit` stores it (`config_update`, as the web config does), `call sys.revert` loads the stored configuration again; both answer `done` or `nothing` and only act when something is waiting. A new game id loads another stored configuration and also clears the flag |
| `events` | not implemented: `err events unavailable not implemented` |

`led.*`, `pair.*` of the design are not here yet; `port.<X>.layout`, `port.<X>.layouts` and `port.<X>.map.<OUTPUT>` are in (see below); the capability bits say what is: `api` (bit 0), `net` (bit 1), `port` (bit 3), `game` (bit 5) = `0x2B`. Bit 4 was `profile`: profiles are not part of BlueRetro and were removed, the bit is reserved.

**How a change is applied:** the Maple interrupt cannot touch files or restart the wired side, so the engine only queues a message (`FB_TYPE_API_CFG`, three bytes, like the game id); `host.c` hands it to `iblue_dcapi_cfg_apply()` (`system/iblue_dcapi_cfg.c`) in the Bluetooth task, which writes `config.out_cfg[]`, and for a mode change asks the system manager for `SYS_MGR_CMD_WIRED_RST`. The answer to the `set` does not wait for that; the engine's own copy of the value changes at once so reading it back shows it.

The ports are read from a snapshot (`iblue_dcapi_port_set()`) that `wired_port_hdl()` in `manager.c` refreshes whenever it maps Bluetooth controllers to ports, because the Maple interrupt that answers cannot call the Bluetooth code. So a change shows up when that function next runs.

## Over Maple (command `0x22`)

Sent to the **memory card unit** of the port (the same unit that takes the game id), payload: the function code of the memory card (`0x00000002`), then the request text. Like all text on the bus it is **word-reversed** (every 4 bytes) and NUL padded to whole words, at most 252 bytes. The adapter answers in the same exchange with a data frame (`0x08`): the function code, then the answer text the same way (at most 400 bytes, cut at a line end). A request with another function code or no text is acknowledged only.

Discovery costs nothing extra: the extended device info (`0x02`) carries the marker `iBlueControlMod ` (16 bytes, for the game id) and, in the next 24 bytes, `dc1 ` plus the capability bits as 8 hex digits and NUL padding: `dc1 0000002b`.

The engine runs in the Maple interrupt: all its text is in RAM (`DRAM_ATTR`), it uses nothing from libc but `memcpy`/`memset`, and its buffers are static. **Risk to check on a console:** the time the answer takes (it is parsing a few lines, then the same kind of copy a VMU block read does), and that the BIOS and games still enumerate the VMU normally.

## Over BLE

Command `0x50` `GET_PORT_STATUS` (info feature bit 5, read only) answers `[count, then per port: state, dev_mode, acc_mode, bt type, bt subtype]` from the same snapshot; it is compiled with the DC API option.

### Command `0x51` (info feature bit 6)

One read answers at most 23 bytes, so the tunnel is chunked: write `0x51`, a flags byte (bit 0 first chunk, bit 1 last chunk) and up to 20 bytes of request text, as many writes as needed; after the last chunk, read: byte 0 is `1` while more of the answer follows (read again) and `0` on the last chunk, then up to 22 bytes of text. Reading past the end gives a single `0`.


## Layouts and the mapping of an output (2026-11-05)

`system/iblue_mapcfg.c` (portable, host-tested with 1032 checks) is the text form of BlueRetro's mapping table; the engine reads and queues changes, it never writes the configuration itself.

| Key | Meaning |
|---|---|
| `port.<X>.layouts` | `standard,all,custom` (read only) |
| `port.<X>.layout` | the preset the port's mapping equals, or `custom`. `set` to `standard` or `all` replaces the pad mapping of the port (keyboard, mouse, combo and other-port entries are kept); `custom` cannot be set |
| `port.<X>.map.<OUTPUT>` | the inputs that drive a Dreamcast output: `lb`, `rb, lt:threshold=78`, `none`. `get port.A.map.A1*` lists by prefix. `set` replaces the inputs of that output |

Outputs (upper case): `A B X Y START D Z C LT RT A1_UP A1_DOWN A1_LEFT A1_RIGHT A2_* DPAD_* DPAD2_*`. Inputs (lower case, XInput names): `a b x y lb rb lt rt back start guide share lstick.up|down|left|right|click rstick.* dpad.* dpad2.*`; Pico2Maple's spellings (`A1_UP`, `DPAD_DOWN`, `SELECT`) are read too. Options after a colon: `deadzone=off|1..5|n%`, `threshold=0..100`, `scale=0..255`, `turbo=0..255`, `algo=linear|aggressive|relaxed|wide|scurve|passthrough`. At most 4 inputs per output.

- **Applying:** `set` asks for ONE queued message per output (`IBLUE_CFG_MAP_SET`; the entries wait in a mailbox of 8 slots, so a request can set up to 8 outputs), or `IBLUE_CFG_SET_LAYOUT`; the Bluetooth task does it. A set is all or nothing. If the message cannot be queued (queue full, muted, mailbox taken) the answer is `err ... busy try again` and nothing changed, `sys.unsaved` included; `call sys.commit` / `revert` answer `ok sys.commit busy try again` and `sys.unsaved` stays `yes`. The answer shows what was understood; a `get` in the same request still shows the old line. Not stored until `call sys.commit`; `sys.unsaved` becomes `yes`. `set port.X.map.OUT` with no value is `err ... empty`; `none` clears. A request longer than 252 bytes (Maple or BLE) is refused whole with `err request too long`, never cut and applied. The queue holds 16 messages shared with rumble and LED feedback.
- **`layout` is computed in `manager.c`** (the Maple interrupt does not compare presets); it follows web-config edits on the next pass.
- **BlueRetro's factory default is the `all` layout** (the bumpers are Z and C, Back is D, the second d-pad is DPAD2): tested.
- **`standard`** is the Pico2Maple layout as read from the Microsap reconstruction (right bumper Z, a fully pulled left trigger C, left bumper and right trigger drive RT): **not checked on a Pico2Maple**.
- **Deadzone byte:** values up to 200 keep their old meaning (hundredths of a percent, default 135 = 1.35 %). **201 to 255 now mean 3 to 57 percent** (`value - 198`), so the levels 2 to 5 (5, 10, 15, 20 %) fit the byte. A configuration that had a value above 200 (the hosted web page offers 205, 210 ... in steps of 5) now has a much bigger deadzone. `iblue_dz_x10000()` is the only place that knows; `adapter.c` uses it.
- **Never built with ESP-IDF:** `iblue_dcapi_cfg.c`, `manager.c` and `adapter.c` changes were only syntax-checked against stub headers. The Maple interrupt renders a line (a scan of the mapping table) per `get`: the time it takes inside the interrupt was not measured.
