#!/bin/sh
# Host tests of the portable iBlue code. Usage: sh main/tests/host/run.sh
set -e
d=$(cd "$(dirname "$0")" && pwd)
out=$(mktemp -d)
cc -Wall -Wextra -Werror -fsanitize=address,undefined -I"$d/../.." -o "$out/anim" "$d/test_anim.c" "$d/../../system/iblue_anim_curve.c" -lm
"$out/anim"
cc -Wall -Wextra -Werror -fsanitize=address,undefined -I"$d/../.." -o "$out/tap" "$d/test_tap.c" "$d/../../system/iblue_tap.c"
"$out/tap"
# the game id (Maple command 0x21), with a few stubs instead of ESP-IDF
cc -Wall -Wextra -Werror -fsanitize=address,undefined -I"$d/stubs" -I"$d/../.." -o "$out/gameid" "$d/test_gameid.c" "$d/../../system/iblue_gameid.c"
"$out/gameid"
# the DC API engine (dc1), with the game id module and the same stubs
cc -Wall -Wextra -Werror -fsanitize=address,undefined -I"$d/stubs" -I"$d/../.." -o "$out/dcapi" "$d/test_dcapi.c" "$d/../../system/iblue_dcapi.c" "$d/../../system/iblue_gameid.c"
"$out/dcapi"
rm -rf "$out"
