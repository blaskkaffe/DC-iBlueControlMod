/* Host test of the Dreamcast game id (Maple command 0x21, iblue/iblue_gameid.c, with stubs): see run.sh */
#include <stdio.h>
#include <string.h>
#include "adapter/adapter.h"
#include "iblue/iblue_gameid.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static struct raw_fb queued[16];
static int nq;
void adapter_q_fb(struct raw_fb *fb) { if (nq < 16) queued[nq++] = *fb; }

/* What openMenu sends: strncpy(frame + 4, product, 12), so NUL padded */
static void send_id(const char *id) {
    uint8_t raw[IBLUE_GAMEID_LEN];
    memset(raw, 0, sizeof(raw));
    strncpy((char *)raw, id, sizeof(raw));
    iblue_gameid_set(raw);
}

/* The same through the Maple bus: the sender's memory with every 4 bytes reversed (what maple.c receives) */
static void send_wire(const char *id) {
    uint8_t mem[IBLUE_GAMEID_LEN], wire[IBLUE_GAMEID_LEN];
    memset(mem, 0, sizeof(mem));
    strncpy((char *)mem, id, sizeof(mem));
    for (int w = 0; w < IBLUE_GAMEID_LEN; w += 4)
        for (int j = 0; j < 4; j++) wire[w + j] = mem[w + 3 - j];
    iblue_gameid_set_wire(wire);
}

int main(void) {
    uint8_t raw[IBLUE_GAMEID_LEN];
    uint8_t ext[16];

    CHECK(IBLUE_GAMEID_CMD == 0x21 && IBLUE_GAMEID_LEN == 12);
    CHECK(strlen(IBLUE_EXT_NAME) == 16);                                    /* the 16 bytes openMenu compares */

    iblue_gameid_ext_name(ext);                                             /* sent word-reversed, like the other strings in maple.c */
    CHECK(memcmp(ext, "ulBinoCelort doM", 16) == 0);

    send_id(""); CHECK(nq == 0);                                            /* nothing applied yet: an empty id changes nothing */

    send_id("MK51035");
    CHECK(nq == 1 && queued[0].header.type == FB_TYPE_SYS_ID && queued[0].header.data_len == 10 &&
          memcmp(queued[0].data, "DC_MK51035", 10) == 0);

    send_id("MK51035"); CHECK(nq == 1);                                     /* the same id again (openMenu sends to every launch): nothing */

    send_id("T-8101N-50-E");                                                /* 12 bytes without a NUL: cut to 10 characters */
    CHECK(nq == 2 && queued[1].header.data_len == 13 && memcmp(queued[1].data, "DC_T-8101N-5", 12) == 0);

    memset(raw, ' ', sizeof(raw)); memcpy(raw, "MK51035", 7);                /* a sender that pads with spaces */
    iblue_gameid_set(raw);
    CHECK(nq == 3 && queued[2].header.data_len == 10 && memcmp(queued[2].data, "DC_MK51035", 10) == 0);

    send_id(""); CHECK(nq == 4 && queued[3].header.data_len == 0);          /* no game: back to the normal configuration */
    send_id(""); CHECK(nq == 4);                                            /* ... and only once */

    /* things that must not do anything */
    send_id("DCBIOS"); CHECK(nq == 4);                                      /* the BIOS menu is not a game */
    send_id("../etc"); CHECK(nq == 4);                                      /* characters that cannot be in a file name */
    send_id("MK 1"); CHECK(nq == 4);
    memset(raw, 0xFF, sizeof(raw)); iblue_gameid_set(raw); CHECK(nq == 4);  /* a corrupt frame */
    send_id("OKGAME"); CHECK(nq == 5 && memcmp(queued[4].data, "DC_OKGAME", 9) == 0);   /* still works after that */
    send_id("DCBIOS"); send_id("OKGAME"); CHECK(nq == 5);                   /* DCBIOS in between does not forget the game */

    send_wire("T-8101N-50"); CHECK(nq == 6 && queued[5].header.data_len == 13 && memcmp(queued[5].data, "DC_T-8101N-50", 13) == 0);   /* as it arrives on the bus */
    send_wire("T-8101N-50"); CHECK(nq == 6);
    send_wire("DCBIOS"); CHECK(nq == 6);

    printf("%d checks, %d failed\n", checks, fails);
    return fails != 0;
}
