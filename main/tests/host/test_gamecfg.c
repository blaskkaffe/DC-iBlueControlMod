/* Built-in game setups (iblue_gamecfg.c): which ids are recognised, what the Samba setups contain. */
#include <stdio.h>
#include <string.h>
#include "iblue/iblue_gamecfg.h"

static int fails, checks;
#define CHECK(c, msg) do { checks++; if (!(c)) { fails++; printf("FAIL: %s\n", msg); } } while (0)

static int has(const struct iblue_mapent *e, size_t n, uint8_t src, uint8_t dst) {
    for (size_t i = 0; i < n; i++) {
        if (e[i].src_btn == src && e[i].dst_btn == dst) { return 1; }
    }
    return 0;
}

int main(void) {
    struct iblue_mapent e[16];
    size_t n;

    /* every version of the game, in the forms the id can arrive in */
    CHECK(iblue_game_profile("DC_HDR-0063") == IBLUE_GAME_SAMBA, "Samba Japan");
    CHECK(iblue_game_profile("HDR0063") == IBLUE_GAME_SAMBA, "without dash and prefix");
    CHECK(iblue_game_profile("DC_hdr-0131") == IBLUE_GAME_SAMBA, "Ver. 2000, lower case");
    CHECK(iblue_game_profile("DC_MK-51092") == IBLUE_GAME_SAMBA, "Samba USA");
    CHECK(iblue_game_profile("DC_MK-51092-5") == IBLUE_GAME_SAMBA, "Samba Europe, cut at 10 characters");
    CHECK(iblue_game_profile("DC_MK5109250") == IBLUE_GAME_SAMBA, "Samba Europe, number glued");
    /* other games and rubbish */
    CHECK(iblue_game_profile("DC_MK-51050") == IBLUE_GAME_NONE, "another game");
    CHECK(iblue_game_profile("DC_HDR-0064") == IBLUE_GAME_NONE, "next number");
    CHECK(iblue_game_profile("DC_HDR-006") == IBLUE_GAME_NONE, "too short");
    CHECK(iblue_game_profile("") == IBLUE_GAME_NONE, "empty");
    CHECK(iblue_game_profile("DC_") == IBLUE_GAME_NONE, "prefix only");
    CHECK(iblue_game_profile(NULL) == IBLUE_GAME_NONE, "null");
    CHECK(iblue_game_profile("DC_DCBIOS") == IBLUE_GAME_NONE, "BIOS");

    /* only with a controller that can be a maraca */
    CHECK(iblue_game_setup(IBLUE_GAME_SAMBA, 0, 0, 1) == IBLUE_SETUP_NONE, "no Joy-Con: nothing is set up");
    CHECK(iblue_game_setup(IBLUE_GAME_SAMBA, 2, 0, 1) == IBLUE_SETUP_MOTION, "two Joy-Cons, motion on");
    CHECK(iblue_game_setup(IBLUE_GAME_SAMBA, 1, 0, 1) == IBLUE_SETUP_MOTION, "one Joy-Con, motion on");
    CHECK(iblue_game_setup(IBLUE_GAME_SAMBA, 2, 0, 0) == IBLUE_SETUP_NONE, "Joy-Cons without motion mode: nothing");
    CHECK(iblue_game_setup(IBLUE_GAME_SAMBA, 0, 1, 1) == IBLUE_SETUP_NONE, "a Wii Remote alone: nothing yet (no motion read)");
    CHECK(iblue_game_setup(IBLUE_GAME_NONE, 2, 1, 1) == IBLUE_SETUP_NONE, "another game: nothing");

    /* Joy-Cons: left drives the left maraca, right the right; both into port 0 */
    n = iblue_game_map(IBLUE_GAME_SAMBA, 1, 0, e, 16);
    CHECK(n == 6, "left Joy-Con: 6 entries");
    CHECK(has(e, n, IBM_RY_UP, IBM_LY_DOWN) && has(e, n, IBM_RY_DOWN, IBM_LY_UP), "left: height turned around");
    CHECK(has(e, n, IBM_LT, IBM_RB_RIGHT), "left: shake = B");
    n = iblue_game_map(IBLUE_GAME_SAMBA, 1, 1, e, 16);
    CHECK(n == 7, "right Joy-Con: 7 entries");
    CHECK(has(e, n, IBM_LT, IBM_RB_DOWN), "right: shake = A");
    CHECK(has(e, n, IBM_MM, IBM_MM) && has(e, n, IBM_RM, IBM_MM), "right: Plus and ZR are Start");
    for (size_t i = 0; i < n; i++) { CHECK(e[i].dst_id == 0, "all drive port A"); }
    CHECK(iblue_game_map(IBLUE_GAME_SAMBA, 1, 2, e, 16) == 0, "no third input");

    /* a pad with two sticks */
    n = iblue_game_map(IBLUE_GAME_SAMBA, 0, 0, e, 16);
    CHECK(n == 13, "pad: 13 entries");
    CHECK(has(e, n, IBM_LS, IBM_RB_RIGHT) && has(e, n, IBM_RS, IBM_RB_DOWN), "pad: L1 / R1 shake");
    CHECK(has(e, n, IBM_MM, IBM_MM), "pad: Start is Start");
    CHECK(iblue_game_map(IBLUE_GAME_SAMBA, 0, 1, e, 16) == 0, "pad: the second input keeps its own table");

    /* limits */
    CHECK(iblue_game_map(IBLUE_GAME_SAMBA, 0, 0, e, 3) == 3, "output is limited");
    CHECK(iblue_game_map(IBLUE_GAME_NONE, 1, 0, e, 16) == 0, "no profile, no entries");

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
