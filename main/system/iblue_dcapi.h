/*
 * iBlueControlMod: the adapter's end of the DC API, language "dc1" (the shared design is 04-control-channel.md in
 * https://github.com/blaskkaffe/DC-UnifiedEcosystem; this file is what is built). A request is a few lines of text, the
 * answer a few lines of text:
 *
 *    dc1                        first line, required
 *    id 17                      optional, echoed
 *    get hello | get <key> | get <prefix>* | set <key> <value> | call <action> [arguments]
 *
 *    answer: dc1, the echoed id, then "ok <key> <value>" or "err <key> <code> [why]" per answered key.
 *
 * Keys this adapter has (capabilities api, net, port, game; the other keys of the design come with the code behind them):
 *    hello.kind adapter, hello.name iBlueControlMod, hello.version <git describe>, hello.api 1, hello.caps api,net,port,game
 *    port.<A-D>.state  empty | bt | wired   (wired: the sense line on boards that have it, or the peer hint below)
 *    port.<A-D>.mode   normal | dual | keyboard | mouse   (the configured output mode; read only for now)
 *    port.<A-D>.acc    none | vmu | rumble | both          (the configured accessory; read only for now)
 *    port.<A-D>.name   what the Dreamcast sees on the port (read only)
 *    port.<A-D>.peer   the device openMenu found on a wired port ("pico2maple" ...): written by the one who knows, not stored,
 *                      "none" clears it; a port with a peer reads as wired
 *    game.id        read only: the game id openMenu sent (Maple 0x21), absent when no game
 *    net.selected   dcnow | dcnet: set by whoever knows (the Pi over BLE, openMenu over Maple), read back; not stored
 *    events         not implemented here (err events unavailable)
 *
 * Three ways in, one engine:
 *    BLE (att_cfg.c, command 0x51, feature bit 6): iblue_dcapi_ble_write() / iblue_dcapi_ble_read(), chunked because one
 *       read answers at most 23 bytes.
 *    Maple (maple.c, command 0x22 to the memory card unit): iblue_dcapi_maple(); the request text and the answer travel
 *       word-reversed like all text on the bus. The request is answered in the same exchange (data frame with the answer).
 *    The extended device info carries "dc1 " and the capability bits as 8 hex digits behind the 16 byte marker, so
 *       a client learns that the device speaks the language, and what it offers, from a request it makes anyway.
 * The engine runs in the Maple interrupt: no flash access, no libc formatting, all its text in RAM (DRAM_ATTR).
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_DCAPI_H_
#define _IBLUE_DCAPI_H_

#include <stddef.h>
#include <stdint.h>
#include "sdkconfig.h"

#define IBLUE_DCAPI_CMD 0x22          /* the Maple command (decision D-15 in DC-UnifiedEcosystem) */
#define IBLUE_DCAPI_REQ_MAX 252       /* bytes of request text (63 words) */
#define IBLUE_DCAPI_ANS_MAX 400       /* bytes of answer text; a longer answer is cut at a line end */
#define IBLUE_DCAPI_CAP_API (1u << 0)
#define IBLUE_DCAPI_CAP_NET (1u << 1)
#define IBLUE_DCAPI_CAP_PORT (1u << 3)
#define IBLUE_DCAPI_CAP_GAME (1u << 5)
#define IBLUE_DCAPI_CAPS (IBLUE_DCAPI_CAP_API | IBLUE_DCAPI_CAP_NET | IBLUE_DCAPI_CAP_PORT | IBLUE_DCAPI_CAP_GAME)
#define IBLUE_DCAPI_PORTS 4           /* ports A to D */
/* port states, output modes and accessory modes use the numbers of adapter.h (DEV_*, ACC_*): 0 empty, 1 Bluetooth, 2 wired */
#define IBLUE_PORT_EMPTY 0
#define IBLUE_PORT_BT 1
#define IBLUE_PORT_WIRED 2

#ifdef CONFIG_BLUERETRO_IBLUE_DCAPI

/* The version text for hello.version (copied; call once at start, from normal code). */
void iblue_dcapi_init(const char *version);

/* The engine: req is text (NUL or the length ends it); the answer is NUL terminated in out, its length is returned. */
size_t iblue_dcapi_handle(const char *req, size_t req_len, char *out, size_t out_cap);

/* Maple: wire = the request text as received (word-reversed), n bytes. Writes the answer word-reversed and NUL padded to
 * whole words into out_wire (which holds IBLUE_DCAPI_ANS_MAX bytes) and returns the number of words. */
size_t iblue_dcapi_maple(const uint8_t *wire, size_t n, uint8_t *out_wire);

/* The 24 bytes after the 16 byte marker in the extended device info: "dc1 " + caps as 8 hex digits, NUL padded, word-reversed. */
void iblue_dcapi_ext_caps(uint8_t *wire24);

/* BLE: write the chunks of a request (first byte: bit0 = first chunk, bit1 = last chunk; then up to max-1 bytes of text).
 * After the last chunk the request is answered; read returns the answer: first byte 1 = more follows (read again), 0 = last. */
/* The ports, from normal code (manager.c, every time it maps Bluetooth controllers to ports): count of ports the board has and,
 * per port, the state (IBLUE_PORT_*), the configured output mode and accessory mode, and the Bluetooth type / subtype. */
void iblue_dcapi_port_count(uint32_t count);
void iblue_dcapi_port_set(uint32_t port, uint8_t state, uint8_t dev_mode, uint8_t acc_mode, uint8_t bt_type, uint8_t bt_subtype);
/* BLE GET_PORT_STATUS (0x50): [count, then per port: state, dev_mode, acc_mode, bt type, bt subtype]; returns the length */
size_t iblue_dcapi_port_status(uint8_t *out);

void iblue_dcapi_ble_write(const uint8_t *data, size_t len);
size_t iblue_dcapi_ble_read(uint8_t *out, size_t max);

#else
#define iblue_dcapi_init(version)
#define iblue_dcapi_ext_caps(wire24)
#define iblue_dcapi_port_count(count)
#define iblue_dcapi_port_set(port, state, dev_mode, acc_mode, bt_type, bt_subtype)
#endif

#endif /* _IBLUE_DCAPI_H_ */
