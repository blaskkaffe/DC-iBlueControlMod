# brctl: control BlueRetro from Dreamcast software

A tiny KallistiOS library for talking to the BlueRetro adapter (the iBlueControlMod fork, `iblue-latest`, with `CONFIG_BLUERETRO_BRCTL=y`) over the Dreamcast controller port. No extra wiring is needed.

From openMenu or any KOS program you can:

- **Read** the adapter info and live status: firmware version, which port has which Bluetooth controller, pairing state and LED colour.
- **Read or write the full BlueRetro config.** This is the same struct blueretro.io edits: system, per-port device and accessory modes, and button mappings.
- **Control the LED:** set any RGB colour, pick a preset, or save a colour as custom.
- **Control Bluetooth:** start or stop pairing, or disconnect all controllers.
- **Profiles and per-game settings** (protocol 3): `PROFILES` / `GAME` items and `BRCTL_OP_PROFILE_*`, `BRCTL_OP_GAME_*`; see `docs/iblue-extras.md`.
- **Remap the button taps:** pick an action for the single, double and triple tap (`brctl_btn_set`).
- **Save or reset the config, and restart the adapter.**

## Files

| File | Purpose |
|---|---|
| `brctl.h` / `brctl.c` | The library (drop both into your project) |
| `brctl_proto.h` | Wire protocol, shared with the firmware (same file as `main/iblue/brctl_proto.h`; `sh main/tests/host/run.sh` checks) |
| `brctl_config.h` | BlueRetro config struct + enums (`struct br_config`, 12356 bytes) |
| `example/` | `brctl_test`: on-screen status, A/B/X/Y/START to exercise every call |

## Minimal use

```c
#include "brctl.h"

int port = brctl_find();          /* 0..3 = A..D, -1 if none */
if (port >= 0) {
    brctl_led_rgb(port, 0xFF, 0x16, 0x00, 1);   /* set + save as custom colour */
    brctl_pair_start(port);                      /* 60 s pairing window */

    static struct br_config cfg;
    if (brctl_config_load(port, &cfg) == BRCTL_OK) {
        cfg.out_cfg[0].acc_mode = BR_ACC_BOTH;   /* VMU + rumble on port A */
        brctl_config_save(port, &cfg);           /* writes + applies + persists */
    }
}
```

`brctl_op_wait()` sends an operation and waits until the adapter has finished it (`status.op_seq`), returning its `BRCTL_RES_*`: use it for anything that touches flash.

Every call returns `BRCTL_OK` (0) or a negative `BRCTL_E*` code, and blocks for at most about 100 ms per frame, with 3 retries.

## Integrating into openMenu

1. **Add the files.** Copy `brctl.c`, `brctl.h`, `brctl_proto.h` and `brctl_config.h` into the source tree and add `brctl.o` to the objects.
2. **Find the adapter.** On entering a "BlueRetro" settings screen, call `brctl_find()`. If it returns -1, show "Connect a Bluetooth controller to configure BlueRetro".
3. **Read state.** Read `brctl_get_status()` for the per-port view and `brctl_config_load()` for the settings.
4. **Save changes.** Edit the struct, then call `brctl_config_save()`. LED and pairing actions are immediate calls.
5. **Don't poll in a tight loop.** Every 250 ms or more is plenty; the adapter refreshes its status every 100 ms.

## Limitations to know

- **Bluetooth controller needed on the iBlue.** The channel only exists on a port where BlueRetro currently shows a controller. With hotplug on (the iBlue default) a port only appears once a Bluetooth pad is connected there. Plain HW1/HW2 boards have hotplug off and always answer.
- **No frame-loss repair.** Frames hit by bus bit-loss are ignored by the adapter. The library times out and retries.
- **Untested on hardware.** The firmware side has only been tested in an emulator and the library has only been compile-checked. Try the example before relying on it.

## Protocol summary

Requests go to the controller main unit, with private function code `BRCTL_FUNC` as the first payload word.

| Maple command | Payload | Reply |
|---|---|---|
| BLOCK_READ (11) | func, param, nbytes | DATA_TX: func, param, data |
| BLOCK_WRITE (12) | func, param, data… | ACK (7), or FILE_ERROR (0xFB) |
| SET_CONDITION (14) | func, op, arg0, arg1, arg2 | ACK (7) |

- **`param`:** `(item << 24) | byte_offset`. Items are `INFO`, `STATUS` and `CONFIG`.
- **Transfer size:** at most 512 bytes per transfer.
- **Byte order:** the Maple bus sends each 32-bit word most-significant byte first, so the library byte-swaps every word. That lets both sides use the structs in `brctl_proto.h` and `brctl_config.h` as-is.

## Building the example

```sh
source /opt/toolchains/dc/kos/environ.sh
cd example && make
# run with dcload (make run), or wrap the .elf in a CDI with mkdcdisc for GDEMU
```
