#!/usr/bin/env python3
"""Proposal / prototype: convert a Pico2Maple map file ("INPUT: OUTPUT" lines) into DC API requests (dc1) for this adapter, and back.

    python3 p2m_to_dc1.py map.cfg [--port A] [--complete]     prints a dc1 request (one "set port.X.map.OUT ..." line per output)
    python3 p2m_to_dc1.py --reverse request.txt               prints "INPUT: OUTPUT" lines from "get port.A.map.*" answers
    python3 p2m_to_dc1.py --selftest

The rules (all of them can be done on the phone / PC / DreamDash; the adapter's parser does not change):
  * a line "IN: OUT" adds IN to the list of OUT (several lines for one OUT give "IN1, IN2"; more than 4 -> error, the Pico2Maple limit is the same);
  * "OPTION_...: value" lines are settings of the Pico2Maple, not a mapping: collected and reported, never sent;
  * '#' and ';' start a comment; blank lines are ignored; names are matched without regard to case;
  * without --complete only the outputs that are named are sent (the others keep what they have); with --complete every output not named gets
    "none" (what a complete Pico2Maple file means);
  * a name the adapter does not know (input or output) is reported with its line number and the request is not produced (exit 1): no half files.
Names: our firmware reads the Pico2Maple spellings of inputs (A1_UP, A2_LEFT, DPAD_DOWN, SELECT, ...) as aliases, so inputs are passed through
(lower-cased); outputs use the same names on both sides. If a real Pico2Maple name is not in the lists below, add it to the two tables (tables
taken from iblue_mapcfg.c; the Pico2Maple's own list was not available here)."""
import re
import sys

OUTPUTS = ("A B X Y START D Z C LT RT A1_UP A1_DOWN A1_LEFT A1_RIGHT A2_UP A2_DOWN A2_LEFT A2_RIGHT "
           "DPAD_UP DPAD_DOWN DPAD_LEFT DPAD_RIGHT DPAD2_UP DPAD2_DOWN DPAD2_LEFT DPAD2_RIGHT").split()
INPUTS = ("a b x y lb rb lt rt back start guide share lstick.up lstick.down lstick.left lstick.right lstick.click "
          "rstick.up rstick.down rstick.left rstick.right rstick.click dpad.up dpad.down dpad.left dpad.right "
          "dpad2.up dpad2.down dpad2.left dpad2.right "
          "a1_up a1_down a1_left a1_right a2_up a2_down a2_left a2_right dpad_up dpad_down dpad_left dpad_right "
          "dpad2_up dpad2_down dpad2_left dpad2_right select menu view l1 r1 l2 r2").split()


def convert(text, port="A", complete=False):
    per_out, settings, errors = {}, [], []
    for no, raw in enumerate(text.splitlines(), 1):
        line = re.split(r"[#;]", raw, maxsplit=1)[0].strip()
        if not line:
            continue
        if ":" not in line:
            errors.append("line %d: no ':' in '%s'" % (no, raw.strip()))
            continue
        left, right = [s.strip() for s in line.split(":", 1)]
        if left.upper().startswith("OPTION_"):
            settings.append((left, right))
            continue
        inp, out = left.lower(), right.upper()
        if inp not in INPUTS:
            errors.append("line %d: unknown input '%s'" % (no, left))
        elif out not in OUTPUTS:
            errors.append("line %d: unknown output '%s'" % (no, right))
        else:
            lst = per_out.setdefault(out, [])
            if inp in lst:
                errors.append("line %d: '%s' twice on %s" % (no, left, out))
            elif len(lst) >= 4:
                errors.append("line %d: more than 4 inputs on %s" % (no, out))
            else:
                lst.append(inp)
    if errors:
        return None, settings, errors
    lines = ["dc1"]
    for out in OUTPUTS:
        if out in per_out:
            lines.append("set port.%s.map.%s %s" % (port, out, ", ".join(per_out[out])))
        elif complete:
            lines.append("set port.%s.map.%s none" % (port, out))
    return "\n".join(lines) + "\n", settings, errors


def reverse(answer):
    out = []
    for ln in answer.splitlines():
        m = re.match(r"ok port\.[A-D]\.map\.(\w+) (.*)$", ln)
        if m and m.group(2) != "none":
            for tok in m.group(2).split(","):
                out.append("%s: %s" % (tok.strip().split(":")[0].upper(), m.group(1)))
    return "\n".join(out) + "\n"


def selftest():
    req, settings, err = convert("A: A\nLB: Z\nRB: Z   # two inputs on one output\nOPTION_AUTOLOAD: TRUE\n\n; comment\nSELECT: D\n")
    assert err == [] and settings == [("OPTION_AUTOLOAD", "TRUE")], (err, settings)
    assert req == "dc1\nset port.A.map.A a\nset port.A.map.D select\nset port.A.map.Z lb, rb\n", req
    assert "OPTION" not in req
    full, _, _ = convert("A: A\n", port="B", complete=True)
    assert full.count("\n") == 27 and "set port.B.map.Z none" in full and "set port.B.map.A a" in full
    assert convert("FOO: A\n")[0] is None and convert("A: NOPE\n")[2][0].startswith("line 1: unknown output")
    assert convert("A: A\nA: A\n")[0] is None
    assert convert("a: Z\nb: Z\nx: Z\ny: Z\nlb: Z\n")[0] is None
    assert convert("garbage\n")[0] is None
    assert reverse("dc1\nok port.A.map.Z lb, rb:turbo=3\nok port.A.map.D none\nok port.A.map.A a\n") == "LB: Z\nRB: Z\nA: A\n"
    print("p2m_to_dc1 selftest ok")


if __name__ == "__main__":
    a = sys.argv[1:]
    if a == ["--selftest"]:
        selftest()
    elif a[:1] == ["--reverse"] and len(a) == 2:
        sys.stdout.write(reverse(open(a[1]).read()))
    elif a and not a[0].startswith("--"):
        port = a[a.index("--port") + 1] if "--port" in a else "A"
        req, settings, errs = convert(open(a[0]).read(), port, "--complete" in a)
        for s in settings:
            sys.stderr.write("setting not sent: %s: %s\n" % s)
        if req is None:
            sys.stderr.write("\n".join(errs) + "\n")
            sys.exit(1)
        sys.stdout.write(req)
    else:
        sys.exit(__doc__)
