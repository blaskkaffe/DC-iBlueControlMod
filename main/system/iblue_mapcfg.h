/*
 * iBlueControlMod: the shared text form of a controller mapping (docs in DC-UnifiedEcosystem: 07-universal-controller-config.md,
 * decisions D-17 and D-18). Portable C: no ESP-IDF calls, no libc formatting, so the DC API engine can use it inside the Maple
 * interrupt and the host tests can compile it.
 *
 * One line per Dreamcast OUTPUT, the INPUTS (XInput names) that drive it, options after a colon:
 *
 *    A      = a
 *    Z      = lb
 *    C      = rb , lt:threshold=78
 *    A1_UP  = lstick.up:deadzone=2
 *    D      = none                        (no input drives D)
 *
 * Inputs (lower case): a b x y lb rb lt rt back start guide share, lstick.up|down|left|right|click, rstick.* , dpad.*, dpad2.*
 *   (Pico2Maple's upper case names A1_UP, A2_LEFT, DPAD_DOWN, SELECT are read as aliases.)
 * Outputs (upper case): A B X Y C D Z START LT RT A1_UP|DOWN|LEFT|RIGHT A2_* DPAD_* DPAD2_*
 * Options: deadzone=off|1..5|<n>%   threshold=0..100   scale=0..255   turbo=0..255   algo=linear|aggressive|relaxed|wide|scurve|passthrough
 * At most IBLUE_MAP_PER_OUT inputs per output (Pico2Maple's limit, so both firmwares can hold any shared mapping).
 *
 * A mapping entry is BlueRetro's struct map_cfg (config.h), byte for byte: struct iblue_mapent has the same layout and the firmware
 * glue asserts it. Entries that are not part of the pad (keyboard / mouse sources, combos, other ports) are never touched.
 *
 * Deadzone levels (decision, 2026-11-05): off, 1 = BlueRetro's 1.35 %, 2 = 5 %, 3 = 10 %, 4 = 15 %, 5 = 20 %. BlueRetro's byte
 * perc_deadzone held hundredths of a percent (0..255 = up to 2.55 %). Values up to 200 keep that meaning, so existing configurations
 * are unchanged; 201 to 218 now mean (value - 198) percent (3 % to 20 %; more is never needed), higher values count as 20 %: iblue_dz_x10000() is the one place that knows, adapter.c uses it.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_MAPCFG_H_
#define _IBLUE_MAPCFG_H_

#include <stddef.h>
#include <stdint.h>

#define IBLUE_MAP_PER_OUT 4
#define IBLUE_MAP_MAX 128             /* ADAPTER_MAPPING_MAX */

struct iblue_mapent {                 /* == struct map_cfg */
    uint8_t src_btn;
    uint8_t dst_btn;
    uint8_t dst_id;
    uint8_t perc_max;
    uint8_t perc_threshold;
    uint8_t perc_deadzone;
    uint8_t turbo;
    uint8_t algo;
} __attribute__((packed));

/* BlueRetro's generic button ids that the pad uses (== PAD_* in adapter.h) */
enum {
    IBM_LX_LEFT = 0, IBM_LX_RIGHT, IBM_LY_DOWN, IBM_LY_UP, IBM_RX_LEFT, IBM_RX_RIGHT, IBM_RY_DOWN, IBM_RY_UP,
    IBM_LD_LEFT, IBM_LD_RIGHT, IBM_LD_DOWN, IBM_LD_UP, IBM_RD_LEFT, IBM_RD_RIGHT, IBM_RD_DOWN, IBM_RD_UP,
    IBM_RB_LEFT, IBM_RB_RIGHT, IBM_RB_DOWN, IBM_RB_UP, IBM_MM, IBM_MS, IBM_MT, IBM_MQ,
    IBM_LM, IBM_LS, IBM_LT, IBM_LJ, IBM_RM, IBM_RS, IBM_RT, IBM_RJ,
    IBM_PAD_IDS
};

/* The number of outputs, and the i-th output's name and BlueRetro destination id (0 .. IBLUE_OUT_COUNT - 1, in listing order) */
#define IBLUE_OUT_COUNT 26
const char *iblue_out_name(uint32_t i);
uint8_t iblue_out_dst(uint32_t i);
int iblue_out_find(const char *s, size_t n);          /* index of the output, case insensitive, or -1 */
const char *iblue_in_name(uint8_t src);               /* NULL: that id has no name */

/* deadzone */
uint8_t iblue_dz_from_level(int level);               /* 0 = off, 1..5; 135 if out of range */
int iblue_dz_level(uint8_t raw);                      /* 0..5, or -1 when raw is not exactly a level */
uint32_t iblue_dz_x10000(uint8_t raw);                /* the fraction of the axis range that is dead, times 10000 */

/* A mapping value (the text right of "=") into entries for the output. Returns the number of entries (0 = "none"), or -1 and *why. */
int iblue_map_parse(const char *val, size_t len, uint32_t out, uint8_t port, struct iblue_mapent *ents, const char **why);

/* The value text of one output from the entries of a port. Returns the length written (NUL terminated, cut at an entry if cap is too small). */
/* How many entries the table has after iblue_map_replace() of this output with nents entries (what the engine checks against the size of the table before it says ok). */
size_t iblue_map_size_after(const struct iblue_mapent *map, size_t n, uint8_t port, uint32_t out, size_t nents);
size_t iblue_map_render(const struct iblue_mapent *map, size_t n, uint8_t port, uint32_t out, char *buf, size_t cap);
/* The same, and *cut = 1 when the value did not fit (what is in buf is then only the first entries, or empty): a caller that must not
 * show a wrong mapping drops the line. */
size_t iblue_map_render_ex(const struct iblue_mapent *map, size_t n, uint8_t port, uint32_t out, char *buf, size_t cap, int *cut);

/* Replace what drives the output by ents. Returns the new size, or (size_t)-1 when the table would overflow (nothing changed). */
size_t iblue_map_replace(struct iblue_mapent *map, size_t n, size_t max, uint8_t port, uint32_t out, const struct iblue_mapent *ents, size_t nents);

/* Layouts: named, complete presets (every pad output). "custom" is not a preset: it is what a port has when it matches none. */
#define IBLUE_LAYOUT_COUNT 2
const char *iblue_layout_name(uint32_t i);            /* "standard", "all" */
int iblue_layout_find(const char *s, size_t n);       /* index, or -1 */
size_t iblue_layout_apply(uint32_t layout, struct iblue_mapent *map, size_t n, size_t max, uint8_t port); /* new size or (size_t)-1 */
int iblue_layout_match(const struct iblue_mapent *map, size_t n, uint8_t port); /* index of the preset the port equals (entry by entry), or -1 (custom) */

#endif /* _IBLUE_MAPCFG_H_ */
