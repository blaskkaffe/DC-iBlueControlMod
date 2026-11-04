/* iBlueControlMod: the DC API changes the configuration (normal code, not the Maple interrupt). See iblue_dcapi.h. */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <esp_attr.h>
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "system/manager.h"
#include "system/iblue_dcapi.h"

#ifdef CONFIG_BLUERETRO_IBLUE_DCAPI

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
