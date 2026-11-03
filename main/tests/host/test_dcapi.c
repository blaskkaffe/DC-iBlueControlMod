/* Host test of the DC API engine (system/iblue_dcapi.c, dc1): see run.sh */
#include <stdio.h>
#include <string.h>
#include "adapter/adapter.h"
#include "system/iblue_gameid.h"
#include "system/iblue_dcapi.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define SAME(got, want) do { checks++; if (strcmp((got), (want)) != 0) { fails++; printf("FAIL %s:%d:\n got:  %s\n want: %s\n", __FILE__, __LINE__, got, want); } } while (0)

void adapter_q_fb(struct raw_fb *fb_data) { (void)fb_data; }

static char ans[IBLUE_DCAPI_ANS_MAX + 1];
static const char *ask(const char *req) {
    iblue_dcapi_handle(req, strlen(req), ans, sizeof(ans));
    return ans;
}

static void game(const char *id) {
    uint8_t raw[IBLUE_GAMEID_LEN];
    memset(raw, 0, sizeof(raw));
    strncpy((char *)raw, id, sizeof(raw));
    iblue_gameid_set(raw);
}

int main(void) {
    CHECK(IBLUE_DCAPI_CMD == 0x22);
    CHECK(IBLUE_DCAPI_CAPS == 0x23);                         /* api, net, game */

    SAME(ask("dc1\nget hello\n"),
         "dc1\nok hello.kind adapter\nok hello.name iBlueControlMod\nok hello.version unknown\nok hello.api 1\nok hello.caps api,net,game\n");
    iblue_dcapi_init("v0.8-12-g1a2b3c4 dirty");
    SAME(ask("dc1\nid 17\nget hello.version\n"), "dc1\nid 17\nok hello.version v0.8-12-g1a2b3c4_dirty\n");

    /* what is not there yet */
    SAME(ask("dc1\nget game.id\nget net.selected\n"), "dc1\nerr game.id unavailable no game\nerr net.selected unavailable not set\n");
    SAME(ask("dc1\nget hello.api\nget net.*\n"), "dc1\nok hello.api 1\nerr net.* unknown\n");

    /* the game id comes from the Maple 0x21 path */
    game("MK51035");
    SAME(ask("dc1\nget game.id\n"), "dc1\nok game.id MK51035\n");
    game("");
    SAME(ask("dc1\nget game.id\n"), "dc1\nerr game.id unavailable no game\n");

    /* net.selected is written by whoever knows */
    SAME(ask("dc1\nset net.selected dcnet\nget net.selected\n"), "dc1\nok net.selected dcnet\nok net.selected dcnet\n");
    SAME(ask("dc1\nset net.selected dcnow\nget net.*\n"), "dc1\nok net.selected dcnow\nok net.selected dcnow\n");
    SAME(ask("dc1\nset net.selected pizza\nget net.selected\n"), "dc1\nerr net.selected range dcnow or dcnet\nok net.selected dcnow\n");
    game("DC_TEST");
    SAME(ask("dc1\nget *\n"), "dc1\nok hello.kind adapter\nok hello.name iBlueControlMod\nok hello.version v0.8-12-g1a2b3c4_dirty\nok hello.api 1\nok hello.caps api,net,game\nok game.id DC_TEST\nok net.selected dcnow\n");
    SAME(ask("dc1\nget game.*\nget hello.c*\n"), "dc1\nok game.id DC_TEST\nok hello.caps api,net,game\n");

    /* read only, unknown, and one bad line does not spoil the rest */
    SAME(ask("dc1\nset game.id X\nset hello.name x\nset port.A.mode dual\n"),
         "dc1\nerr game.id readonly\nerr hello.name readonly\nerr port.A.mode unknown\n");
    SAME(ask("dc1\nget port.A.mode\nfrobnicate x\ncall sys.reset\nget net.selected\n"),
         "dc1\nerr port.A.mode unknown\nerr frobnicate unknown\nerr sys.reset unknown\nok net.selected dcnow\n");
    SAME(ask("dc1\nget events since=0\n"), "dc1\nerr events unavailable not implemented\n");

    /* bad requests, blank lines, CRLF, padding */
    SAME(ask("get hello\n"), "dc1\nerr request bad first line must be dc1\n");
    SAME(ask(""), "dc1\nerr request bad first line must be dc1\n");
    SAME(ask("dc1\n"), "dc1\n");
    SAME(ask("dc1\r\n\r\nget net.selected\r\n\r\n"), "dc1\nok net.selected dcnow\n");
    {
        char padded[64];
        memset(padded, 0, sizeof(padded));
        strcpy(padded, "dc1\nget hello.api\n");
        iblue_dcapi_handle(padded, sizeof(padded), ans, sizeof(ans));      /* the Maple frame is NUL padded to whole words */
        SAME(ans, "dc1\nok hello.api 1\n");
    }

    /* who the request is for */
    SAME(ask("dc1\nto A4CF12B3\nget hello\n"), "dc1\nerr to unavailable not this device\n");
    CHECK(strstr(ask("dc1\nto iblue\nget hello.api\n"), "ok hello.api 1"));
    CHECK(strstr(ask("dc1\nto self\nget hello.api\n"), "ok hello.api 1"));

    /* an answer that does not fit is cut at a whole line, never in the middle */
    {
        char small[40];
        size_t n = iblue_dcapi_handle("dc1\nget hello\n", 14, small, sizeof(small));
        CHECK(n == strlen(small));
        CHECK(n > 0 && small[n - 1] == '\n');
        CHECK(n <= 39);
        CHECK(strncmp(small, "dc1\nok hello.kind adapter\n", 26) == 0);
        CHECK(iblue_dcapi_handle("dc1\nget hello\n", 14, small, 1) == 0);
    }
    /* more operations than fit are dropped, not crashed on */
    {
        char many[800] = "dc1\n";
        for (int i = 0; i < 30; i++) strcat(many, "get hello.api\n");
        CHECK(iblue_dcapi_handle(many, strlen(many), ans, sizeof(ans)) > 0);
    }

    /* the Maple bus: text word-reversed in both directions, padded to whole words */
    {
        const char *req = "dc1\nget hello.api\n";               /* 18 bytes */
        uint8_t mem[32], wire[32], back[IBLUE_DCAPI_ANS_MAX + 4];
        memset(mem, 0, sizeof(mem));
        memcpy(mem, req, strlen(req));
        for (int w = 0; w < 20; w += 4)
            for (int j = 0; j < 4; j++) wire[w + j] = mem[w + 3 - j];
        size_t words = iblue_dcapi_maple(wire, 20, back);
        CHECK(words == (strlen("dc1\nok hello.api 1\n") + 3) / 4);
        char text[IBLUE_DCAPI_ANS_MAX + 4];
        for (size_t w = 0; w < words * 4; w += 4)
            for (int j = 0; j < 4; j++) text[w + j] = (char)back[w + 3 - j];
        text[words * 4] = 0;
        SAME(text, "dc1\nok hello.api 1\n");
        CHECK(iblue_dcapi_maple(wire, 0, back) == (strlen("dc1\nerr request bad first line must be dc1\n") + 3) / 4);   /* an empty request: the "first line" error, whole words */
    }
    /* a request of any length cannot overrun the buffers */
    {
        uint8_t wire[600], back[IBLUE_DCAPI_ANS_MAX + 4];
        memset(wire, 'a', sizeof(wire));
        CHECK(iblue_dcapi_maple(wire, sizeof(wire), back) <= (IBLUE_DCAPI_ANS_MAX + 3) / 4);
    }

    /* the hello bits behind the marker in the extended device info */
    {
        uint8_t wire[24], mem[24];
        iblue_dcapi_ext_caps(wire);
        for (int w = 0; w < 24; w += 4)
            for (int j = 0; j < 4; j++) mem[w + j] = wire[w + 3 - j];
        CHECK(memcmp(mem, "dc1 00000023", 12) == 0);
        CHECK(mem[12] == 0 && mem[23] == 0);
    }

    /* BLE: chunks of 22 bytes in, chunks of 22 bytes out */
    {
        const char *req = "dc1\nget hello\nget net.selected\n";
        size_t rl = strlen(req), sent = 0;
        uint8_t chunk[23], rd[23];
        char got[IBLUE_DCAPI_ANS_MAX + 1];
        size_t gl = 0;
        while (sent < rl) {
            size_t n = rl - sent > 10 ? 10 : rl - sent;
            chunk[0] = (uint8_t)((sent == 0 ? 1 : 0) | (sent + n == rl ? 2 : 0));
            memcpy(chunk + 1, req + sent, n);
            iblue_dcapi_ble_write(chunk, n + 1);
            sent += n;
        }
        for (int i = 0; i < 40; i++) {
            size_t n = iblue_dcapi_ble_read(rd, 23);
            CHECK(n >= 1 && n <= 23);
            memcpy(got + gl, rd + 1, n - 1);
            gl += n - 1;
            if (!rd[0]) break;
        }
        got[gl] = 0;
        SAME(got, ask(req));
        CHECK(iblue_dcapi_ble_read(rd, 23) == 1 && rd[0] == 0);    /* read past the end: empty, last */
        CHECK(iblue_dcapi_ble_read(rd, 1) == 0);
    }

    printf("dcapi: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
