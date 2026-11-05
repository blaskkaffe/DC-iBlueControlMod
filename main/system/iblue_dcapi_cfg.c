/* iBlueControlMod: the DC API changes the configuration (normal code, not the Maple interrupt). See iblue_dcapi.h. */

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <esp_attr.h>
#include <esp_cpu.h>
#include "queue_bss.h"
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

/* A mapping change is too big for one queue message (13 bytes), so the entries wait in a mailbox slot and ONE message (with the slot
 * number) tells the Bluetooth task to apply them. A request holds up to 8 operations, so there are 8 slots. A slot is taken with an atomic
 * exchange because the Maple interrupt and the Bluetooth task both ask; whoever finds all taken gets "busy". If the message cannot be
 * queued the slot is freed again. */
#define MAILBOX_SLOTS 8
/* one queue per producer: core 1 = the Maple side, core 0 = the BLE side (the queue holds 15 usable items) */
static queue_bss_handle_t cfgq[2];

void iblue_dcapi_cfg_init(void) {
    for (int i = 0; i < 2; i++) {
        cfgq[i] = queue_bss_init(16, sizeof(struct raw_fb));
    }
}

static int IRAM_ATTR cfg_send(struct raw_fb *fb) {
    queue_bss_handle_t q = cfgq[esp_cpu_get_core_id() ? 1 : 0];

    return q && queue_bss_enqueue(q, (uint8_t *)fb, sizeof(*fb)) >= 0;
}

static struct {
    uint32_t taken;
    uint8_t n;
    struct iblue_mapent ents[IBLUE_MAP_PER_OUT];
} mailbox[MAILBOX_SLOTS];

int IRAM_ATTR iblue_dcapi_cfg_set_map(uint8_t port, uint8_t out, const struct iblue_mapent *ents, size_t n) {
    struct raw_fb fb = {0};
    uint32_t s = 0;

    if (n > IBLUE_MAP_PER_OUT) {
        return 0;
    }
    while (s < MAILBOX_SLOTS && __atomic_exchange_n(&mailbox[s].taken, 1, __ATOMIC_ACQ_REL)) {
        s++;
    }
    if (s == MAILBOX_SLOTS) {
        return 0;
    }
    mailbox[s].n = (uint8_t)n;
    for (size_t i = 0; i < n; i++) {
        mailbox[s].ents[i] = ents[i];
    }
    fb.header.type = FB_TYPE_API_CFG;
    fb.header.data_len = 4;
    fb.data[0] = IBLUE_CFG_MAP_SET;
    fb.data[1] = port;
    fb.data[2] = out;
    fb.data[3] = (uint8_t)s;
    if (!cfg_send(&fb)) {
        __atomic_store_n(&mailbox[s].taken, 0, __ATOMIC_RELEASE);
        return 0;
    }
    return 1;
}

/* Asks; callable from the Maple interrupt (so it is in IRAM, like iblue_gameid_set()) and from the Bluetooth task: a queued message */
int IRAM_ATTR iblue_dcapi_cfg_request(uint8_t op, uint8_t a, uint8_t b) {
    struct raw_fb fb = {0};

    fb.header.type = FB_TYPE_API_CFG;
    fb.header.data_len = 3;
    fb.data[0] = op;
    fb.data[1] = a;
    fb.data[2] = b;
    return cfg_send(&fb);
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
        case IBLUE_CFG_MAP_SET:
        {
            uint8_t s = fb->data[3];

            if (s >= MAILBOX_SLOTS) {
                return;
            }
            if (a < WIRED_MAX_DEV && b < IBLUE_OUT_COUNT) {
                size_t r = iblue_map_replace((struct iblue_mapent *)config.in_cfg[a].map_cfg, config.in_cfg[a].map_size,
                                             ADAPTER_MAPPING_MAX, a, b, mailbox[s].ents, mailbox[s].n);

                if (r != (size_t)-1) {
                    config.in_cfg[a].map_size = (uint8_t)r;
                }
            }
            __atomic_store_n(&mailbox[s].taken, 0, __ATOMIC_RELEASE);
            break;
        }
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

void iblue_dcapi_cfg_poll(void) {
    for (int i = 0; i < 2; i++) {
        uint32_t *len;
        uint8_t *item;

        while (cfgq[i] && (item = queue_bss_dequeue(cfgq[i], &len))) {
            iblue_dcapi_cfg_apply((const struct raw_fb *)item);
            queue_bss_return(cfgq[i], item, len);
        }
    }
}

#endif /* CONFIG_BLUERETRO_IBLUE_DCAPI */
