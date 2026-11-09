/* Prints the boot animation as frames: one line per frame, "<ms> d0 d1 d2 d3 | h0 .. h40" (PWM duty 0..1023,
 * then the water height at 41 points).
 * cc -o anim_frames anim_frames.c ../../iblue/iblue_anim_curve.c -lm && ./anim_frames <total_ms> <fps> */
#include <stdio.h>
#include <stdlib.h>
#include "../../iblue/iblue_anim_curve.h"

int main(int argc, char **argv) {
    uint32_t total = argc > 1 ? (uint32_t)atoi(argv[1]) : 6000;
    int fps = argc > 2 ? atoi(argv[2]) : 30;

    for (uint32_t ms = 0; ms < total + 400; ms += 1000 / fps) {
        printf("%u", ms);
        for (int i = 0; i < IBLUE_ANIM_LEDS; i++) {
            printf(" %u", iblue_anim_duty(ms, total, i));
        }
        printf(" |");
        /* the water surface, 41 samples across the 4 LEDs (for the preview drawing) */
        for (int i = 0; i <= 40; i++) {
            printf(" %.3f", iblue_anim_height(4.0f * (float)i / 40.0f, ms, total));
        }
        printf("\n");
    }
    return 0;
}
