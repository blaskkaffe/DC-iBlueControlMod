/* Prints the name tables of iblue_mapcfg.c for check_names.py:  "in <id> <name>", "out <index> <name> <dst>", and for every
 * argument "alias <text> <id>" (the source id the text parses to as a mapping input, or -1). */
#include <stdio.h>
#include <string.h>
#include "iblue/iblue_mapcfg.h"

int main(int argc, char **argv) {
    for (uint32_t i = 0; i < IBM_PAD_IDS; i++) {
        const char *n = iblue_in_name((uint8_t)i);
        printf("in %u %s\n", i, n ? n : "-");
    }
    for (uint32_t i = 0; i < IBLUE_OUT_COUNT; i++) {
        printf("out %u %s %u\n", i, iblue_out_name(i), iblue_out_dst(i));
    }
    for (int a = 1; a < argc; a++) {
        struct iblue_mapent e;
        const char *why;
        int c = iblue_map_parse(argv[a], strlen(argv[a]), 0, 0, &e, &why);
        printf("alias %s %d\n", argv[a], c == 1 ? e.src_btn : -1);
    }
    return 0;
}
