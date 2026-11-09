/* Host test of the tap counting: see run.sh */
#include <stdio.h>
#include "../../iblue/iblue_tap.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

/* A button pattern: pairs of (pressed, ms). Runs it with 10 ms steps and reports the events. */
struct seg { int pressed; uint32_t ms; };
struct result { int done, hold_start, hold_end, taps; uint32_t done_at; };

static struct result run(const struct seg *s, int n) {
    struct iblue_tap t;
    struct result r = {0, 0, 0, 0, 0};
    uint32_t now = 0;

    iblue_tap_start(&t);
    for (int i = 0; i < n && !r.done; i++) {
        for (uint32_t ms = 0; ms < s[i].ms && !r.done; ms += 10, now += 10) {
            int ev = iblue_tap_step(&t, s[i].pressed, 10);
            if (ev == IBLUE_TAP_EV_DONE) { r.done = 1; r.taps = t.taps; r.done_at = now; }
            if (ev == IBLUE_TAP_EV_HOLD_START) r.hold_start = 1;
            if (ev == IBLUE_TAP_EV_HOLD_END) { r.hold_end = 1; r.done = 1; }
        }
    }
    return r;
}

int main(void) {
    struct result r;

    /* one tap: nothing follows */
    { const struct seg s[] = {{0, 1000}}; r = run(s, 1); CHECK(r.done && r.taps == 1 && r.done_at >= 390 && r.done_at <= 410); }
    /* two taps */
    { const struct seg s[] = {{0, 150}, {1, 80}, {0, 1000}}; r = run(s, 3); CHECK(r.done && r.taps == 2 && !r.hold_start); }
    /* three taps: final as soon as the third ends, with no wait */
    { const struct seg s[] = {{0, 150}, {1, 80}, {0, 120}, {1, 80}, {0, 1000}}; r = run(s, 5);
      CHECK(r.done && r.taps == 3 && r.done_at <= 150 + 80 + 120 + 80 + 10); }
    /* a pause longer than the gap makes the next press a new tap sequence, not the second tap */
    { const struct seg s[] = {{0, 500}, {1, 80}, {0, 1000}}; r = run(s, 3); CHECK(r.done && r.taps == 1 && r.done_at < 450); }
    /* a second press that is kept down: tap + hold, then released */
    { const struct seg s[] = {{0, 150}, {1, 1500}, {0, 200}}; r = run(s, 3); CHECK(r.hold_start && r.hold_end && !r.taps); }
    /* a second press just under the hold time is a normal second tap */
    { const struct seg s[] = {{0, 150}, {1, 500}, {0, 1000}}; r = run(s, 3); CHECK(r.done && r.taps == 2 && !r.hold_start); }
    /* the third press kept down is still the third tap (no tap + hold), counted when released */
    { const struct seg s[] = {{0, 100}, {1, 80}, {0, 100}, {1, 1500}, {0, 1000}}; r = run(s, 5); CHECK(r.done && r.taps == 3 && !r.hold_start); }
    /* slow taps at the edge of the gap */
    { const struct seg s[] = {{0, 380}, {1, 80}, {0, 1000}}; r = run(s, 3); CHECK(r.done && r.taps == 2); }

    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
