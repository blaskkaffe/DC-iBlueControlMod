/* Prints what the port LEDs do from the moment the LED code starts: the intro and then the first seconds of the
 * drops, the way iblue/iblue_port_led.c plays them (the LED task works in 20 ms frames, the drops are an
 * animation of their own that starts when the intro is over and ends with the whole, brightness capped at the
 * LED_MAX_PCT).
 * One line per `step_ms`: "<ms> d0 d1 d2 d3" (PWM duty 0..1023).
 * cc -o anim_intro_frames anim_intro_frames.c ../../iblue/iblue_anim_curve.c -lm
 * ./anim_intro_frames <total_ms> <start_ms> <show_ms> <step_ms> */
#include <stdio.h>
#include <stdlib.h>
#include "../../iblue/iblue_anim_curve.h"

#define FRAME_MS 20
#define MAX_PCT 80

int main(int argc, char **argv) {
    uint32_t total = argc > 1 ? (uint32_t)atoi(argv[1]) : 15000; /* the whole animation, from the console's power-on */
    uint32_t start = argc > 2 ? (uint32_t)atoi(argv[2]) : 1000;  /* how far into it the LED code starts */
    uint32_t show = argc > 3 ? (uint32_t)atoi(argv[3]) : 5000;
    uint32_t step = argc > 4 ? (uint32_t)atoi(argv[4]) : 10;
    uint32_t remain = total - start;
    uint32_t intro = remain > IBLUE_ANIM_INTRO_MS + 2000 ? IBLUE_ANIM_INTRO_MS : 0;
    uint32_t drops = remain - intro; /* the drops have an animation of their own after the intro, ending with the whole */

    for (uint32_t ms = 0; ms < show; ms += step) {
        uint32_t run = ms / FRAME_MS * FRAME_MS; /* the LED task updates every 20 ms */

        printf("%u", ms);
        for (int i = 0; i < IBLUE_ANIM_LEDS; i++) {
            uint32_t duty = 0;

            if (run < intro) {
                duty = iblue_anim_intro_duty(run, i);
            }
            else if (run < remain) {
                duty = iblue_anim_duty(run - intro, drops, i);
            }
            printf(" %u", duty * MAX_PCT / 100);
        }
        printf("\n");
    }
    return 0;
}
