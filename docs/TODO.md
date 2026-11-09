# To do

Order of the day: **organise and restructure first** (one branch, one place for the iBlue code, one implementation of each feature, one set of docs; see "Restructure"), then the items below. Reviewed with the owner 2026-11-01. What exists and how far it got: [FEATURES.md](FEATURES.md).

## Restructure (in progress on `claude/integration`)
- One integration branch from `main` with the current test firmware work (`claude/test-fw`), the web config and, later, `claude/restructure` (the layout, `brctl`).
- All fork-specific firmware in `main/iblue/` with one Kconfig menu; the upstream files keep one-line hooks.
- One implementation of each feature: tap actions stay; whole-configuration profiles were removed on `unified` (not in BlueRetro, not wanted); the game id arrives as Maple command 0x21 (no VMU mailbox); `brctl` is replaced by the DC API (docs/dcapi.md).
- Web page: redesigned as one page with one connection (done, to be tried on the adapter); the BlueRetro pages inside it keep their own look for now; one set of docs; test builds are not committed to `main`.
- Check before anything goes to `main`: every config builds (hw1, hw2, iblue), the host tests and the web tests pass.

## Features to add (after the restructure)
- **Customizable Home + key combos** (replaces "Home + arrows" and the fixed Home + face button). Holding Home on a controller and pressing a second button runs an action the user chose for that button, on the controller it is pressed on, and the second button is hidden from the game. Buttons that can be set: the four d-pad arrows and the four face buttons (the 8 "face buttons"), Start, Select, L1, L2, L3, R1, R2, R3, and the left and right half of the touchpad on DualShock and DualSense. Actions: rumble on/off, next controller type or a chosen one (GamePad, GamePadAlt, Racing), game id on/off, next colour, disconnect, later network switch. A table on the web page; saved in the adapter. Needs: a hotkey engine on the Bluetooth input (the one on `claude/restructure` is the start), and for the touchpad halves the touch position from the DS4/DS5 report (BlueRetro only passes the touchpad click as one button).
- **Tap actions**: the list of actions and their settings need changes (to be listed with the owner).
- **LED colours by network** (DCNow! / DCNET) and a button to switch networks: needs the control channel (`brctl`).
- **Samba de Amigo maracas**: concept in [samba-de-amigo.md](samba-de-amigo.md).
- **Panther mode** (`0x003FFFFF` with the name "Dreamcast PantherDC Controller") and the full device info reply (command 6, with the version string some games need): later, owner's call.

## Fixes and checks
- **Test**: brightness of a connected controller (20 %), the animation settings, the fireworks intro, the start-up order, game id with openMenu, Racing mode, `brctl`.

- **brctl**: the control channel over the Maple bus (status, config read and write, LED and pairing control, remappable gestures; it is on `development`). (The game id no longer uses a VMU mailbox: it arrives as Maple command 0x21 like on the other adapters, see `led-and-gameid.md`.)
- **Hotkeys**: a tap or Home + button that cycles a port's controller type (normal / dual stick / ...). Whole-configuration profiles were removed on `unified` (not in BlueRetro, not wanted); the per-game settings from openMenu stay (written on `claude/restructure`).
- **The openMenu Controllers screen** (remapping, type, rumble, VMU, per-button settings): on `claude/iblue-controllers` in openMenu-Virtual-Folder-Bundle; it needs brctl.
- **A better factory reset**: the 10 s hold is too easy to trigger. Per-game configurations are already kept through it.
- **Measure the BIOS boot animation** on a real console and set the length of the animation on the web config (LED settings; default 15 s); check the 700 ms of adapter start-up (`BLUERETRO_IBLUE_BOOT_ANIM_BOOT_MS`).
- **Plain HW1/HW2 builds** with these features (`dreamcast_openmenu` configs on `claude/restructure`): kept for now.
- Hide the blue flash of the power LED at power-on: it is the chip's, before the firmware runs; acceptable for now.

## Dropped
- A port D (or any port) that is always there but not a controller.

## Done
- The `iblue-v1.9` text is out of `docs/README_iBlueControlMod.md`.
