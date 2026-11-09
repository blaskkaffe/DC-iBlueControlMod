/* Compatibility gap: Pico2Maple's own map file has lines "INPUT: OUTPUT" (A: A, LB: Z, OPTION_AUTOLOAD: TRUE), one input per line, an
 * output may appear on several lines. iblue_mapcfg.c reads "OUTPUT = INPUT, INPUT" (the output is the key, the value is a list), so a
 * Pico2Maple line is not understood, and worse: "A: A" is read as the input "A" with an (empty) option.
 * This test DOCUMENTS the gap: each XFAIL line is behaviour that is wrong for a Pico2Maple file today; if the parser learns the form, the
 * XFAIL check flips and this test fails on purpose (then turn it into a normal test). See p2m_to_dc1.py for the converter proposal.
 * The Pico2Maple file format here is taken from the task description (not from DC-UnifiedEcosystem/docs/08-microsap.md, which could not be read). */
#include "tcommon.h"
#include "iblue/iblue_mapcfg.h"

static int xfail_seen, xfail_fixed;
#define XFAIL(c, what) do { if (!(c)) { xfail_seen++; printf("  XFAIL (expected, gap): %s\n", what); } else { xfail_fixed++; printf("  XPASS: %s now works: make this a normal test\n", what); } } while (0)

int main(void) {
    struct iblue_mapent e[4];
    const char *why = "";
    int c;

    /* the lines of a Pico2Maple file given to the value parser (what a naive "send the file line by line" does) */
    c = iblue_map_parse("A: A", 4, 0, 0, e, &why);
    XFAIL(c == 1 && e[0].src_btn == IBM_RB_DOWN, "\"A: A\" as a value is read as input a -> output A");
    CHECK(c == -1);                                                                  /* what happens today: refused, why = unknown option */
    CHECK(strcmp(why, " range unknown option") == 0);
    c = iblue_map_parse("LB: Z", 5, 6, 0, e, &why);
    XFAIL(c == 1 && e[0].src_btn == IBM_LS, "\"LB: Z\" as a value is read as input lb -> output Z");
    CHECK(c == -1);
    c = iblue_map_parse("OPTION_AUTOLOAD: TRUE", 21, 0, 0, e, &why);
    XFAIL(c >= 0, "\"OPTION_AUTOLOAD: TRUE\" is a setting, not a mapping, and is skipped");
    CHECK(c == -1);
    /* the same names in the form we do read */
    c = iblue_map_parse("lb", 2, (uint32_t)iblue_out_find("Z", 1), 0, e, &why);
    CHECK(c == 1 && e[0].src_btn == IBM_LS);
    c = iblue_map_parse("LB", 2, (uint32_t)iblue_out_find("Z", 1), 0, e, &why);       /* upper case input names are read (aliases are case insensitive) */
    CHECK(c == 1 && e[0].src_btn == IBM_LS);
    /* a Pico2Maple file maps several inputs to one output on separate lines: "LB: Z" and "RB: Z" are two lines, here one value */
    c = iblue_map_parse("LB, RB", 6, (uint32_t)iblue_out_find("Z", 1), 0, e, &why);
    CHECK(c == 2);

    printf("compat: %d checks, %d failed, %d expected gaps shown, %d gaps closed\n", checks, fails, xfail_seen, xfail_fixed);
    return fails || xfail_fixed ? 1 : 0;
}
