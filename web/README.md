# iBlueControlMod web config

A fork of [BlueRetroWebCfg](https://github.com/darthcloud/BlueRetroWebCfg) (Apache-2.0, see NOTICE.md) that adds what the
iBlueControlMod firmware has:

- `iblue.html`: map single / double / triple tap to an action, and save / load / delete 4 whole-config profiles
  (BLE commands 0x40-0x46, `main/bluetooth/att_cfg.c`). Other firmwares answer with a clear message instead.
- `ota.html`: besides the file picker, a list of firmwares from this GitHub repo (`firmware-sources.json`).
  The baseline (`firmware/BlueRetro_iBlueControlMod_RGB_v1.bin` on main) is listed first and preselected; each download is
  checked against the folder's `SHA256SUMS.txt` before it is sent.

Publishing: `.github/workflows/web.yml` builds this folder with Jekyll and pushes it to the `gh-pages` branch.
Enable it once: repo Settings > Pages > Deploy from a branch > `gh-pages` / root.
Page: https://blaskkaffe.github.io/iBlueControlMod/ (Web Bluetooth needs Chrome/Edge over HTTPS).

Test (mocked Bluetooth + GitHub, needs Playwright): `NODE_PATH=/opt/node22/lib/node_modules node test/web_test.js <built _site>`.
Nothing here has been run against a real board or a real Bluetooth stack.
