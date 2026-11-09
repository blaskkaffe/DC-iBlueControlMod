#!/usr/bin/env python3
"""Cross-check the names of main/iblue/iblue_mapcfg.c against two sources of truth, and print the tables.

 1. web/utils/constants.js, btnList, the "Xbox One S / X|S" column: BlueRetro's generic button id -> the label of the Xbox controller.
    Every input name of iblue_mapcfg.c is compared with the label of its id.
 2. main/adapter/wired/dc.c: dc_mask / dc_desc / dc_btns_mask and the DC_* bit enum: which BlueRetro destination ids a Dreamcast pad
    accepts, and which bit each sets. Every output name is compared with that.

Usage: python3 check_names.py [repo_root]      exit 0 = no mismatch (notes do not count), 1 = a mismatch
Not a hardware test: it reads source files and runs the host-compiled name tables."""
import os
import re
import subprocess
import sys
import tempfile

root = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
here = os.path.dirname(os.path.abspath(__file__))
cc = os.environ.get("CC", "cc")
bad = []
notes = []

# ---- the dump of our tables
ALIASES = ["a1_left", "a1_right", "a1_down", "a1_up", "a2_left", "a2_right", "a2_down", "a2_up", "dpad_left", "dpad_right", "dpad_down", "dpad_up",
           "dpad2_left", "dpad2_right", "dpad2_down", "dpad2_up", "select", "menu", "view", "l1", "r1", "l2", "r2",
           "A1_UP", "A2_LEFT", "DPAD_DOWN", "SELECT", "A", "LB", "LT", "lstick.click", "rstick.click"]
with tempfile.TemporaryDirectory() as tmp:
    exe = os.path.join(tmp, "names_dump")
    subprocess.check_call([cc, "-Wall", "-Wextra", "-I" + os.path.join(here, "stubs"), "-I" + os.path.join(root, "main"), "-o", exe,
                           os.path.join(here, "names_dump.c"), os.path.join(root, "main", "iblue", "iblue_mapcfg.c")])
    dump = subprocess.check_output([exe] + ALIASES).decode().splitlines()
in_names, outs, alias = {}, [], {}
for ln in dump:
    f = ln.split()
    if f[0] == "in":
        in_names[int(f[1])] = None if f[2] == "-" else f[2]
    elif f[0] == "out":
        outs.append((f[2], int(f[3])))
    elif f[0] == "alias":
        alias[f[1]] = int(f[2])

# ---- 1. the Xbox column of the web config
js = open(os.path.join(root, "web", "utils", "constants.js"), encoding="utf-8").read()
header = None
for ln in js.splitlines():
    if "Xbox One S" in ln and ln.strip().startswith("/*"):
        header = re.split(r"\s{2,}", ln.strip().strip("/*").strip())
        break
if not header:
    sys.exit("no header comment with the Xbox One S column in constants.js")
col = next(i for i, h in enumerate(header) if h.startswith("Xbox One S"))
rows = []
for ln in js.splitlines():
    s = ln.strip()
    if s.startswith("['") and s.endswith("],"):
        rows.append([m.replace("\\'", "'") for m in re.findall(r"'((?:[^'\\]|\\.)*)'", s)])
# the first 32 rows are BlueRetro's pad ids 0..31 (the rest are keyboard and mouse ids)
assert rows[0][0].startswith("GP: LX Left") and rows[31][0].startswith("GP: RJ"), "unexpected btnList order"
xbox = [r[col] if col < len(r) else "" for r in rows[:32]]

def expect_input(label):
    """The input name the Xbox label should have (None = none expected)."""
    m = re.fullmatch(r"(Left|Right) stick (left|right|up|down)", label)
    if m:
        return ("lstick." if m.group(1) == "Left" else "rstick.") + m.group(2)
    m = re.fullmatch(r"D-pad (left|right|up|down)", label)
    if m:
        return "dpad." + m.group(1)
    return {"X": "x", "B": "b", "A": "a", "Y": "y", "Menu": "start", "View": "back", "Xbox": "guide", "Share": "share", "LT": "lt", "LB": "lb",
            "RT": "rt", "RB": "rb", "L Stick": "lstick.click", "R Stick": "rstick.click"}.get(label)

print("== inputs: BlueRetro id | web config label (Xbox One S / X|S) | our input name | result")
for i in range(32):
    label, ours, want = xbox[i], in_names.get(i), expect_input(xbox[i])
    if want is None:
        if ours is None:
            res = "ok (no name: %s)" % ("no Xbox label" if not label else "'%s' is covered by another input" % label)
        else:
            res = "NOTE: '%s' has no Xbox label; the name exists for other pads" % ours
            notes.append("input %d '%s': no label in the Xbox column (reachable only from other controllers)" % (i, ours))
    elif ours == want:
        res = "ok"
    else:
        res = "MISMATCH: want '%s'" % want
        bad.append("input id %d: Xbox label '%s' -> expected '%s', iblue_mapcfg.c has '%s'" % (i, label, want, ours))
    print("%2d | %-18s | %-13s | %s" % (i, label or "-", ours or "-", res))
# the ones named in the task
task = {16: "x", 17: "b", 18: "a", 19: "y", 20: "start", 21: "back", 24: "lt", 25: "lb", 27: "lstick.click", 28: "rt", 29: "rb", 31: "rstick.click"}
for i, n in task.items():
    if in_names.get(i) != n:
        bad.append("task table: id %d should be '%s', is '%s'" % (i, n, in_names.get(i)))
for i, lab in {16: "X", 17: "B", 18: "A", 19: "Y", 20: "Menu", 21: "View", 24: "LT", 25: "LB", 28: "RT", 29: "RB"}.items():
    if xbox[i] != lab:
        bad.append("constants.js id %d: label is '%s', the task says '%s'" % (i, xbox[i], lab))
# aliases: Pico2Maple's / BlueRetro's spellings must land on the same ids as the canonical names
canon = {n: i for i, n in in_names.items() if n}
alias_want = {"a1_left": "lstick.left", "a1_right": "lstick.right", "a1_down": "lstick.down", "a1_up": "lstick.up",
              "a2_left": "rstick.left", "a2_right": "rstick.right", "a2_down": "rstick.down", "a2_up": "rstick.up",
              "dpad_left": "dpad.left", "dpad_right": "dpad.right", "dpad_down": "dpad.down", "dpad_up": "dpad.up",
              "dpad2_left": "dpad2.left", "dpad2_right": "dpad2.right", "dpad2_down": "dpad2.down", "dpad2_up": "dpad2.up",
              "select": "back", "menu": "start", "view": "back", "l1": "lb", "r1": "rb", "l2": "lt", "r2": "rt",
              "A1_UP": "lstick.up", "A2_LEFT": "rstick.left", "DPAD_DOWN": "dpad.down", "SELECT": "back", "A": "a", "LB": "lb", "LT": "lt",
              "lstick.click": "lstick.click", "rstick.click": "rstick.click"}
for a, want in alias_want.items():
    if alias.get(a) != canon[want]:
        bad.append("alias '%s' parses to id %s, expected %d (%s)" % (a, alias.get(a), canon[want], want))
# the web column also says what l1/r1 etc. are on a PS pad: ids 25 / 29 are the shoulder buttons, 24 / 28 the analog triggers
print("aliases checked: %d" % len(alias_want))

# ---- 2. the Dreamcast driver
dc = open(os.path.join(root, "main", "adapter", "wired", "dc.c"), encoding="utf-8").read()
enum = re.search(r"enum\s*\{(.*?)\};", dc, re.S).group(1)
bits = {}
for k, name in enumerate(re.findall(r"\b(DC_[A-Z_]+)\b", enum)):
    bits[name] = k
def u32(name):
    m = re.search(name + r"\[4\]\s*=\s*\{\s*(0x[0-9A-Fa-f]+)", dc)
    return int(m.group(1), 16)
dc_mask, dc_desc, mouse_mask, kb_mask = u32("dc_mask"), u32("dc_desc"), u32("dc_mouse_mask"), u32("dc_kb_mask")
body = re.search(r"dc_btns_mask\[32\]\s*=\s*\{(.*?)\};", dc, re.S).group(1)
btns = []
for item in re.split(r",", re.sub(r"\s+", " ", body)):
    item = item.strip()
    if not item:
        continue
    btns.append(item)
# BIT(DC_X) entries contain no comma: the split above is by comma, the list has 32 items
assert len(btns) == 32, len(btns)
def btn_bit(i):
    m = re.fullmatch(r"BIT\((DC_[A-Z_]+)\)", btns[i])
    return m.group(1) if m else None

expected_bit = {"A": "DC_A", "B": "DC_B", "X": "DC_X", "Y": "DC_Y", "START": "DC_START", "D": "DC_D", "Z": "DC_Z", "C": "DC_C",
                "DPAD_UP": "DC_LD_UP", "DPAD_DOWN": "DC_LD_DOWN", "DPAD_LEFT": "DC_LD_LEFT", "DPAD_RIGHT": "DC_LD_RIGHT",
                "DPAD2_UP": "DC_RD_UP", "DPAD2_DOWN": "DC_RD_DOWN", "DPAD2_LEFT": "DC_RD_LEFT", "DPAD2_RIGHT": "DC_RD_RIGHT"}
print()
print("== outputs: name | BlueRetro dst id | pad mode (dc_mask, dc_desc, dc_btns_mask) | mouse mode | keyboard mode | result")
print("dc_mask=%08X dc_desc=%08X dc_mouse_mask=%08X dc_kb_mask[0]=%08X" % (dc_mask, dc_desc, mouse_mask, kb_mask))
for name, dst in outs:
    in_mask = bool(dc_mask >> dst & 1)
    is_axis = bool(dc_desc >> dst & 1)
    bit = btn_bit(dst)
    if not in_mask:
        res = "MISMATCH: the Dreamcast pad driver IGNORES destination id %d" % dst
        bad.append("output %s (dst %d) is not in dc_mask: the driver ignores it" % (name, dst))
    elif name in expected_bit:
        if is_axis or bit != expected_bit[name]:
            res = "MISMATCH: sets %s, expected %s" % (bit or ("axis" if is_axis else "nothing"), expected_bit[name])
            bad.append("output %s (dst %d): dc.c sets %s, expected %s" % (name, dst, bit, expected_bit[name]))
        else:
            res = "ok -> bit %s" % bit
    else:   # analog: sticks (A1/A2) and the triggers (LT/RT)
        if is_axis and bit is None:
            res = "ok -> analog axis"
        else:
            res = "MISMATCH: expected an analog axis (dc_desc bit set, no button bit)"
            bad.append("output %s (dst %d): not an analog axis in dc.c" % (name, dst))
    print("%-11s | %3d | %-9s | %-5s | %-5s | %s" % (name, dst, "pad:yes" if in_mask else "pad:NO", "mouse" if mouse_mask >> dst & 1 else "-",
                                                     "kb" if kb_mask >> dst & 1 else "-", res))
    if in_mask and not (mouse_mask >> dst & 1):
        pass
n_mouse = sum(1 for _, d in outs if mouse_mask >> d & 1)
notes.append("map.* keys name the pad's outputs: in mouse mode only %d of %d output destinations are accepted by the driver, in keyboard mode %d "
             "(the keys do not say so; read from dc_mouse_mask / dc_kb_mask)" % (n_mouse, len(outs), sum(1 for _, d in outs if kb_mask >> d & 1)))
# destinations the driver accepts that no output can reach
unreach = [d for d in range(32) if (dc_mask >> d & 1) and d not in [x[1] for x in outs]]
notes.append("destination ids the pad driver accepts but no output name reaches: %s (22 = guide/MT is accepted by dc_mask but has no bit)" % unreach)

print()
for n in notes:
    print("NOTE:", n)
for b in bad:
    print("MISMATCH:", b)
print("check_names: %d mismatches, %d notes" % (len(bad), len(notes)))
sys.exit(1 if bad else 0)
