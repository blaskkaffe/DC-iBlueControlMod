# Prebuilt firmware

| File | Branch / commit | Status |
|---|---|---|
| `BlueRetro_iBlueControlMod_RGB_v1.bin` | `main` @ e2fad8e (build config `configs/hw2/dreamcast_iblue`) | **Tested on real hardware** |
| `test/BlueRetro_iBlueControlMod_unified_test_0c440c8.bin` | `unified` @ 0c440c8 | **Flashed on hardware (2026-11-19):** boots, pads pair and play, rumble and VMU work; Joy-Con motion (two Joy-Cons as the maracas of one port) does not work yet. See [test/README.md](test/README.md) |

Contents: final upstream BlueRetro with the iBlueControlMod pin map, an orange LED by default (RGB 255/22/0, double-tap for blue), a 60 s pairing window and PS4/PS5 light bars in the Dreamcast menu colours.

**Install:** open [blueretro.io](https://blueretro.io) in Chrome or Edge, connect to the adapter, and upload the `.bin` on the OTA page. Hold the button for 30 s to go back to the vendor firmware.

The version string inside the image is `v25.10-beta-11-g66b5582-dirty`. That's the working tree it was built from, and its code is identical to commit e2fad8e. Check the download against `SHA256SUMS.txt`.
