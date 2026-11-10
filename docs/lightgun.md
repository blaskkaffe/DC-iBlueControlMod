# Light gun from controller motion: is it possible? (checked 2026-11-10, nothing written)

## How the Dreamcast reads a gun

The position is not data. The gun is a normal Maple device for its buttons (function 0x80). For the position the Dreamcast sends
a special start pattern (8 pulses on SDCKB instead of 4) and then watches the port for the rest of the picture: the gun pulls
SDCKB low at the moment its lens sees the beam of the CRT, and the video chip stores where the beam was. No pulse = "off
screen" (reload). Sources: dreamcast.wiki "Maple bus" (Light Gun CRT Beam Detection Sequence), mc.pp.se "Maple Bus" (GUN bit).

## What an adapter would have to do

Pull SDCKB low at the exact time the picture is at the aimed point, without seeing the picture:

- The only time reference on the controller port is the gun pattern itself. Games normally send it at a fixed point of the
  picture (the Maple transfer started by the vertical blank), so "pattern + delay" can work. This is an assumption per game.
- Down / up is easy: one line is about 64 microseconds.
- Left / right is hard: a pixel is about 0.07 microseconds wide. The adapter's Maple code runs on its own processor core and can
  time to roughly 0.1 to 0.2 microseconds, so a few pixels of jitter at best, more if the game's pattern is not steady.
- The delay differs for 480i, 240p, PAL and VGA, so it needs a calibration screen per video mode (shoot two corners).

Verdict: **possible in principle, unproven**. It needs an oscilloscope on a real console to see when games send the pattern and
how steady it is, before any firmware is worth writing. An earlier hobby attempt (benryves' DC-LiGuE) got the pulse working but
did not become usable.

## Where the aim would come from

| Controller | What it has | Usable as a gun |
|---|---|---|
| Joy-Con, Switch Pro | accelerometer + gyro (already read for the maracas) | yes: gyro aiming with a "centre" button; drifts, re-centre often |
| DualShock 4, DualSense | accelerometer + gyro in every report (not read by the firmware yet) | yes, same as above |
| DualShock 3 | accelerometer, one gyro axis | poor |
| Wii Remote | accelerometer; camera for the sensor bar | the best candidate: the camera gives a real pointing position, no drift (not read by the firmware yet) |
| Wii Remote + MotionPlus | adds a gyro | yes |

Gyro aiming is relative: it is a cursor moved by the hand, not "where the barrel points", and it needs a visible crosshair, which
most Dreamcast gun games show only in calibration. The Wii Remote with a sensor bar is the only one that aims absolutely.

## Order of work, if wanted

1. Measure on a console (House of the Dead 2, Confidential Mission): when the gun pattern comes, how steady, per video mode.
2. Firmware: gun device (function 0x80), the timed pulse, a calibration.
3. Aim sources: Wii Remote camera first, then gyro for the others.
