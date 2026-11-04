# DC API (`dc1`) in the iBlueControlMod firmware

The adapter's end of the control language shared by the DC- projects. The design and the decisions are in the repository [DC-UnifiedEcosystem](https://github.com/blaskkaffe/DC-UnifiedEcosystem) (`docs/04-control-channel.md`); this page is what the firmware builds.

**Status: written. The engine (`main/system/iblue_dcapi.c`) is host-tested (`sh main/tests/host/run.sh`, 81 checks, sanitizers on). The Maple and BLE glue and the Kconfig option are not built with ESP-IDF and nothing has run on an adapter or a console.** Option: `CONFIG_BLUERETRO_IBLUE_DCAPI` (default on; needs the game id option).

## What it answers

A request is lines of text: `dc1`, an optional `id N`, an optional `to <who>` (this device answers for none, `self`, `iblue`, `iBlueControlMod`), then `get <key>`, `get <prefix>*`, `set <key> <value>`, `call <action>`. The answer is `dc1`, the echoed `id`, and one `ok <key> <value>` / `err <key> <code> [why]` line per key. Unknown keys and verbs give their own `err ... unknown` line and nothing else is spoiled.

| Key | |
|---|---|
| `hello.kind` `hello.name` `hello.version` `hello.api` `hello.caps` | `adapter`, `iBlueControlMod`, the firmware version, `1`, `api,net,port,game` |
| `game.id` | read only: the game id from openMenu (Maple `0x21`); `err game.id unavailable no game` when none |
| `net.selected` | `dcnow` or `dcnet`; `set` by the Pi (over BLE) or openMenu (over Maple), read back; not stored; `err ... unavailable not set` until someone sets it. Nothing in the firmware uses it yet (the LED colour by network, IB-P2, will) |
| `port.<A-D>.state` | `empty`, `bt` (a Bluetooth controller with finished HID setup is mapped to the port) or `wired` (the port's sense input says something is plugged in; a physical controller wins over a Bluetooth one). The adapter never learns **what** the wired device is, that is for openMenu to find out |
| `port.<A-D>.mode` `.acc` | the output mode (`normal`, `dual`, `keyboard`, `mouse`) and accessory mode (`none`, `vmu`, `rumble`, `both`). **`set` works**: it is applied at once, and because a console only looks at what is on a port when it enumerates it, the wired side restarts (the console sees the controllers leave and come back, like a hot-plug). Not stored until `call sys.commit` |
| `port.<A-D>.name` | read only: what the Dreamcast sees (`Controller`, `Controller (dual stick)`, `Keyboard`, `Mouse`). Ports the board does not have answer `unavailable no such port` |
| `sys.unsaved` | `yes` while a live change waits to be stored, else `no`. `call sys.commit` stores it (`config_update`, as the web config does), `call sys.revert` loads the stored configuration again; both answer `done` or `nothing` and only act when something is waiting. A new game id loads another stored configuration and also clears the flag |
| `events` | not implemented: `err events unavailable not implemented` |

`profile.*`, `led.*`, `pair.*` of the design are not here yet; the capability bits say what is: `api` (bit 0), `net` (bit 1), `port` (bit 3), `game` (bit 5) = `0x2B`.

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
