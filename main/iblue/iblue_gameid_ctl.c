/* iBlueControlMod: keeps the game id on/off switch over restarts. See iblue_gameid.h. */

#include <stdio.h>
#include <nvs.h>
#include "iblue/iblue_gameid.h"
#include "iblue/iblue_gameid_ctl.h"

#ifdef CONFIG_BLUERETRO_IBLUE_GAMEID

#define NVS_NS "iblue"
#define NVS_KEY "gid_off"

void iblue_gameid_ctl_init(void) {
    nvs_handle_t nvs;
    uint8_t off = 0;

    if (nvs_open(NVS_NS, NVS_READONLY, &nvs) == ESP_OK) {
        nvs_get_u8(nvs, NVS_KEY, &off);
        nvs_close(nvs);
    }
    iblue_gameid_set_enabled(!off);
}

int iblue_gameid_ctl_toggle(void) {
    nvs_handle_t nvs;
    int on = !iblue_gameid_enabled();

    iblue_gameid_set_enabled(on);
    if (nvs_open(NVS_NS, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u8(nvs, NVS_KEY, on ? 0 : 1);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
    printf("# %s: game id %s\n", __FUNCTION__, on ? "on" : "off");
    return on;
}

#endif
