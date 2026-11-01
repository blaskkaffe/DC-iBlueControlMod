# Notice

This site is a fork of the **BlueRetro web config** (https://github.com/darthcloud/BlueRetroWebCfg, Apache License 2.0,
copyright Jacques Gagnon and the Google Chrome Samples authors it is based on), taken at commit 8b3d59b (2026-05-30).
The license is in `LICENSE`.

Changes in this fork:

- `iblue.html`, `iblue.js`, `utils/iblueCmd.js`: a page for the iBlueControlMod firmware: what the single, double and
  triple tap of the button do, and four configuration profiles (save, load, delete).
- `ota.html`, `ota.js`, `utils/firmwareList.js`: the OTA page can list the firmware files of the iBlueControlMod GitHub
  repository (the tested baseline first) and flash one without downloading it first. Uploading a file still works.
- `utils/constants.js`: the command codes of the iBlueControlMod additions.
- The Google Analytics include and the blueretro.io `CNAME` were removed; the index links to the new page.

Everything else is unchanged, so the upstream pages (advance, presets, system, files, N64 and Dreamcast managers,
debug) work as before with any BlueRetro firmware.
