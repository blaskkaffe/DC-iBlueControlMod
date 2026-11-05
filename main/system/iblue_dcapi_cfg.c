/* iBlueControlMod: the DC API changes the configuration (normal code, not the Maple interrupt). See iblue_dcapi.h. */

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <esp_attr.h>
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "system/manager.h"
#include "system/iblue_dcapi.h"

#ifdef CONFIG_BLUERETRO_IBLUE_DCAPI

/* iblue_mapcfg.c works on BlueRetro's struct map_cfg through its own, identical type */
_Static_assert(sizeof(struct iblue_mapent) == sizeof(struct map_cfg), "iblue_mapent must be struct map_cfg");
_Static_assert(offsetof(struct iblue_mapent, dst_id) == offsetof(struct map_cfg, dst_id), "map_cfg layout");
_Static_assert(offsetof(struct iblue_mapent, perc_deadzone) == offsetof(struct map_cfg, perc_deadzone), "map_cfg layout");
_Static_assert(offsetof(struct iblue_mapent, algo) == offsetof(struct map_cfg, algo), "map_cfg layout");
_Static_assert(IBLUE_MAP_MAX == ADAPTER_MAPPING_MAX, "mapping table size");
#define SAME_ID(a, b) ((int)(a) == (int)(b))
_Static_assert(SAME_ID(IBM_LX_LEFT, PAD_LX_LEFT) && SAME_ID(IBM_RD_UP, PAD_RD_UP) && SAME_ID(IBM_RB_DOWN, PAD_RB_DOWN) &&
               SAME_ID(IBM_MM, PAD_MM) && SAME_ID(IBM_MS, PAD_MS) && SAME_ID(IBM_LM, PAD_LM) && SAME_ID(IBM_LS, PAD_LS) &&
               SAME_ID(IBM_LJ, PAD_LJ) && SAME_ID(IBM_RM, PAD_RM) && SAME_ID(IBM_RS, PAD_RS) && SAME_ID(IBM_RJ, PAD_RJ), "generic button ids");

/* The live mapping of a port, for the engine (read only) */
const struct iblue_mapent * IRAM_ATTR iblue_dcapi_cfg_map(uint32_t port, size_t *count) {
    if (port >= WIRED_MAX_DEV) {
        *count = 0;
        return NULL;
    }
    *count = config.in_cfg[port].map_size;
    return (const struct iblue_mapent *)config.in_cfg[port].map_cfg;
}

/* One entry of a mapping change: the message carries port, output index and the entry (9 bytes of the 13) */
void IRAM_ATTR iblue_dcapi_cfg_request_map(uint8_t port, uint8_t out, const struct iblue_mapent *e) {
    struct raw_fb fb = {0};

    fb.header.type = FB_TYPE_API_CFG;
    fb.header.data_len = 9;
    fb.data[0] = IBLUE_CFG_MAP_ADD;
    fb.data[1] = port;
    fb.data[2] = out;
    fb.data[3] = e->src_btn;
    fb.data[4] = e->perc_max;
    fb.data[5] = e->perc_threshold;
    fb.data[6] = e->perc_deadzone;
    fb.data[7] = e->turbo;
    fb.data[8] = e->algo;
    adapter_q_fb(&fb);
}

/* Asks; callable from the Maple interrupt (so it is in IRAM, like iblue_gameid_set()) and from the Bluetooth task: a queued message */
void IRAM_ATTR iblue_dcapi_cfg_request(uint8_t op, uint8_t a, uint8_t b) {
    struct raw_fb fb = {0};

    fb.header.type = FB_TYPE_API_CFG;
    fb.header.data_len = 3;
    fb.data[0] = op;
    fb.data[1] = a;
    fb.data[2] = b;
    adapter_q_fb(&fb);
}

/* Does it; called by the Bluetooth task (host.c) when the message comes out of the queue. The live configuration is `config`: the
 * Maple side reads it for every frame, but the console only looks at what a port is when it enumerates it, so a change of output
 * or accessory mode needs the wired side to start again (the console sees the controllers leave and come back). */
void iblue_dcapi_cfg_apply(const struct raw_fb *fb) {
    uint8_t op = fb->data[0];
    uint8_t a = fb->data[1];
    uint8_t b = fb->data[2];
    bool reset = false;

    switch (op) {
        case IBLUE_CFG_SET_MODE:
            if (a < WIRED_MAX_DEV && b <= DEV_MOUSE && config.out_cfg[a].dev_mode != b) {
                config.out_cfg[a].dev_mode = b;
                reset = true;
            }
            break;
        case IBLUE_CFG_SET_ACC:
            if (a < WIRED_MAX_DEV && b <= ACC_BOTH && config.out_cfg[a].acc_mode != b) {
                config.out_cfg[a].acc_mode = b;
                reset = true;
            }
            break;
        case IBLUE_CFG_MAP_CLEAR:
            if (a < WIRED_MAX_DEV && b < IBLUE_OUT_COUNT) {
                size_t r = iblue_map_replace((struct iblue_mapent *)config.in_cfg[a].map_cfg, config.in_cfg[a].map_size,
                                             ADAPTER_MAPPING_MAX, a, b, NULL, 0);

                if (r != (size_t)-1) {
                    config.in_cfg[a].map_size = (uint8_t)r;
                }
            }
            break;
        case IBLUE_CFG_MAP_ADD:
            if (a < WIRED_MAX_DEV && b < IBLUE_OUT_COUNT && fb->data[3] < IBM_PAD_IDS && config.in_cfg[a].map_size < ADAPTER_MAPPING_MAX) {
                struct iblue_mapent e = {fb->data[3], iblue_out_dst(b), a, fb->data[4], fb->data[5], fb->data[6], fb->data[7], fb->data[8]};
                struct iblue_mapent *m = (struct iblue_mapent *)config.in_cfg[a].map_cfg;

                m[config.in_cfg[a].map_size++] = e;
            }
            break;
        case IBLUE_CFG_SET_LAYOUT:
            if (a < WIRED_MAX_DEV && b < IBLUE_LAYOUT_COUNT) {
                size_t r = iblue_layout_apply(b, (struct iblue_mapent *)config.in_cfg[a].map_cfg, config.in_cfg[a].map_size,
                                              ADAPTER_MAPPING_MAX, a);

                if (r != (size_t)-1) {
                    config.in_cfg[a].map_size = (uint8_t)r;
                }
            }
            break;
        case IBLUE_CFG_COMMIT:
            config_update(config_get_src());
            break;
        case IBLUE_CFG_REVERT:
        {
            struct out_cfg before[WIRED_MAX_DEV];

            memcpy(before, config.out_cfg, sizeof(before));
            config_init(config_get_src());
            reset = memcmp(before, config.out_cfg, sizeof(before)) != 0;
            break;
        }
        default:
            return;
    }
    printf("# %s: op %u port %u value %u%s\n", __FUNCTION__, (unsigned)op, (unsigned)a, (unsigned)b, reset ? " (wired restart)" : "");
    if (reset) {
        sys_mgr_cmd(SYS_MGR_CMD_WIRED_RST);
    }
}

#endif /* CONFIG_BLUERETRO_IBLUE_DCAPI */
