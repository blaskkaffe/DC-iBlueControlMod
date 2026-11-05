/* Shared helpers of the extra host tests (fuzz, properties, engine, known bugs). Test code only: it may use libc. */
#ifndef _TCOMMON_H_
#define _TCOMMON_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TUNUSED __attribute__((unused))
TUNUSED static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; if (fails < 40) printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECKM(c, ...) do { checks++; if (!(c)) { fails++; if (fails < 40) { printf("FAIL %s:%d: %s\n  ", __FILE__, __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } } while (0)
#define SAME(got, want) do { checks++; if (strcmp((got), (want)) != 0) { fails++; if (fails < 40) printf("FAIL %s:%d:\n got:  [%s]\n want: [%s]\n", __FILE__, __LINE__, got, want); } } while (0)

/* xorshift64*: deterministic, the seed comes from FUZZ_SEED (default 1) */
static uint64_t rng_state = 88172645463325252ull;
static uint32_t rnd(void) {
    rng_state ^= rng_state >> 12;
    rng_state ^= rng_state << 25;
    rng_state ^= rng_state >> 27;
    return (uint32_t)((rng_state * 2685821657736338717ull) >> 32);
}
TUNUSED static uint32_t rnd_below(uint32_t n) { return n ? rnd() % n : 0; }
TUNUSED static void rnd_seed(void) {
    const char *s = getenv("FUZZ_SEED");
    if (s) { rng_state ^= (uint64_t)strtoull(s, NULL, 10) * 0x9E3779B97F4A7C15ull; }
    for (int i = 0; i < 8; i++) { (void)rnd(); }
}
TUNUSED static long env_n(const char *name, long def) {
    const char *s = getenv(name);
    return s ? strtol(s, NULL, 10) : def;
}

#endif
