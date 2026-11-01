/* Host test of the Dreamcast game id mailbox (iblue/iblue_gameid.c, with stubs): see run.sh */
#include <stdio.h>
#include <string.h>
#include "adapter/adapter.h"
#include "iblue/iblue_gameid.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static struct raw_fb queued[8];
static int nq;
void adapter_q_fb(struct raw_fb *fb) { if (nq < 8) queued[nq++] = *fb; }

static uint32_t crc32(const uint8_t *d, uint32_t n) {
    uint32_t c = 0xFFFFFFFF;
    while (n--) { c ^= *d++; for (int i = 0; i < 8; i++) c = (c >> 1) ^ (0xEDB88320 & (0 - (c & 1))); }
    return ~c;
}

/* What openMenu writes: the block, as four 128 byte phases */
static void make(uint8_t *b, uint32_t seq, const char *id) {
    memset(b, 0, 512);
    memcpy(b, "iBGI", 4);
    b[4] = 1;
    b[8] = (uint8_t)seq; b[9] = (uint8_t)(seq >> 8);
    strncpy((char *)b + 12, id, 16);
    uint32_t c = crc32(b, 28);
    b[28] = (uint8_t)c; b[29] = (uint8_t)(c >> 8); b[30] = (uint8_t)(c >> 16); b[31] = (uint8_t)(c >> 24);
}
static void send(const uint8_t *b) { for (uint32_t p = 0; p < 4; p++) iblue_gameid_write(p, b + p * 128); }

int main(void) {
    uint8_t b[512], r[512];

    CHECK(iblue_gameid_owns(200) && !iblue_gameid_owns(199) && !iblue_gameid_owns(201));
    iblue_gameid_read(r);
    CHECK(memcmp(r, "iBGI", 4) == 0 && r[4] == 1);                          /* marker an untouched adapter shows */

    make(b, 1, "MK51035"); send(b);
    CHECK(nq == 1 && queued[0].header.type == FB_TYPE_SYS_ID && queued[0].header.data_len == 10 &&
          memcmp(queued[0].data, "DC_MK51035", 10) == 0);
    iblue_gameid_read(r);
    CHECK(memcmp(r + 12, "MK51035", 7) == 0 && r[8] == 1);                  /* read back what was applied */

    send(b);                                                                /* the same request again: nothing */
    CHECK(nq == 1);
    make(b, 2, "MK51035"); send(b);                                         /* a new sequence number: applied again */
    CHECK(nq == 2);

    make(b, 3, "T-8101N-50-EXTRA"); send(b);                                /* long ids are cut to 10 characters */
    CHECK(nq == 3 && queued[2].header.data_len == 13 && memcmp(queued[2].data, "DC_T-8101N-5", 12) == 0);

    make(b, 4, ""); send(b);                                                /* no game: back to the normal configuration */
    CHECK(nq == 4 && queued[3].header.data_len == 0);

    /* things that must not do anything */
    make(b, 5, "../etc"); send(b); CHECK(nq == 4);                          /* characters that cannot be in a file name */
    make(b, 6, "MK 1"); send(b); CHECK(nq == 4);
    make(b, 7, "ABC"); b[13] ^= 1; send(b); CHECK(nq == 4);                 /* bad CRC */
    make(b, 8, "ABC"); for (uint32_t p = 0; p < 3; p++) iblue_gameid_write(p, b + p * 128);
    CHECK(nq == 4);                                                         /* half a write */
    memset(b, 0xFF, 512); send(b); CHECK(nq == 4);                          /* a game formatting the block */
    make(b, 9, "OKGAME"); send(b); CHECK(nq == 5 && memcmp(queued[4].data, "DC_OKGAME", 9) == 0); /* still works after that */

    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
