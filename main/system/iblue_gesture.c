/* iBlueControlMod: what the taps do. See iblue_gesture.h. */

#include <stdio.h>
#include <string.h>
#include <nvs.h>
#include "bluetooth/host.h"
#include "bluetooth/hci.h"
#include "system/manager.h"
#include "system/iblue_led.h"
#include "system/iblue_gesture.h"

#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED

#define NVS_NS "iblue"
#define NVS_KEY "gest"

/* The single tap does nothing for now (it used to open the pairing window: the window opens by itself
 * for a while after power-on, and a 6 s hold searches); the double tap picks the next colour. */
static const uint8_t defaults[IBLUE_GEST_MAX] = {IBLUE_ACT_NONE, IBLUE_ACT_NEXT_COLOUR, IBLUE_ACT_NONE};
static uint8_t map[IBLUE_GEST_MAX] = {IBLUE_ACT_NONE, IBLUE_ACT_NEXT_COLOUR, IBLUE_ACT_NONE};

void iblue_gesture_init(void) {
    nvs_handle_t nvs;
    uint8_t stored[IBLUE_GEST_MAX];
    size_t len = sizeof(stored);

    memcpy(map, defaults, sizeof(map));
    if (nvs_open(NVS_NS, NVS_READONLY, &nvs) == ESP_OK) {
        if (nvs_get_blob(nvs, NVS_KEY, stored, &len) == ESP_OK && len == sizeof(stored)) {
            bool ok = true;

            for (uint32_t i = 0; i < IBLUE_GEST_MAX; i++) {
                ok = ok && stored[i] < IBLUE_ACT_MAX;
            }
            if (ok) {
                memcpy(map, stored, sizeof(map));
            }
        }
        nvs_close(nvs);
    }
}

uint8_t iblue_gesture_get(uint32_t gesture) {
    return gesture < IBLUE_GEST_MAX ? map[gesture] : IBLUE_ACT_NONE;
}

bool iblue_gesture_set(const uint8_t new_map[IBLUE_GEST_MAX]) {
    nvs_handle_t nvs;

    for (uint32_t i = 0; i < IBLUE_GEST_MAX; i++) {
        if (new_map[i] >= IBLUE_ACT_MAX) {
            return false;
        }
    }
    memcpy(map, new_map, sizeof(map));
    if (nvs_open(NVS_NS, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_blob(nvs, NVS_KEY, map, sizeof(map));
        nvs_commit(nvs);
        nvs_close(nvs);
    }
    return true;
}

void iblue_gesture_run(uint32_t gesture) {
    uint8_t act = iblue_gesture_get(gesture);

    printf("# %s: gesture %ld -> action %d\n", __FUNCTION__, gesture, act);
    switch (act) {
        case IBLUE_ACT_PAIR:
#ifdef IBLUE_PAIR_WINDOW
            sys_mgr_pair_window_open();
#endif
            if (!bt_hci_get_inquiry()) {
                bt_hci_start_inquiry();
            }
            break;
        case IBLUE_ACT_STOP_PAIRING:
            if (bt_hci_get_inquiry()) {
                bt_hci_stop_inquiry();
            }
            break;
        case IBLUE_ACT_NEXT_COLOUR:
            iblue_led_toggle_color();
            break;
        case IBLUE_ACT_DISCONNECT:
            bt_host_disconnect_all();
            break;
        default:
            break;
    }
}

#endif /* CONFIG_BLUERETRO_IBLUE_RGB_LED */
