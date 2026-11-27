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
| `answers on` / `wants` / `updates` / `held back` / `card busy` (from step 12) | the ports the adapter answers on, the ports the manager wants it to answer on, how many port updates it applied, how many waited for the memory card, and the card's blocks still to store. `answers on` and `wants` differ while an update waits |
| `bt N: port P ready, maps to M` (from step 12) | each Bluetooth controller, the port it is given (4 or more: none, all taken) and the ports its mapping writes to |
| `ADAPTER ANSWERS HERE` / `adapter quiet` | whether the adapter has the port's Maple pins on (it answers the console there). With a physical controller in the port it must say quiet |
| `sense GPIOnn=x (plugged/empty)` | the sense pin: 0 = something plugged in (low), 1 = empty |
| `m0` / `m1` / `sn` lines | the pin's level (`in`), output enable (`oe`, must be 0 on a quiet port), its IO_MUX register, GPIO pin register, output signal selection, and for the RTC pins (GPIO26, 27, 32, 33) the RTC pad register |
| `traffic 200ms` | edges seen on the two Maple pins in 200 ms (the console polls every port, so a port with a controller shows many) and how much of the time the pin was high |
| `pulls (down,up)` | a quiet port's pins with the internal pull-down, then pull-up: `follow` = nothing drives the line; `HIGH` / `LOW` = something holds it there |
| `maple since boot` | frames the adapter's Maple interrupt received on the port, answers it sent, frames it gave up on; from step 12 also the same during the 200 ms (answers there = the adapter talks on that port now) |

**Sniffer (from step 13):** at the end the check listens for 500 ms on every port the adapter does not answer on, with the same decoder that reads the console when it answers, but it never sends: the port's pins are inputs without pulls and the interrupt only logs the frame. Per port it reports the frames from the console and from a controller (or its VMU / rumble), frames broken off mid-way, and the last six frames: `sender>recipient command(code) len words/header length lost bits total bits`. Addresses: the top two bits are the port (00 A, 40 B, 80 C, C0 D); low bits 00 = the console, 20 = the controller, 01 / 02 = its sub-devices (VMU, rumble). A working controller shows `00>20 info?(01)` or `cond?(09)` from the console and `20>00 info(05)` or `data(08)` back. Ports with a working wired controller are the control: if they show controller frames, the sniffer works, and a port that shows only console frames has a controller that never answers.

What to look for on port D compared with C: `oe1` on a quiet port, an RTC pad register with its mux bit set (the pin is then cut off from the digital side), no traffic while C has some, a pin held `LOW`, or answers sent while a controller is plugged in.

The pins, their chip defaults and what the firmware sets: `port-pins.md`.

**Not tried on hardware** (step 10).

## Result on the owner's board (step 11, 2026-11-25)

Wired pads in A and D, no Bluetooth controller: the adapter answers on no port; every Maple pin is input off, output off, plain GPIO output, internal pull-up only; GPIO26's RTC pad register `88000000` (drive 2, pull-up; digital, no DAC, no hold), GPIO27's `4a000000` (drive 2, pull-up; digital, touch off). The console's polling reaches all ports' pins (64 to 84 edges in 200 ms on an empty port). The pad in A answers (318 edges); the pad in D does not (68 / 72, as an empty port), while D's sense reads plugged (held low). The pad gets power, ground and sense on D, but not the data lines: the break is between the iBlue's port D socket pins 1 / 5 and the point where the console's lines and GPIO26 / 27 meet.

Again on step 12 (2026-11-27), wired pads in all four ports, no Bluetooth controller: the adapter answers on no port and wants none (`wants 0000`, no update held back), all four sense pins read plugged. Traffic in 200 ms: A 850 / 882, B 648 / 674, C 378 / 398, **D 24 / 20**: the pads in A to C answer, the pad in D does not. With a Bluetooth controller on D (A to C wired), plugging a pad into D moves the controller off (it shows port A's colour: no port left) and neither works, as expected while D's data lines do not reach the pad.

The web config cannot connect while a Bluetooth controller is connected: BlueRetro (v1.9 and later, unchanged here) stops advertising its configuration service when a controller connects and starts it again when the last one disconnects (`hci.c`, BT_HCI_EVT_CONN_COMPLETE / DISCONN_COMPLETE).
