# iBlueControlMod web config

One page, one Bluetooth connection (`index.html`, `app.js`). The categories are in a menu on the side (a tab bar on a phone); light or dark follows the system and can be switched.

| Category | What | Where it is |
|---|---|---|
| Device | the connected adapter, deep sleep / reset / factory reset, the "Dreamcast only" setting, the connection log | `panels/device.js` |
| LEDs | brightness of a connected controller, length of the boot animation, the water (LED distance, density, tank length, group position, depth, level curve) with previews, Save and play | `panels/leds.js` |
| Buttons | what a single, double and triple tap do, the four profiles | `panels/buttons.js` |
| Controllers | the Dreamcast controller types (standard, 6 button + dual stick, racing, keyboard, mouse) with the default mapping | `panels/controllers.js` |
| Games | the game that runs (announced by openMenu), save the configuration for that game | `panels/games.js` |
| Update | the firmware: from a file or from the firmwares published on GitHub (the baseline first, checksums checked) | `ota.html` |
| Advanced | the BlueRetro pages: mapping, input presets, files, Dreamcast VMU, N64 Controller Pak, debug | `advance.html` and the others |

The pages of BlueRetro (`advance.html`, `ota.html`, `files.html` ...) are the upstream pages, shown inside the app (`?embed=1` hides their own heading and menu, `styles/embed.css`). They do not ask for a device of their own: `utils/connection.js` gives them the connection of the app, so connecting once is enough. Opened on their own they still work as before.

Settings that only the iBlueControlMod firmware has use BLE commands 0x40 to 0x4A (`utils/iblueCmd.js`); the table of the LED settings (`utils/ibluAnim.js`) is the same as `struct iblue_anim_cfg` in the firmware, a test compares them. Another firmware gets a note instead of those panels.

Publishing: `.github/workflows/web.yml` builds this folder with Jekyll and pushes it to the `gh-pages` branch from `main`.
Page: https://blaskkaffe.github.io/iBlueControlMod/ (Web Bluetooth needs Chrome/Edge over HTTPS).

Test (mocked Bluetooth and GitHub, needs Playwright): `NODE_PATH=/opt/node22/lib/node_modules node test/web_test.js <built _site>`.
Nothing here has been run against a real board or a real Bluetooth stack.

A fork of [BlueRetroWebCfg](https://github.com/darthcloud/BlueRetroWebCfg) (Apache-2.0, see NOTICE.md).
