/* iBlueControlMod: the settings of the boot animation and of the port LEDs. See iblue_anim_cfg.h. */

#include <string.h>
#include <nvs.h>
#include "iblue/iblue_anim_cfg.h"

#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED

#define NVS_NS "iblue"
#define NVS_KEY "anim"
#define BLOB_VERSION 1 /* first byte of the blob; then the fields, 16 bit little endian, in the order of the struct */

void iblue_anim_cfg_init(void) {
    nvs_handle_t nvs;
    uint8_t blob[1 + 2 * IBLUE_ANIM_CFG_FIELDS];
    size_t len = sizeof(blob);
    struct iblue_anim_cfg c = iblue_anim_cfg_default;

    if (nvs_open(NVS_NS, NVS_READONLY, &nvs) == ESP_OK) {
        if (nvs_get_blob(nvs, NVS_KEY, blob, &len) == ESP_OK && len == sizeof(blob) && blob[0] == BLOB_VERSION) {
            int16_t *v = (int16_t *)&c;

            for (uint32_t i = 0; i < IBLUE_ANIM_CFG_FIELDS; i++) {
                v[i] = (int16_t)((uint16_t)blob[1 + 2 * i] | ((uint16_t)blob[2 + 2 * i] << 8));
            }
        }
        nvs_close(nvs);
    }
    iblue_anim_set_cfg(&c); /* clamps */
}

void iblue_anim_cfg_save(const struct iblue_anim_cfg *c) {
    nvs_handle_t nvs;
    uint8_t blob[1 + 2 * IBLUE_ANIM_CFG_FIELDS];
    const int16_t *v;

    iblue_anim_set_cfg(c);
    v = (const int16_t *)iblue_anim_get_cfg(); /* what was kept: clamped */
    blob[0] = BLOB_VERSION;
    for (uint32_t i = 0; i < IBLUE_ANIM_CFG_FIELDS; i++) {
        blob[1 + 2 * i] = (uint8_t)((uint16_t)v[i] & 0xFF);
        blob[2 + 2 * i] = (uint8_t)((uint16_t)v[i] >> 8);
    }
    if (nvs_open(NVS_NS, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_blob(nvs, NVS_KEY, blob, sizeof(blob));
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

void iblue_anim_cfg_reset(void) {
    iblue_anim_cfg_save(&iblue_anim_cfg_default);
}

#endif /* CONFIG_BLUERETRO_IBLUE_RGB_LED */
