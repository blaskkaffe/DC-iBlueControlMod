/*
 * Copyright (c) 2019-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>
#include <esp_partition.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <soc/efuse_reg.h>
#include "driver/gpio.h"
#include "hal/ledc_hal.h"
#include "hal/gpio_hal.h"
#include "driver/ledc.h"
#include "esp_rom_gpio.h"
#include "adapter/adapter.h"
#include "adapter/config.h"
#include "adapter/memory_card.h"
#include "bluetooth/host.h"
#include "bluetooth/hci.h"
#include "wired/wired_bare.h"
#include "tools/util.h"
#include "system/fs.h"
#include "system/led.h"
#include "bare_metal_app_cpu.h"
#include "manager.h"
#include "esp_timer.h"
#include <nvs_flash.h>
#include <esp_timer.h>
#include "iblue/iblue_led.h"
#include "iblue/iblue_anim_cfg.h"
#include "iblue/iblue_port_led.h"
#include "iblue/iblue_gesture.h"
#include "iblue/iblue_gameid_ctl.h"
#include "iblue/iblue_motion_ctl.h"
#include "iblue/iblue_tap.h"

#ifdef CONFIG_BLUERETRO_HW_IBLUECONTROLMOD
/*
 * iBlueControlMod (internal Dreamcast mod) pinout, recovered from the
 * vendor firmware SEGA_DC_24V01 (BlueRetro v1.9 derivative).
 *  - Boot/pair button is on GPIO16 (GPIO0 is not used as a button).
 *  - GPIO13/GPIO14 drive a status LED (they are POWER_ON/RESET on BlueRetro HW2).
 *  - GPIO17 (BlueRetro error LED) is held low as an output: never PWM it.
 *  - GPIO25 is touched only by the button LED pattern and deep sleep.
 */
#define BOOT_BTN_PIN 16

#define RESET_PIN 14

#define POWER_ON_PIN 13
#define POWER_OFF_PIN 17
#define POWER_SENSE_PIN 39

#define POWER_OFF_ALT_PIN 12

#define SENSE_P1_PIN 36
#define SENSE_P2_PIN 35
#define SENSE_P3_PIN 32
#define SENSE_P4_PIN 33

#define SENSE_P1_ALT_PIN 15
#define SENSE_P2_ALT_PIN 34

#define LED_P1_PIN 15
#define LED_P2_PIN 4
#define LED_P3_PIN 2
#define LED_P4_PIN 12

#define IBLUE_AUX_PIN 25
#define IBLUE_BOOT_BTN_DELAY_MS 1000
#else
#define BOOT_BTN_PIN 0

#define RESET_PIN 14

#define POWER_ON_PIN 13
#define POWER_OFF_PIN 16
#define POWER_SENSE_PIN 39

#define POWER_OFF_ALT_PIN 12

#define SENSE_P1_PIN 35
#define SENSE_P2_PIN 36
#define SENSE_P3_PIN 32
#define SENSE_P4_PIN 33

#define SENSE_P1_ALT_PIN 15
#define SENSE_P2_ALT_PIN 34

#define LED_P1_PIN 2
#define LED_P2_PIN 4
#define LED_P3_PIN 12
#define LED_P4_PIN 15
#endif

#define INHIBIT_CNT 200

typedef void (*sys_mgr_cmd_t)(void);

enum {
    SYS_MGR_BTN_STATE0 = 0,
    SYS_MGR_BTN_STATE1,
    SYS_MGR_BTN_STATE2,
    SYS_MGR_BTN_STATE3,
};

#ifdef CONFIG_BLUERETRO_HW2
static uint8_t sense_list[] = {
    SENSE_P1_PIN, SENSE_P2_PIN, SENSE_P3_PIN, SENSE_P4_PIN
};
#endif
static uint8_t led_list[] = {
    LED_P1_PIN, LED_P2_PIN, LED_P3_PIN, LED_P4_PIN
};
static uint8_t current_pulse_led = LED_P1_PIN;
static uint8_t err_led_pin;
static uint8_t power_off_pin = POWER_OFF_PIN;
static uint8_t led_init_cnt = 1;
static uint16_t port_state = 0;
static RingbufHandle_t cmd_q_hdl = NULL;
static uint32_t chip_package = EFUSE_RD_CHIP_VER_PKG_ESP32D0WDQ6;
static bool factory_reset = false;

static int32_t sys_mgr_get_power(void);
static int32_t sys_mgr_get_boot_btn(void);
static void sys_mgr_reset(void);
static void sys_mgr_power_on(void);
static void sys_mgr_power_off(void);
static void sys_mgr_inquiry_toggle(void);
static void sys_mgr_factory_reset(void);
static void sys_mgr_deep_sleep(void);
static void sys_mgr_esp_restart(void);
static void sys_mgr_wired_reset(void);

static const sys_mgr_cmd_t sys_mgr_cmds[] = {
    sys_mgr_reset,
    sys_mgr_power_on,
    sys_mgr_power_off,
    sys_mgr_inquiry_toggle,
    sys_mgr_factory_reset,
    sys_mgr_deep_sleep,
    sys_mgr_esp_restart,
    sys_mgr_wired_reset,
};

static inline uint32_t sense_port_is_empty(uint32_t index) {
#ifdef CONFIG_BLUERETRO_HW2
    if (hw_config.ports_sense_input_polarity) {
        return !gpio_get_level(sense_list[index]);
    }
    else {
        return gpio_get_level(sense_list[index]);
    }
#else
    return 1;
#endif
}

static inline void set_power_on(uint32_t state) {
#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
    /* GPIO13 is an RGB LED colour on this board, there is no power output */
    (void)state;
    return;
#endif
    if (hw_config.power_pin_polarity) {
        gpio_set_level(POWER_ON_PIN, !state);
    }
    else {
        gpio_set_level(POWER_ON_PIN, state);
    }
}

static inline void set_power_off(uint32_t state) {
    if (hw_config.power_pin_polarity) {
        gpio_set_level(power_off_pin, !state);
    }
    else {
        gpio_set_level(power_off_pin, state);
    }
}

static inline void set_reset(uint32_t state) {
#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
    /* GPIO14 is an RGB LED colour on this board, there is no reset output */
    (void)state;
    return;
#endif
    if (hw_config.reset_pin_polarity) {
        gpio_set_level(RESET_PIN, !state);
    }
    else {
        gpio_set_level(RESET_PIN, state);
    }
}


#ifdef IBLUE_PAIR_WINDOW
/* Window starts at boot (esp_timer counts from boot) */
static volatile int64_t pair_window_end_us = (int64_t)CONFIG_BLUERETRO_IBLUE_PAIR_WINDOW_S * 1000000;

void sys_mgr_pair_window_open(void) {
    pair_window_end_us = esp_timer_get_time() + (int64_t)CONFIG_BLUERETRO_IBLUE_PAIR_WINDOW_S * 1000000;
}

bool sys_mgr_pairing_allowed(void) {
    return esp_timer_get_time() < pair_window_end_us;
}

static void pair_window_check(void) {
    if (bt_hci_get_inquiry() && !sys_mgr_pairing_allowed()) {
        printf("# %s: pairing window closed\n", __FUNCTION__);
        bt_hci_stop_inquiry();
    }
}
#endif

static inline void set_sense_out(uint32_t pin, uint32_t state) {
    if (hw_config.ports_sense_output_polarity) {
        gpio_set_level(pin, !state);
    }
    else {
        gpio_set_level(pin, state);
    }
}

static inline void set_port_led(uint32_t index, uint32_t state) {
#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
    /* Own PWM channel per LED: it fades in and out, and shows the boot animation */
    iblue_port_led_set(index, state);
    return;
#endif
    if (state) {
        esp_rom_gpio_connect_out_signal(led_list[index], ledc_periph_signal[LEDC_LOW_SPEED_MODE].sig_out0_idx + LEDC_CHANNEL_1, 0, 0);
    }
    else {
        esp_rom_gpio_connect_out_signal(led_list[index], ledc_periph_signal[LEDC_LOW_SPEED_MODE].sig_out0_idx + LEDC_CHANNEL_2, 0, 0);
    }
}

static inline uint32_t get_port_led_pin(uint32_t index) {
    return led_list[index];
}

static void internal_flag_init(void) {
#if defined(CONFIG_BLUERETRO_HW_IBLUECONTROLMOD)
    /* Always internal: GPIO13/14 are status LED pins, don't sample them */
    hw_config.external_adapter = 0;
#elif defined(CONFIG_BLUERETRO_HW2)
    if (hw_config.power_pin_polarity) {
        if (!gpio_get_level(POWER_ON_PIN) && gpio_get_level(RESET_PIN)) {
            hw_config.external_adapter = 1;
        }
    }
    else {
        if (gpio_get_level(POWER_ON_PIN) && gpio_get_level(RESET_PIN)) {
            hw_config.external_adapter = 1;
        }
    }
#else
    hw_config.external_adapter = 1;
#endif
    if (hw_config.external_adapter) {
        printf("# %s: External adapter\n", __FUNCTION__);
    }
    else {
        printf("# %s: Internal adapter\n", __FUNCTION__);
    }
}

static void port_led_pulse(uint32_t pin) {
    if (pin && !iblue_port_led_busy()) {
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);
        gpio_set_direction(pin, GPIO_MODE_OUTPUT);
        esp_rom_gpio_connect_out_signal(pin, ledc_periph_signal[LEDC_HIGH_SPEED_MODE].sig_out0_idx + LEDC_CHANNEL_0, 0, 0);
#if defined(CONFIG_BLUERETRO_HW_IBLUECONTROLMOD) && !defined(CONFIG_BLUERETRO_IBLUE_RGB_LED)
        iblue_status_led_pairing();
#endif
    }
}

static void set_leds_as_btn_status(uint8_t state) {
    ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, hw_config.led_flash_on_duty_cycle, 0);

    /* Use all port LEDs. On the iBlue they only flash with a Bluetooth controller connected (and not during
     * the boot animation): the RGB LED shows the button stages on its own. */
#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
    struct bt_dev *ready_dev = NULL;
    bool port_leds_may_flash = !iblue_port_led_busy() && bt_host_get_hid_init_dev(&ready_dev) >= 0;
#else
    bool port_leds_may_flash = true;
#endif

    for (uint32_t i = 0; i < hw_config.port_cnt; i++) {
        uint8_t pin = led_list[i];

        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);
        gpio_set_direction(pin, GPIO_MODE_OUTPUT);
        if (state && port_leds_may_flash) {
            esp_rom_gpio_connect_out_signal(pin, ledc_periph_signal[LEDC_LOW_SPEED_MODE].sig_out0_idx + LEDC_CHANNEL_1, 0, 0);
        }
    }

#if defined(CONFIG_BLUERETRO_IBLUE_RGB_LED)
    iblue_led_btn(state, 0);
    (void)err_led_pin;
#elif defined(CONFIG_BLUERETRO_HW_IBLUECONTROLMOD)
    /* Replicates vendor fw button LED pattern on GPIO13/14/25 */
    if (state) {
        gpio_set_level(RESET_PIN, 1);
        gpio_set_level(IBLUE_AUX_PIN, 1);
        esp_rom_gpio_connect_out_signal(POWER_ON_PIN, ledc_periph_signal[LEDC_LOW_SPEED_MODE].sig_out0_idx + LEDC_CHANNEL_1, 1, 0);
        esp_rom_gpio_connect_out_signal(RESET_PIN, ledc_periph_signal[LEDC_HIGH_SPEED_MODE].sig_out0_idx + LEDC_CHANNEL_1, 1, 0);
    }
    else {
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[POWER_ON_PIN], PIN_FUNC_GPIO);
        gpio_set_direction(POWER_ON_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level(POWER_ON_PIN, 1);
        gpio_set_level(RESET_PIN, 1);
        gpio_set_level(IBLUE_AUX_PIN, 1);
    }
    (void)err_led_pin;
#else
    /* Use error LED as well */
    if (state) {
        esp_rom_gpio_connect_out_signal(err_led_pin, ledc_periph_signal[LEDC_LOW_SPEED_MODE].sig_out0_idx + LEDC_CHANNEL_1, 0, 0);
    }
    else {
        esp_rom_gpio_connect_out_signal(err_led_pin, ledc_periph_signal[LEDC_HIGH_SPEED_MODE].sig_out0_idx + LEDC_CHANNEL_0, 0, 0);
    }
#endif
}

static void power_on_hdl(void) {
#ifdef CONFIG_BLUERETRO_HW2
    static int32_t curr = 0, prev = 0;
    struct bt_dev *not_used;

    prev = curr;
    curr = sys_mgr_get_power();
#ifdef CONFIG_BLUERETRO_IBLUE_BOOT_ANIM
    if (curr && !prev) {
        /* Console just turned on. The adapter boots with it: the first time, part of the BIOS animation
         * has already played while the firmware was starting. */
        static bool first = true;
        uint32_t elapsed = first ? (uint32_t)(esp_timer_get_time() / 1000) + CONFIG_BLUERETRO_IBLUE_BOOT_ANIM_BOOT_MS : 0;

        first = false;
        iblue_port_led_boot_anim(elapsed);
    }
#endif
#ifdef IBLUE_PAIR_WINDOW
    if (curr && !prev) {
        /* Console just turned on: allow pairing for a while */
        sys_mgr_pair_window_open();
    }
#endif
    if (curr) {
        /* System is power on */
        bt_hci_inquiry_override(0);

        if (bt_host_get_active_dev(&not_used) < 0) {
            /* No Bt device */
            uint32_t port_cnt_loc = 0;

            for (uint32_t i = 0; i < hw_config.port_cnt; i++) {
                if (sense_port_is_empty(i)) {
                    port_cnt_loc++;
                }
            }
            if (port_cnt_loc == hw_config.port_cnt) {
                /* No wired controller */
                if (!bt_hci_get_inquiry()) {
                    bt_hci_start_inquiry();
                }
            }
        }
    }
    else {
        /* System is off */
        if (curr != prev) {
            /* Kick BT controllers on off transition */
            bt_host_disconnect_all();
        }
        else if (bt_host_get_hid_init_dev(&not_used) > -1) {
            /* Power on BT connect */
            bt_hci_inquiry_override(0);
            sys_mgr_power_on();
            return;
        }
        /* Make sure no inquiry while power off */
        bt_hci_inquiry_override(1);
        if (bt_hci_get_inquiry()) {
            bt_hci_stop_inquiry();
        }
    }
#endif /* CONFIG_BLUERETRO_HW2 */
}

static void wired_port_hdl(void) {
    uint32_t update = 0;
    uint16_t port_mask = 0;
    uint8_t err_led_set = 0;
#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
    struct bt_dev *ready_dev = NULL;
    bool any_ready = bt_host_get_hid_init_dev(&ready_dev) >= 0; /* port LEDs only blink with a controller connected */
    bool connecting = false;
#else
    bool any_ready = true;
#endif

    for (int32_t i = 0, j = 0, idx = 0; i < BT_MAX_DEV; i++) {
        struct bt_dev *device = NULL;
        uint8_t bt_ready = 0;

        bt_host_get_dev_from_id(i, &device);

        for (; j < hw_config.port_cnt; j++) {
            if (sense_port_is_empty(j)) {
                j++;
                break;
            }
            set_port_led(j, 0);
            idx++;
        }

        bt_ready = atomic_test_bit(&device->flags, BT_DEV_HID_INIT_DONE);
#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
        if (!bt_ready && atomic_test_bit(&device->flags, BT_DEV_DEVICE_FOUND)) {
            connecting = true;
        }
#endif

#ifdef CONFIG_BLUERETRO_HW2
        int32_t prev_idx = device->ids.out_idx;
#endif
        device->ids.out_idx = idx;
        if ((hw_config.hotplug && bt_ready) || !hw_config.hotplug) {
            port_mask |= BIT(idx) | adapter_get_out_mask(idx);
        }
        idx++;


        if (device->ids.out_idx < hw_config.port_cnt) {
            if (bt_ready) {
#if defined(CONFIG_BLUERETRO_HW_IBLUECONTROLMOD) && !defined(CONFIG_BLUERETRO_IBLUE_RGB_LED)
                iblue_status_led_connected();
#endif
                set_port_led(device->ids.out_idx, 1);
            }
            else if (get_port_led_pin(device->ids.out_idx) != current_pulse_led) {
                set_port_led(device->ids.out_idx, 0);
            }
        }

        if (!bt_ready && !err_led_set) {
            uint8_t new_led = (device->ids.out_idx < hw_config.port_cnt) ? get_port_led_pin(device->ids.out_idx) : 0;

            if (bt_hci_get_inquiry()) {
                if (any_ready) {
                    port_led_pulse(new_led);
                    current_pulse_led = new_led;
                }
                else {
                    current_pulse_led = 0; /* nothing connected: no blinking port LED, the RGB LED shows it */
                }
                err_led_set = 1;
            }
            else {
                current_pulse_led = 0;
            }
        }

#ifdef CONFIG_BLUERETRO_HW2
        if (device->ids.out_idx != prev_idx) {
            update++;
            printf("# %s: BTDEV %ld map to WIRED %ld\n", __FUNCTION__, i, device->ids.out_idx);
            if (bt_ready) {
                struct raw_fb fb_data = {0};

                fb_data.header.wired_id = device->ids.out_idx;
                fb_data.header.type = FB_TYPE_PLAYER_LED;
                fb_data.header.data_len = 0;
                adapter_q_fb(&fb_data);
            }
        }
#endif
    }
#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
    iblue_led_connecting(connecting);
#endif
    if (hw_config.hotplug) {
        if (port_state != port_mask) {
            update++;
        }
    }
    if (update && !mc_get_state()) {
        printf("# %s: Update ports state: %04X\n", __FUNCTION__, port_mask);
        wired_bare_port_cfg(port_mask);
        port_state = port_mask;
        if (hw_config.ports_sense_p3_p4_as_output) {
            /* Toggle Wii classic sense line to force ctrl reinit */
            set_sense_out(SENSE_P3_PIN, 0);
            set_sense_out(SENSE_P4_PIN, 0);
            vTaskDelay(hw_config.ports_sense_output_ms / portTICK_PERIOD_MS);
            set_sense_out(SENSE_P3_PIN, 1);
            set_sense_out(SENSE_P4_PIN, 1);
        }
    }
}

#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
/* A short press (a tap) has just ended while the console is on. Count the taps that follow it (up to
 * three, see iblue_tap.h) and run what that many taps are set to do. If the second press is kept down
 * it is tap + hold: the LED sweeps through the colours until the button is released, and that colour is
 * kept as the custom colour. The single tap waits for the gap, so a double tap does not also run it. */
static void iblue_tap_gestures(void) {
    struct iblue_tap tap;

    iblue_tap_start(&tap);
    while (1) {
        int ev = iblue_tap_step(&tap, sys_mgr_get_boot_btn(), 10);

        if (ev == IBLUE_TAP_EV_DONE) {
            iblue_gesture_run(tap.taps - 1u);
            return;
        }
        if (ev == IBLUE_TAP_EV_HOLD_START) {
            iblue_led_sweep_start();
        }
        else if (ev == IBLUE_TAP_EV_HOLD_END) {
            iblue_led_sweep_stop();
            return;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
#endif

static void boot_btn_hdl(void) {
    static uint32_t check_qdp = 0;
    static uint32_t inhibit_cnt = 0;
    uint32_t hold_cnt = 0;
    uint32_t state = 0;

    /* Let inhibit_cnt reach 0 before handling button again */
    if (inhibit_cnt && inhibit_cnt--) {
        /* Power off on quick double press */
#if defined(CONFIG_BLUERETRO_IBLUE_RGB_LED)
        /* No power control on this board: the taps are counted in iblue_tap_gestures() */
        (void)check_qdp;
        if (0) {
#elif defined(CONFIG_BLUERETRO_HW_IBLUECONTROLMOD)
        /* No power control on this board (GPIO13 is the RGB LED): a double
         * tap would only disconnect every BT controller. Vendor fw ignores it. */
        (void)check_qdp;
        if (0) {
#else
        if (check_qdp && sys_mgr_get_power() && sys_mgr_get_boot_btn()) {
#endif
            sys_mgr_power_off();
            check_qdp = 0;
        }
        return;
    }
    check_qdp = 0;

    if (sys_mgr_get_boot_btn()) {
        set_leds_as_btn_status(1);

        while (sys_mgr_get_boot_btn()) {
            hold_cnt++;
            if (hold_cnt > (hw_config.sw_io0_hold_thres_ms[state] / 10) && state < SYS_MGR_BTN_STATE3) {
                ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, hw_config.led_flash_duty_cycle, 0);
                ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1, hw_config.led_flash_hz[state]);
                state++;
#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
                iblue_led_btn(1, state);
#endif
            }
            if (hold_cnt == 3000) {
                printf("# FW will be factory reset\n");
                factory_reset = true;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }

#ifndef CONFIG_BLUERETRO_SYSTEM_UNIVERSAL
        if (hw_config.external_adapter)
#endif
        {
            state++;
        }

        if (sys_mgr_get_power()) {
            /* System is on */
            switch (state) {
                case SYS_MGR_BTN_STATE0:
#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
                    iblue_tap_gestures();
#else
                    sys_mgr_reset();
                    check_qdp = 1;
#endif
                    break;
                case SYS_MGR_BTN_STATE1:
                    if (bt_hci_get_inquiry()) {
                        bt_hci_stop_inquiry();
                    }
                    else {
                        bt_host_disconnect_all();
                    }
                    break;
                case SYS_MGR_BTN_STATE2:
#ifdef IBLUE_PAIR_WINDOW
                    sys_mgr_pair_window_open();
#endif
                    bt_hci_start_inquiry();
                    break;
                default:
                    sys_mgr_factory_reset();
                    break;
            }
        }
        else {
            /* System is off */
            switch (state) {
                case SYS_MGR_BTN_STATE0:
                    sys_mgr_power_on();
                    break;
                default:
                    set_reset(0);
                    sys_mgr_power_on();
                    set_reset(1);
                    break;
            }
        }

        set_leds_as_btn_status(0);
#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
        inhibit_cnt = (state == SYS_MGR_BTN_STATE0) ? 20 : INHIBIT_CNT; /* after a tap sequence only a short pause */
#else
        inhibit_cnt = INHIBIT_CNT;
#endif
    }
}

static void sys_mgr_task(void *arg) {
    uint32_t cnt = 0;
    uint8_t *cmd;
    size_t cmd_len;

    while (1) {
        boot_btn_hdl();
#ifdef IBLUE_PAIR_WINDOW
        pair_window_check();
#endif

        /* Fetch system cmd to execute */
        if (cmd_q_hdl) {
            cmd = (uint8_t *)xRingbufferReceive(cmd_q_hdl, &cmd_len, 0);
            if (cmd) {
                if (sys_mgr_cmds[*cmd]) {
                    sys_mgr_cmds[*cmd]();
                }
                vRingbufferReturnItem(cmd_q_hdl, (void *)cmd);
            }
        }

        /* Update those only 1/32 loop */
        if ((cnt & 0x1F) == 0x1F) {
            wired_port_hdl();
            if (!hw_config.external_adapter) {
                power_on_hdl();
            }
        }
        cnt++;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

static void sys_mgr_reset(void) {
#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
    /* No reset line on this board: a short tap (re)opens the pairing window */
#ifdef IBLUE_PAIR_WINDOW
    sys_mgr_pair_window_open();
#endif
    if (!bt_hci_get_inquiry()) {
        bt_hci_start_inquiry();
    }
#else
    set_reset(0);
    vTaskDelay(hw_config.reset_pin_pulse_ms / portTICK_PERIOD_MS);
    set_reset(1);
#endif
}

static void sys_mgr_inquiry_toggle(void) {
    if (bt_hci_get_inquiry()) {
        bt_hci_stop_inquiry();
    }
    else {
        bt_hci_start_inquiry();
    }
}

static void sys_mgr_power_on(void) {
    set_power_on(1);
    if (!hw_config.power_pin_is_hold) {
        vTaskDelay(hw_config.power_pin_pulse_ms / portTICK_PERIOD_MS);
        set_power_on(0);
    }
}

static void sys_mgr_power_off(void) {
    bt_host_disconnect_all();
#ifdef CONFIG_BLUERETRO_HW2
    if (hw_config.power_pin_is_hold) {
        set_power_on(0);
    }
    else {
        set_power_off(1);
        vTaskDelay(hw_config.power_pin_pulse_ms / portTICK_PERIOD_MS);
        set_power_off(0);
    }
#endif
}

static int32_t sys_mgr_get_power(void) {
#ifdef CONFIG_BLUERETRO_SYSTEM_UNIVERSAL
    return 1;
#else
    if (hw_config.external_adapter) {
        return 1;
    }
    else {
        return gpio_get_level(POWER_SENSE_PIN);
    }
#endif
}

static int32_t sys_mgr_get_boot_btn(void) {
    return !gpio_get_level(BOOT_BTN_PIN);
}

static void sys_mgr_factory_reset(void) {
    const esp_partition_t* partition = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, "otadata");
    if (factory_reset && partition) {
        esp_partition_erase_range(partition, 0, partition->size);
    }

    fs_reset();
    printf("BlueRetro factory reset\n");
    bt_host_disconnect_all();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
}

static void sys_mgr_esp_restart(void) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
}

static void sys_mgr_deep_sleep(void) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
#ifdef CONFIG_BLUERETRO_HW_IBLUECONTROLMOD
#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
    iblue_led_sleep();
    static const uint8_t hold_pins[] = {LED_P1_PIN};
#else
    /* Vendor fw: drive GPIO25 and GPIO15 low and latch them through deep sleep */
    static const uint8_t hold_pins[] = {IBLUE_AUX_PIN, LED_P1_PIN};
#endif
    for (uint32_t i = 0; i < sizeof(hold_pins); i++) {
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[hold_pins[i]], PIN_FUNC_GPIO);
        gpio_set_direction(hold_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(hold_pins[i], 0);
        gpio_hold_en(hold_pins[i]);
    }
    gpio_deep_sleep_hold_en();
#endif
    esp_deep_sleep_start();
}

static void IRAM_ATTR sys_mgr_wired_reinit_task(void) {
    for (uint32_t i = 0; i < WIRED_MAX_DEV; i++) {
        adapter_init_buffer(i);
    }

    if (wired_adapter.system_id < WIRED_MAX) {
        wired_bare_init(chip_package);
    }
}

static void sys_mgr_wired_reset(void) {
    init_app_cpu_baremetal();
    start_app_cpu(sys_mgr_wired_reinit_task);
    port_state = 0;
}

void sys_mgr_cmd(uint8_t cmd) {
    if (cmd_q_hdl) {
        UBaseType_t ret = xRingbufferSend(cmd_q_hdl, &cmd, sizeof(cmd), portMAX_DELAY);
        if (ret != pdTRUE) {
            printf("# %s cmd_q full!\n", __FUNCTION__);
        }
    }
    else {
        printf("# %s cmd_q_hdl NULL!\n", __FUNCTION__);
        if (cmd == SYS_MGR_CMD_WIRED_RST) {
            /* For gameid cfg we may need to reset bare core very early */
            sys_mgr_wired_reset();
        }
    }
}

#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
/* First thing the firmware does: take over the LEDs, so the port LEDs and the power LED do not sit in
 * whatever state the pins power up in while the file system, Bluetooth and the rest start (that took
 * seconds). The port LEDs belong to the boot animation from here on; the power LED shows its colour.
 * Assumes four ports (the Dreamcast build). Needs NVS for the saved colour. */
void sys_mgr_iblue_early(void) {
    int32_t ret;

    /* The port LEDs first (they need nothing else): B, C and D go off, A stays lit until the animation starts. */
    iblue_port_led_init(led_list, sizeof(led_list));
    /* The settings of the animation and the saved colour of the power LED are in NVS */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    iblue_anim_cfg_init();
#ifdef CONFIG_BLUERETRO_IBLUE_BOOT_ANIM
    iblue_port_led_boot_anim((uint32_t)(esp_timer_get_time() / 1000) + CONFIG_BLUERETRO_IBLUE_BOOT_ANIM_BOOT_MS);
#endif
    iblue_led_init();
}
#endif

void sys_mgr_init(uint32_t package) {
    gpio_config_t io_conf = {0};

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    io_conf.pin_bit_mask = 1ULL << BOOT_BTN_PIN;
    gpio_config(&io_conf);

    chip_package = package;
    err_led_pin = err_led_get_pin();

    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_20_BIT,
        .freq_hz = 2,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_1,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_1,
        .duty       = hw_config.led_flash_on_duty_cycle,
        .gpio_num   = LED_P1_PIN,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_1,
    };
    ledc_timer_config(&ledc_timer);
    ledc_channel_config(&ledc_channel);
#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
    iblue_port_led_reclaim(); /* the channel setup above routed port A's pin to the 2 Hz flash */
#endif

    while (wired_adapter.system_id <= WIRED_AUTO) {
        boot_btn_hdl();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    cmd_q_hdl = xRingbufferCreate(32, RINGBUF_TYPE_NOSPLIT);
    if (cmd_q_hdl == NULL) {
        printf("# Failed to create cmd_q ring buffer\n");
    }

    switch (wired_adapter.system_id) {
        case GENESIS:
        case SATURN:
            hw_config.port_cnt = 2;
#ifdef CONFIG_BLUERETRO_HW2
            sense_list[0] = SENSE_P1_ALT_PIN;
            sense_list[1] = SENSE_P2_ALT_PIN;
            power_off_pin = POWER_OFF_ALT_PIN;
#endif
            break;
        case JAGUAR:
            hw_config.port_cnt = 1;
#ifdef CONFIG_BLUERETRO_HW2
            sense_list[0] = SENSE_P1_ALT_PIN;
            sense_list[1] = SENSE_P2_ALT_PIN;
#endif
            break;
        case WII_EXT:
            hw_config.port_cnt = 2;
            hw_config.power_pin_is_hold = 1;
            hw_config.ports_sense_input_polarity = 1;
            hw_config.power_pin_polarity = 1;
            hw_config.ports_sense_p3_p4_as_output = 1;
            break;
        case N64:
        case DC:
        case GC:
            hw_config.port_cnt = 4;
            break;
        case PARALLEL_1P:
        case PCE:
        case REAL_3DO:
        case JVS:
        case VBOY:
        case PARALLEL_1P_OD:
        case SEA_BOARD:
            hw_config.port_cnt = 1;
            break;
        default:
            hw_config.port_cnt = 2;
            break;
    }

#ifdef CONFIG_BLUERETRO_BT_H4_TRACE
    hw_config.port_cnt = 1;
#endif

    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
#if defined(CONFIG_BLUERETRO_HW2) && !defined(CONFIG_BLUERETRO_IBLUE_RGB_LED)
    io_conf.pin_bit_mask = 1ULL << POWER_ON_PIN;
    gpio_config(&io_conf);
    io_conf.pin_bit_mask = 1ULL << RESET_PIN;
    gpio_config(&io_conf);
#endif

    internal_flag_init();

    hw_config_patch();

    err_led_cfg_update();

    led_init_cnt = hw_config.port_cnt;
    if (wired_adapter.system_id == PSX || wired_adapter.system_id == PS2) {
        led_init_cnt = 4;
    }

#ifdef CONFIG_BLUERETRO_HW2
    for (uint32_t i = 0; i < hw_config.port_cnt; i++) {
        io_conf.pin_bit_mask = 1ULL << sense_list[i];
        gpio_config(&io_conf);
    }
    io_conf.pin_bit_mask = 1ULL << POWER_SENSE_PIN;
    gpio_config(&io_conf);
#endif

#ifndef CONFIG_BLUERETRO_HW2
    for (uint32_t i = 0; i < sizeof(led_list); i++) {
        led_list[i] = hw_config.hw1_ports_led_pins[i];
    }
#endif

    io_conf.mode = GPIO_MODE_OUTPUT;
    for (uint32_t i = 0; i < led_init_cnt; i++) {
#ifdef CONFIG_BLUERETRO_HW2
        /* Skip pin 15 if alt sense pin are used */
        if (sense_list[0] == led_list[i]) {
            continue;
        }
#endif
        gpio_set_level(led_list[i], 0);
        io_conf.pin_bit_mask = 1ULL << led_list[i];
        gpio_config(&io_conf);

#ifndef CONFIG_BLUERETRO_IBLUE_RGB_LED /* the port LED PWM channels own these pins already */
        if (i < hw_config.port_cnt) {
            /* Can't use GPIO mode on port LED as some wired driver overwrite whole GPIO port */
            /* Use unused LEDC channel 2 to force output low */
            esp_rom_gpio_connect_out_signal(led_list[i], ledc_periph_signal[LEDC_LOW_SPEED_MODE].sig_out0_idx + LEDC_CHANNEL_2, 0, 0);
        }
#endif
    }
#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
    iblue_port_led_init(led_list, hw_config.port_cnt);
    /* gpio_config() above made every port LED pin a plain GPIO output, which disconnects it from its PWM
     * channel: the intro stopped there (after A and B) and the pins stayed dark until the old start-up path
     * attached them again. Give them back. */
    iblue_port_led_reclaim();
#endif

#ifdef CONFIG_BLUERETRO_HW2
    if (hw_config.power_pin_od) {
        io_conf.mode = GPIO_MODE_OUTPUT_OD;
    }
    else {
        io_conf.mode = GPIO_MODE_OUTPUT;
    }
#ifndef CONFIG_BLUERETRO_IBLUE_RGB_LED
    set_power_on(0);
    io_conf.pin_bit_mask = 1ULL << POWER_ON_PIN;
    gpio_config(&io_conf);
#endif

    set_power_off(0);
    io_conf.pin_bit_mask = 1ULL << power_off_pin;
    gpio_config(&io_conf);

#ifndef CONFIG_BLUERETRO_IBLUE_RGB_LED
    gpio_set_level(RESET_PIN, 1);
    if (hw_config.reset_pin_od) {
        io_conf.mode = GPIO_MODE_OUTPUT_OD;
    }
    else {
        io_conf.mode = GPIO_MODE_OUTPUT;
    }
    io_conf.pin_bit_mask = 1ULL << RESET_PIN;
    gpio_config(&io_conf);
#endif

    if (hw_config.ports_sense_p3_p4_as_output) {
        /* Wii-ext got a sense line that we need to control */
        if (hw_config.ports_sense_output_od) {
            io_conf.mode = GPIO_MODE_OUTPUT_OD;
        }
        else {
            io_conf.mode = GPIO_MODE_OUTPUT;
        }
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.pin_bit_mask = 1ULL << SENSE_P4_PIN;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&io_conf);
        gpio_set_level(SENSE_P4_PIN, 1);
        io_conf.pin_bit_mask = 1ULL << SENSE_P3_PIN;
        gpio_config(&io_conf);
        gpio_set_level(SENSE_P3_PIN, 1);
    }

#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
    iblue_led_init();
    iblue_gesture_init();
    iblue_gameid_ctl_init();
    iblue_motion_ctl_init();
#endif

    /* If boot switch pressed at boot, trigger system on and goes to deep sleep */
#ifdef CONFIG_BLUERETRO_HW_IBLUECONTROLMOD
    vTaskDelay(IBLUE_BOOT_BTN_DELAY_MS / portTICK_PERIOD_MS);
    if (sys_mgr_get_boot_btn()) {
#else
    if (sys_mgr_get_boot_btn() && !sys_mgr_get_power()) {
#endif
        sys_mgr_power_on();
        sys_mgr_deep_sleep();
    }
#endif /* CONFIG_BLUERETRO_HW2 */

    xTaskCreatePinnedToCore(sys_mgr_task, "sys_mgr_task", 2048, NULL, 5, NULL, 0);
}
