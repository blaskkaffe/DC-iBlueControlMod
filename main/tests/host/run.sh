#!/bin/sh
# Host tests of the portable iBlue code. Usage: sh main/tests/host/run.sh
set -e
d=$(cd "$(dirname "$0")" && pwd)
out=$(mktemp -d)
cc -Wall -Wextra -Werror -fsanitize=address,undefined -I"$d/../.." -o "$out/anim" "$d/test_anim.c" "$d/../../iblue/iblue_anim_curve.c" -lm
"$out/anim"
cc -Wall -Wextra -Werror -fsanitize=address,undefined -I"$d/../.." -o "$out/tap" "$d/test_tap.c" "$d/../../iblue/iblue_tap.c"
"$out/tap"
# the game id (Maple command 0x21), with a few stubs instead of ESP-IDF
cc -Wall -Wextra -Werror -fsanitize=address,undefined -I"$d/stubs" -I"$d/../.." -o "$out/gameid" "$d/test_gameid.c" "$d/../../iblue/iblue_gameid.c"
"$out/gameid"
# the DC API engine (dc1), with the game id module and the same stubs
cc -Wall -Wextra -Werror -fsanitize=address,undefined -I"$d/stubs" -I"$d/../.." -o "$out/dcapi" "$d/test_dcapi.c" "$d/../../iblue/iblue_dcapi.c" "$d/../../iblue/iblue_gameid.c" "$d/../../iblue/iblue_mapcfg.c"
"$out/dcapi"
# the shared mapping text form and the layouts
cc -Wall -Wextra -Werror -fsanitize=address,undefined -I"$d/stubs" -I"$d/../.." -o "$out/mapcfg" "$d/test_mapcfg.c" "$d/../../iblue/iblue_mapcfg.c"
"$out/mapcfg"
# ---- test engineer pass: properties, fuzz, engine behaviour, compatibility gap, names, known bugs (all of these pass today, except the known bugs)
CFLAGS="-Wall -Wextra -Werror -fsanitize=address,undefined -I$d/stubs -I$d/../.."
SYS="$d/../../iblue"
cc $CFLAGS -o "$out/props" "$d/test_props.c" "$SYS/iblue_mapcfg.c"
"$out/props"
cc $CFLAGS -o "$out/engine" "$d/test_engine.c" "$SYS/iblue_dcapi.c" "$SYS/iblue_gameid.c" "$SYS/iblue_mapcfg.c"
"$out/engine"
# FUZZ_N rounds (default 20000), FUZZ_SEED seed: `FUZZ_N=300000 FUZZ_SEED=7 sh main/tests/host/run.sh` for a long run
cc $CFLAGS -o "$out/fuzz" "$d/test_fuzz.c" "$SYS/iblue_dcapi.c" "$SYS/iblue_gameid.c" "$SYS/iblue_mapcfg.c"
"$out/fuzz"
cc $CFLAGS -o "$out/compat" "$d/test_compat.c" "$SYS/iblue_mapcfg.c"
"$out/compat"
if command -v python3 >/dev/null 2>&1; then
    python3 -I "$d/p2m_to_dc1.py" --selftest
    if [ -f "$d/../../../web/utils/constants.js" ]; then
        python3 -I "$d/check_names.py" "$d/../../.." | tail -3
    fi
fi
# Known bugs: each test says what SHOULD happen and fails while the bug is there (docs: main/tests/host/KNOWN_BUGS.md).
# They do not fail this script unless STRICT=1 (then any reproduced bug exits 1). An ASAN abort counts as a reproduction.
cc $CFLAGS -o "$out/bugs" "$d/test_bugs.c" "$SYS/iblue_dcapi.c" "$SYS/iblue_gameid.c" "$SYS/iblue_mapcfg.c"
nbugs=$("$out/bugs" list | wc -l)
repro=0
i=1
while [ "$i" -le "$nbugs" ]; do
    if "$out/bugs" "$i" >"$out/bug.log" 2>&1; then
        tail -1 "$out/bug.log"
    else
        repro=$((repro + 1))
        if grep -q "^BUG $i:" "$out/bug.log"; then grep "^BUG $i:" "$out/bug.log"; else echo "BUG $i: $("$out/bugs" list | sed -n "${i}p" | cut -f2): REPRODUCED (aborted: $(grep -m1 SUMMARY "$out/bug.log" | cut -c1-60))"; fi
    fi
    i=$((i + 1))
done
echo "known bugs: $repro of $nbugs reproduced"
if [ "$STRICT" = 1 ] && [ "$repro" -gt 0 ]; then
    rm -rf "$out"
    exit 1
fi
cc -Wall -Wextra -Werror -fsanitize=address,undefined -I"$d/../.." -o "$out/motion" "$d/test_motion.c" "$d/../../iblue/iblue_motion.c" -lm
"$out/motion"
rm -rf "$out"
