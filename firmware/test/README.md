# Test firmware

`BlueRetro_iBlueControlMod_test_<commit>.bin`: hw2 `dreamcast_iblue`, built without a hardware tag so blueretro.io / the hosted web config OTA accepts it.
Baseline (untouched): `../BlueRetro_iBlueControlMod_RGB_v1.bin`.

Changes: port LEDs never blink without a Bluetooth controller; power LED blinks while connecting; orange/blue/custom colour
(double tap = next, hold on second tap to sweep and lock custom); fast fade-in, max 80 %; drop boot animation; Dreamcast game id
from openMenu; single tap and triple tap do nothing by default (mappable in the hosted web config, which also holds 4 profiles).

Not tested on hardware. Details: `docs/led-and-gameid.md`, `docs/TODO.md`.
