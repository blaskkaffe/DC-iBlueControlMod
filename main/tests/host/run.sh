#!/bin/sh
# Host tests of the portable iBlue code. Usage: sh main/tests/host/run.sh
set -e
d=$(cd "$(dirname "$0")" && pwd)
out=$(mktemp -d)
cc -Wall -Wextra -Werror -fsanitize=address,undefined -I"$d/../.." -o "$out/anim" "$d/test_anim.c" "$d/../../iblue/iblue_anim_curve.c" -lm
"$out/anim"
cc -Wall -Wextra -Werror -fsanitize=address,undefined -I"$d/../.." -o "$out/tap" "$d/test_tap.c" "$d/../../iblue/iblue_tap.c"
"$out/tap"
# the game id mailbox, with a few stubs instead of ESP-IDF
cc -Wall -Wextra -Werror -fsanitize=address,undefined -I"$d/stubs" -I"$d/../.." -o "$out/gameid" "$d/test_gameid.c" "$d/../../iblue/iblue_gameid.c"
"$out/gameid"
cc -Wall -Wextra -Werror -fsanitize=address,undefined -I"$d/../.." -o "$out/motion" "$d/test_motion.c" "$d/../../iblue/iblue_motion.c" -lm
"$out/motion"
rm -rf "$out"
