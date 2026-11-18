/*
 * brctl_test - exercise the BlueRetro Maple control channel (KallistiOS)
 *
 * Shows adapter info + live status on screen and maps controller buttons:
 *   A      LED preset: orange -> blue -> custom
 *   B      start pairing (60 s window)
 *   X      LED colour wheel step (not saved)
 *   Y      save the current LED colour as custom
 *   START  read the full BlueRetro config and show a summary
 *   L+R    toggle "openMenu owns the tap": tap = report only / default
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <kos.h>
#include <dc/biosfont.h>
#include <dc/maple/controller.h>
#include "brctl.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

static int line;

static void clear(void) {
    memset(vram_s, 0, 640 * 480 * 2);
    line = 0;
}

static void say(const char *fmt, ...) {
    char buf[96];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    printf("%s\n", buf);
    if (line < 18) {
        bfont_draw_str((void *)(vram_s + (24 + line * 24) * 640 + 24), 640, 1, buf);
        line++;
    }
}

static void hue(uint32_t h, uint8_t *r, uint8_t *g, uint8_t *b) {
    uint32_t seg = (h / 256) % 6, x = h % 256;
    *r = *g = *b = 0;
    switch (seg) {
        case 0: *r = 255; *g = x; break;
        case 1: *r = 255 - x; *g = 255; break;
        case 2: *g = 255; *b = x; break;
        case 3: *g = 255 - x; *b = 255; break;
        case 4: *r = x; *b = 255; break;
        default: *r = 255; *b = 255 - x; break;
    }
}

static void show(int port, const struct brctl_info *info) {
    struct brctl_status st;
    int ret;

    clear();
    say("BlueRetro brctl test  -  port %c", 'A' + port);
    say("FW %s (%s)", info->fw_version, info->fw_name);
    say("proto %u  config %u bytes  features %02x",
        info->proto_ver, info->config_size, info->features);
    ret = brctl_get_status(port, &st);
    if (ret) {
        say("status read failed: %d", ret);
        return;
    }
    say("seq %lu  pairing %s  window %us  console %s",
        (unsigned long)st.seq, (st.flags & BRCTL_ST_PAIRING) ? "ON" : "off",
        st.pair_window_s, (st.flags & BRCTL_ST_CONSOLE_ON) ? "on" : "off");
    say("LED #%02X%02X%02X preset %u", st.led_rgb[0], st.led_rgb[1], st.led_rgb[2], st.led_preset);
    {
        say("tap actions: single %u, double %u, triple %u", st.btn_map[0], st.btn_map[1], st.btn_map[2]);
    }
    for (int i = 0; i < BRCTL_PORTS; i++) {
        if (st.port_bt_id[i] == 0xFF) {
            say("  port %c: -", 'A' + i);
        }
        else {
            say("  port %c: BT dev %u type %u sub %u", 'A' + i,
                st.port_bt_id[i], st.port_bt_type[i], st.port_bt_subtype[i]);
        }
    }
    say("");
    say("A preset  B pair  X colour wheel  Y save  START config");
}

int main(int argc, char **argv) {
    struct brctl_info info;
    uint32_t prev_btns = 0, h = 0;
    int port = -1;

    vid_set_mode(DM_640x480, PM_RGB565);
    clear();
    say("Looking for a BlueRetro adapter...");

    while (port < 0) {
        port = brctl_find();
        if (port < 0) {
            say("Not found: connect a Bluetooth pad, retrying");
            thd_sleep(1000);
        }
    }
    if (brctl_get_info(port, &info)) {
        say("info read failed");
        return 1;
    }

    for (;;) {
        maple_device_t *cont = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *s = cont ? (cont_state_t *)maple_dev_status(cont) : NULL;
        uint32_t btns = s ? s->buttons : 0;
        uint32_t pressed = btns & ~prev_btns;

        prev_btns = btns;
        if (pressed & CONT_A) {
            struct brctl_status st;
            if (!brctl_get_status(port, &st)) {
                brctl_led_preset(port, (st.led_preset + 1) % 3);
            }
        }
        if (pressed & CONT_B) {
            brctl_pair_start(port);
        }
        if (pressed & CONT_X) {
            uint8_t r, g, b;
            h = (h + 64) % 1536;
            hue(h, &r, &g, &b);
            brctl_led_rgb(port, r, g, b, 0);
        }
        if (pressed & CONT_Y) {
            struct brctl_status st;
            if (!brctl_get_status(port, &st)) {
                brctl_led_rgb(port, st.led_rgb[0], st.led_rgb[1], st.led_rgb[2], 1);
            }
        }
        if (s && s->ltrig > 200 && s->rtrig > 200) {
            /* demo: hand the single tap over to this program (not persisted) */
            static int owned;
            owned = !owned;
            brctl_btn_set(port, BRCTL_GEST_TAP, BRCTL_ACT_NONE, 0);
            thd_sleep(500);
        }
        if (pressed & CONT_START) {
            static struct br_config cfg;
            int ret = brctl_config_load(port, &cfg);
            clear();
            if (ret) {
                say("config load failed: %d", ret);
            }
            else {
                say("magic %08lx system %u inquiry %s banksel %u",
                    (unsigned long)cfg.magic, cfg.global_cfg.system_cfg,
                    cfg.global_cfg.inquiry_mode ? "manual" : "auto",
                    cfg.global_cfg.banksel);
                for (int i = 0; i < 4; i++) {
                    say("out %c: dev_mode %u acc_mode %u  in: %u mappings", 'A' + i,
                        cfg.out_cfg[i].dev_mode, cfg.out_cfg[i].acc_mode,
                        cfg.in_cfg[i].map_size);
                }
            }
            thd_sleep(3000);
        }

        show(port, &info);
        thd_sleep(250);
    }
    return 0;
}
