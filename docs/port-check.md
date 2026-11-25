# Port check (test build step 10)

A physical controller does not work on port D (2026-11-19 to 11-25, every build from 0c440c8 to step 9), while A, B and C work. Comparing the code found no difference for port D between the vendor firmware, BlueRetro v1.9 and this firmware: the same Maple pins (GPIO26 / GPIO27), sense pin (GPIO33), sense test, port loop and pin driver. The port check measures on the board what the code cannot say.

## Running it

1. Console on, the wired controller in port D (it can also be run with a wired controller in another port to compare).
2. Open the web config of the test builds, `https://blaskkaffe.github.io/DC-iBlueControlMod/test/`, and connect.
3. Device > **Port check** > Run port check (about 2 s), then **Copy report** and send it.

It only reads: the pins are as before when it ends. Firmware side `main/iblue/iblue_portcheck.c`, BLE command `0x52` (feature bit 7), web `panels/device.js`.

## What the report says, per port

| Line | Meaning |
|---|---|
| `ADAPTER ANSWERS HERE` / `adapter quiet` | whether the adapter has the port's Maple pins on (it answers the console there). With a physical controller in the port it must say quiet |
| `sense GPIOnn=x (plugged/empty)` | the sense pin: 0 = something plugged in (low), 1 = empty |
| `m0` / `m1` / `sn` lines | the pin's level (`in`), output enable (`oe`, must be 0 on a quiet port), its IO_MUX register, GPIO pin register, output signal selection, and for the RTC pins (GPIO26, 27, 32, 33) the RTC pad register |
| `traffic 200ms` | edges seen on the two Maple pins in 200 ms (the console polls every port, so a port with a controller shows many) and how much of the time the pin was high |
| `pulls (down,up)` | a quiet port's pins with the internal pull-down, then pull-up: `follow` = nothing drives the line; `HIGH` / `LOW` = something holds it there |
| `maple since boot` | frames the adapter's Maple interrupt received on the port, answers it sent, frames it gave up on |

What to look for on port D compared with C: `oe1` on a quiet port, an RTC pad register with its mux bit set (the pin is then cut off from the digital side), no traffic while C has some, a pin held `LOW`, or answers sent while a controller is plugged in.

**Not tried on hardware** (step 10).
