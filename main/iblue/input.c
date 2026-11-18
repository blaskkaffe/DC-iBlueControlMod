/* iBlueControlMod: see input.h. */

#include <esp_attr.h>
#include <esp_timer.h>
#include "zephyr/types.h"
#include "tools/util.h"
#include "adapter/adapter.h"
#include "iblue/brctl.h"
#include "iblue/input.h"

#ifdef CONFIG_BLUERETRO_BRCTL

#define LIVE_PORTS BRCTL_PORTS
#define LIVE_STALE_US 500000

static DRAM_ATTR volatile uint32_t live[LIVE_PORTS];
static DRAM_ATTR volatile int64_t live_us[LIVE_PORTS];

void iblue_input(uint8_t out_idx, struct wireless_ctrl *ctrl) {
    uint32_t w;

    if (out_idx >= LIVE_PORTS || ctrl == NULL) {
        return;
    }
    w = (uint32_t)ctrl->btns[0].value;
    /* sticks and triggers count as pressed past half travel */
    for (uint32_t a = 0; a < ADAPTER_MAX_AXES; a++) {
        const struct ctrl_meta *meta = ctrl->axes[a].meta;
        uint32_t neg = axis_to_btn_id(a);
        int32_t v = ctrl->axes[a].value;

        if (meta == NULL || meta->abs_max == 0) {
            continue;
        }
        if (a >= TRIG_L && a <= TRIG_R) {
            if (v > meta->abs_max / 2) {
                w |= BIT(neg & 0x1F);
            }
        }
        else if (v > meta->abs_max / 2 || v < -(meta->abs_max / 2)) {
            uint32_t pos = neg + 1;
            uint32_t id = ((v > 0) == (btn_sign(meta->polarity, pos) > 0)) ? pos : neg;

            w |= BIT(id & 0x1F);
        }
    }
    live[out_idx] = w;
    live_us[out_idx] = esp_timer_get_time();
}

uint32_t iblue_input_live(uint8_t port) {
    if (port >= LIVE_PORTS || esp_timer_get_time() - live_us[port] > LIVE_STALE_US) {
        return 0;
    }
    return live[port];
}

#endif /* CONFIG_BLUERETRO_BRCTL */
