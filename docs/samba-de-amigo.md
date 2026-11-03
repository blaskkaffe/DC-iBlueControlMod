# Samba de Amigo maracas: concept

Goal: play Samba de Amigo with two Wii remotes or two Joy-Cons (or one DualShock / DualSense) instead of the real maracas, by sensing the angle and shaking of each controller and reporting it to the Dreamcast as a maracas controller. Status: **research and concept only, nothing is built.** Written 2026-11-01.

## What the Dreamcast sees (facts, with sources)

The maracas are an ordinary Dreamcast **controller function** (FT0) device with a different capability word and name. From Flycast's emulation of them (`core/hw/maple/maple_devs.cpp`, `maple_maracas_controller`), read 2026-11-01:

| | Value |
|---|---|
| Name | "Maracas Controller" |
| Capability word | `0x0f093c00` as Flycast stores it, the same form as the standard pad's `0xfe060f00`; on the wire (the form `maple.c` uses) that is **`0x003C090F`** (derived by byte reversal, not seen on a real unit) |
| Analog axes | four: axes 2 to 5, the same slots as the standard pad's stick X, Y and a second stick X, Y (in this firmware: `AXIS_LX`, `AXIS_LY`, `AXIS_RX`, `AXIS_RY`) |
| Buttons advertised | C, B, A, Start, Z, D (no d-pad, no X, Y) |
| Current | 100 mA standby, 130 mA max |

What the buttons and axes mean, from dreamcast.wiki's Maracas page (left maraca / right maraca):

| | Left | Right |
|---|---|---|
| Position | analog axes of the left maraca | analog axes of the right maraca |
| Shake | B | A |
| Button | C | Start |
| "Connection loss" signal | D | Z |

Flycast gives each maraca two axes (left maraca = axes 2 and 3, right = 4 and 5) and feeds them from the two thumbsticks. After a reset the real maracas run an ultrasonic timing adjustment, 4, 5, 6 or 7 s on port A, B, C or D, during which buttons are off, positions fixed and the connection loss signal is on. The game also sends a SetCondition with the ultrasonic settings (an analog update interval of 2 for Samba de Amigo); Flycast does not implement it and the game still runs.

The real maracas measure position with an ultrasonic receiver (the "sensor bar" on the floor or the TV) and detect shakes with a sensor in each maraca. Real units report **position (about three heights: high, middle, low) and a shake button**; the game needs nothing else.

**Not known yet** (needs a capture from real maracas, or someone who has one): which of each maraca's two axes carries the height, the value range of high, middle and low, whether the game cares about the connection loss signal being off from the start, and whether it checks the name or the capability word.

## What the adapter has to do

1. **Output mode "Maracas"**: capability word `0x003C090F`, name "Maracas Controller", the condition packet of the pad with the four axes (the same as GamePadAlt's sticks) and only the buttons C, B, A, Start, Z, D. A new `dev_mode` next to Racing in `maple.c` and `dc.c`; selectable on the web page and per game through the game id. Small.
2. **Connection loss**: report D and Z released from the start (Flycast does), or hold them for the 4 to 7 s the real ones do if a test shows the game wants it.
3. **Input**: turn the motion of each controller into the two axes and the shake button of one maraca. This is the new work, see below.

## Where the motion comes from (what BlueRetro reads today)

| Controller | Motion in the report | Today in BlueRetro |
|---|---|---|
| Wii remote | accelerometer (3 axes) in the "core + accelerometer" reports (0x31, 0x35, 0x37) | reports are defined in `wii.h`; the data is not used |
| Joy-Con, Switch Pro | accelerometer and gyro after the "enable IMU" subcommand (0x40) | the subcommand is in `sw.c` behind `SW_ENABLE_IMU_EN`, off; the data is not parsed |
| DualShock 4, DualSense | gyro and accelerometer in every input report | not parsed |

Two controllers as two maracas: BlueRetro already runs several Bluetooth controllers at once and maps each to an output port; here both would feed the **same** output (the maracas), one per hand, so the mapping needs a "controller 1 = left maraca, controller 2 = right maraca" rule. One controller alone (a DualSense held in one hand per maraca is not possible) can only be both maracas by using its two sticks, which is the Flycast fallback.

## Turning motion into the maraca (design)

- **Height from the angle**: the gravity vector from the accelerometer gives the tilt (pitch). Pointing up is high, level is middle, pointing down is low. The three bands have a dead zone between them so the value does not flicker; the axis is set to a fixed value per band (or a smooth value of the pitch if the game accepts it), to be found by test.
- **Shake**: the size of the change in acceleration crossing a threshold sets the shake button (B left, A right) for a short, fixed time (about 50 to 80 ms), with a short lockout so one shake is one press. The threshold and the hold time are settings.
- **Latency**: a rhythm game is sensitive to it. Bluetooth reports come at about 100 Hz (Wii remote), 60 Hz (Joy-Con in the default mode) or 250 Hz (DualSense); a shake can be seen within one or two reports (10 to 30 ms). The existing adapter latency adds to that. The shake press should be sent in the next Maple poll after it is seen.
- **Settings on the web page**: which controller is left and right (the first to connect is left unless swapped), tilt bands, shake threshold, press time, axis values per band.

## Joy-Cons

Joy-Cons are the best source: each has a 3-axis accelerometer and a gyro, and two of them are one maraca each. BlueRetro already asks them for the full input report (0x30), which has room for the IMU samples (3 per report, 6 values each), but the subcommand that switches the IMU on is compiled out (`SW_ENABLE_IMU_EN` in `sw.c`) and the samples are not parsed. See "Phases" 2 and 3.

## Phases

1. **(Built, untried with the game.) Output mode Maracas with sticks** (Flycast's way): the descriptor, name, buttons, and a preset mapping the left stick to the left maraca and the right stick to the right, B and A on two buttons. No motion. Small; shows what the game accepts and answers the open questions (which axis is the height, the value range, the connection loss signal). **Do this first.**
2. **One motion source**: parse the DualSense or Joy-Con motion (the simpler report), tilt to height and shake to button, one controller as one maraca; tune on the real game.
3. **Two controllers, one output**: the "left and right" rule, then the Wii remote.
4. **Polish**: settings on the web page, per-game selection through the game id, a preset on the presets page.

## Risks

- The axis meaning and range are from an emulator, not a real unit: phase 1 exists to find out cheaply. A capture of a real unit (a Maple bus logger or the MaplePad / dreamwave tools) would settle it.
- Tilt gives a height but not a real position: the game was written for ultrasonic distance. It may want a rise and fall, not a held value.
- Sensor noise and drift (gyro) need filtering; the accelerometer alone is enough for tilt.
- BlueRetro's per-controller output model: two inputs into one output is new; it touches the input mapping code.
