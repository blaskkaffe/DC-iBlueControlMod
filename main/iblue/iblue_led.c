/*
 * iBlueControlMod RGB status LED driver
 * SPDX-License-Identifier: Apache-2.0
 *
 * The board has a 4-pin common-anode RGB LED: each colour pin lights when
 * driven LOW. Each colour gets its own LEDC low-speed channel with the output
 * inverted, so duty 0 = off and full duty = fully on.
 *
 * Pattern priority: button held > pairing (BT inquiry) > solid colour.
 */

#include <stdio.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <nvs.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "hal/gpio_hal.h"
#include "soc/io_mux_reg.h"
#include "bluetooth/host.h"
#include "bluetooth/hci.h"
#include "iblue_led.h"
#include "iblue_port_led.h"

#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED

#define IBLUE_LED_TIMER     LEDC_TIMER_2
#define IBLUE_LED_MODE      LEDC_LOW_SPEED_MODE
#define IBLUE_LED_RES       LEDC_TIMER_10_BIT
#define IBLUE_LED_MAX_DUTY  1023
#define IBLUE_LED_FREQ_HZ   5000
#define IBLUE_LED_TICK_MS   20
#define IBLUE_LED_BREATH_MS 1500

#define IBLUE_LED_SWEEP_MS  10000 /* full hue circle while adjusting */
#define IBLUE_LED_CAP_PCT   CONFIG_BLUERETRO_IBLUE_LED_MAX_PCT /* never brighter than this */
#define IBLUE_LED_CONNECT_BLINK_MS 300  /* connecting: half a period of the blink (3 Hz) */
#define IBLUE_LED_CONNECT_MAX_MS 20000  /* a connection that never finishes must not blink forever */
/* When the LED settles to solid (a controller connected, or at power-on) it fades in: each frame the
 * level covers this part of the way to the goal. It is the level the eye sees (the duty is that to
 * the power 2.2), so it is quick at first and slow at the end, a logarithmic look. 95 % in 0.26 s. */
#define IBLUE_LED_FADE_STEP 0.20f

#define NVS_NS  "iblue"
#define NVS_KEY "led_col"
#define NVS_KEY_RGB "led_rgb"

static const uint8_t pins[3] = {
    CONFIG_BLUERETRO_IBLUE_LED_R_GPIO,
    CONFIG_BLUERETRO_IBLUE_LED_G_GPIO,
    CONFIG_BLUERETRO_IBLUE_LED_B_GPIO,
};
static const ledc_channel_t chans[3] = {LEDC_CHANNEL_3, LEDC_CHANNEL_4, LEDC_CHANNEL_5};

/* r, g, b. Green is scaled down for orange as LED green is much brighter. */
static const uint8_t presets[IBLUE_LED_COLOR_MAX][3] = {
    [IBLUE_LED_ORANGE] = {255, 22, 0}, /* deep amber like the original DC LED */
    [IBLUE_LED_BLUE]   = {0, 0, 255},
    [IBLUE_LED_CUSTOM] = {255, 22, 0}, /* replaced by the saved custom colour */
};
/* Blink frequency per button hold stage (stage 0 = solid) */
static const uint32_t btn_blink_hz[4] = {0, 2, 4, 8};

static volatile uint8_t rgb[3];
static volatile uint32_t color_idx;
static volatile uint32_t btn_active;
static volatile uint32_t btn_stage;
static volatile uint32_t sleeping;
static volatile uint32_t sweeping;
static volatile uint32_t connecting;
static volatile uint32_t sweep_t;
static volatile uint32_t notice_ticks; /* frames left of a notice (see iblue_led_notice) */
static volatile uint32_t notice_total;
static uint8_t custom_rgb[3] = {255, 22, 0};
static uint32_t has_custom;
static uint32_t init_done;

/* Hue 0..1535 (6 segments of 256) at full saturation and value */
static void hue_to_rgb(uint32_t h, volatile uint8_t *out) {
    uint32_t seg = (h / 256) % 6;
    uint8_t x = h % 256;
    uint8_t r = 0, g = 0, b = 0;

    switch (seg) {
        case 0: r = 255; g = x; break;
        case 1: r = 255 - x; g = 255; break;
        case 2: g = 255; b = x; break;
        case 3: g = 255 - x; b = 255; break;
        case 4: r = x; b = 255; break;
        default: r = 255; b = 255 - x; break;
    }
    out[0] = r;
    out[1] = g;
    out[2] = b;
}

static void set_level(uint32_t level_1023) {
    level_1023 = level_1023 * IBLUE_LED_CAP_PCT / 100;
    for (uint32_t i = 0; i < 3; i++) {
        uint32_t duty = (rgb[i] * level_1023) / 255;
        ledc_set_duty(IBLUE_LED_MODE, chans[i], duty);
        ledc_update_duty(IBLUE_LED_MODE, chans[i]);
    }
}

static void iblue_led_task(void *arg) {
    uint32_t t = 0;
    uint32_t connect_ms = 0;
    float shown = 1.0f; /* what the eye sees of the solid LED, 0..1: on fully from the start, no fade-in at power-on */
    uint32_t breath_t = 0; /* time into the pairing breath: it starts at full brightness every time it begins */

    while (1) {
        uint32_t level = IBLUE_LED_MAX_DUTY;
        int solid = 0;
        int breathing = 0;

        connect_ms = connecting ? connect_ms + IBLUE_LED_TICK_MS : 0;
        if (sleeping) {
            level = 0;
        }
        else if (notice_ticks) {
            /* a dip: 8 frames dark, 8 lit, once per count */
            level = (((notice_total - notice_ticks) % 16) < 8) ? 0 : IBLUE_LED_MAX_DUTY;
            notice_ticks--;
        }
        else if (sweeping) {
            /* Colour adjust: sweep the hue wheel, starting at red */
            hue_to_rgb((sweep_t * 1536) / IBLUE_LED_SWEEP_MS, rgb);
            sweep_t = (sweep_t + IBLUE_LED_TICK_MS) % IBLUE_LED_SWEEP_MS;
        }
        else if (btn_active) {
            uint32_t hz = btn_blink_hz[btn_stage & 3];
            if (hz) {
                uint32_t period = 1000 / hz;
                level = ((t % period) < (period / 2)) ? IBLUE_LED_MAX_DUTY : 0;
            }
        }
        else if (iblue_port_led_busy()) {
            /* the boot animation plays on the port LEDs: the power LED stays solid until it is over */
            solid = 1;
        }
        else if (connect_ms && connect_ms < IBLUE_LED_CONNECT_MAX_MS) {
            /* a controller is connecting: blink in the chosen colour until it is ready */
            level = ((connect_ms / IBLUE_LED_CONNECT_BLINK_MS) % 2) ? 0 : IBLUE_LED_MAX_DUTY;
        }
        else if (bt_hci_get_inquiry()) {
            /* Triangle "breathing" between 5% and 100%. It starts at full brightness (where the solid LED
             * was), goes down and comes back up, so it joins the solid LED without a jump. */
            uint32_t ph = breath_t % IBLUE_LED_BREATH_MS;
            uint32_t half = IBLUE_LED_BREATH_MS / 2;
            uint32_t tri = (ph < half) ? ph : (IBLUE_LED_BREATH_MS - ph);
            uint32_t lin = 1000 - (tri * 1000) / half; /* 1000 at the start, 0 at the bottom */
            level = 50 + ((lin * lin / 1000) * (IBLUE_LED_MAX_DUTY - 50)) / 1000;
            breathing = 1;
        }
        else {
            solid = 1;
        }

        if (solid) {
            /* settle to solid with a quick fade: fast at first, slow at the end */
            shown += (1.0f - shown) * IBLUE_LED_FADE_STEP;
            if (shown > 0.995f) {
                shown = 1.0f;
            }
            level = (uint32_t)(powf(shown, 2.2f) * (float)IBLUE_LED_MAX_DUTY + 0.5f);
        }
        else {
            /* the other patterns set the level themselves; the fade starts from what is shown */
            shown = powf((float)level / (float)IBLUE_LED_MAX_DUTY, 1.0f / 2.2f);
        }

        breath_t = breathing ? breath_t + IBLUE_LED_TICK_MS : 0;
        set_level(level);
        vTaskDelay(IBLUE_LED_TICK_MS / portTICK_PERIOD_MS);
        t += IBLUE_LED_TICK_MS;
    }
}

static void apply_preset(uint32_t idx) {
    if (idx >= IBLUE_LED_COLOR_MAX) {
        idx = 0;
    }
    color_idx = idx;
    if (idx == IBLUE_LED_CUSTOM) {
        rgb[0] = custom_rgb[0];
        rgb[1] = custom_rgb[1];
        rgb[2] = custom_rgb[2];
    }
    else {
        rgb[0] = presets[idx][0];
        rgb[1] = presets[idx][1];
        rgb[2] = presets[idx][2];
    }
}

static uint32_t load_color(void) {
    uint32_t idx =
#ifdef CONFIG_BLUERETRO_IBLUE_LED_BLUE
        IBLUE_LED_BLUE;
#else
        IBLUE_LED_ORANGE;
#endif
    nvs_handle_t nvs;
    if (nvs_open(NVS_NS, NVS_READONLY, &nvs) == ESP_OK) {
        uint8_t val;
        uint32_t val32;
        if (nvs_get_u32(nvs, NVS_KEY_RGB, &val32) == ESP_OK) {
            custom_rgb[0] = (val32 >> 16) & 0xFF;
            custom_rgb[1] = (val32 >> 8) & 0xFF;
            custom_rgb[2] = val32 & 0xFF;
            has_custom = 1;
        }
        if (nvs_get_u8(nvs, NVS_KEY, &val) == ESP_OK && val < IBLUE_LED_COLOR_MAX) {
            idx = val;
        }
        nvs_close(nvs);
    }
    return idx;
}

static void save_color(uint32_t idx) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NS, NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_u8(nvs, NVS_KEY, (uint8_t)idx);
        if (has_custom) {
            nvs_set_u32(nvs, NVS_KEY_RGB, ((uint32_t)custom_rgb[0] << 16)
                | ((uint32_t)custom_rgb[1] << 8) | custom_rgb[2]);
        }
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

void iblue_led_init(void) {
    ledc_timer_config_t timer = {
        .speed_mode = IBLUE_LED_MODE,
        .duty_resolution = IBLUE_LED_RES,
        .timer_num = IBLUE_LED_TIMER,
        .freq_hz = IBLUE_LED_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    if (init_done) {
        return;
    }
    ledc_timer_config(&timer);

    apply_preset(load_color());

    for (uint32_t i = 0; i < 3; i++) {
        ledc_channel_config_t ch = {
            .gpio_num = pins[i],
            .speed_mode = IBLUE_LED_MODE,
            .channel = chans[i],
            .timer_sel = IBLUE_LED_TIMER,
            /* the final level at once: the chip may have lit the LED while it starts, and it stays lit (the
             * channel is not set up dark and then raised) */
            .duty = (rgb[i] * (IBLUE_LED_MAX_DUTY * IBLUE_LED_CAP_PCT / 100)) / 255,
            .hpoint = 0,
            .flags.output_invert = 1, /* common anode: low = on */
        };
        ledc_channel_config(&ch);
    }
    set_level(IBLUE_LED_MAX_DUTY); /* on fully now, before the task has run once */
    init_done = 1;
    printf("# %s: RGB LED on R:%d G:%d B:%d colour %ld\n", __FUNCTION__,
        pins[0], pins[1], pins[2], color_idx);

    /* Above wl_init_task (10): its file system, NVS and Bluetooth set-up keeps core 0 busy for seconds, and a
     * LED task below it is not scheduled until that is over. This one sleeps most of the time. */
    xTaskCreatePinnedToCore(iblue_led_task, "iblue_led", 2048, NULL, 12, NULL, 0);
}

void iblue_led_notice(uint32_t count) {
    notice_total = count * 16;
    notice_ticks = notice_total;
}

void iblue_led_connecting(uint32_t on) {
    connecting = on;
}

void iblue_led_btn(uint32_t active, uint32_t stage) {
    btn_stage = stage;
    btn_active = active;
}

void iblue_led_set_color(uint32_t idx) {
    apply_preset(idx);
    save_color(color_idx);
}

void iblue_led_toggle_color(void) {
    uint32_t next = (color_idx + 1) % IBLUE_LED_COLOR_MAX;

    if (next == IBLUE_LED_CUSTOM && !has_custom) {
        next = 0;
    }
    iblue_led_set_color(next);
    printf("# %s: colour %ld\n", __FUNCTION__, color_idx);
}

void iblue_led_set_rgb(uint8_t r, uint8_t g, uint8_t b) {
    rgb[0] = r;
    rgb[1] = g;
    rgb[2] = b;
}

void iblue_led_save_custom(void) {
    custom_rgb[0] = rgb[0];
    custom_rgb[1] = rgb[1];
    custom_rgb[2] = rgb[2];
    has_custom = 1;
    color_idx = IBLUE_LED_CUSTOM;
    save_color(IBLUE_LED_CUSTOM);
    printf("# %s: custom colour %02X%02X%02X saved\n", __FUNCTION__, rgb[0], rgb[1], rgb[2]);
}

void iblue_led_sweep_start(void) {
    sweep_t = 0;
    sweeping = 1;
}

void iblue_led_sweep_stop(void) {
    if (sweeping) {
        sweeping = 0;
        iblue_led_save_custom();
    }
}

void iblue_led_get_rgb(uint8_t *out) {
    out[0] = rgb[0];
    out[1] = rgb[1];
    out[2] = rgb[2];
}

uint32_t iblue_led_get_preset(void) {
    return color_idx;
}

void iblue_led_sleep(void) {
    sleeping = 1;
    if (init_done) {
        for (uint32_t i = 0; i < 3; i++) {
            ledc_set_duty(IBLUE_LED_MODE, chans[i], 0);
            ledc_update_duty(IBLUE_LED_MODE, chans[i]);
        }
    }
    /* LEDC stops in deep sleep: latch the pins high (= LED off) */
    for (uint32_t i = 0; i < 3; i++) {
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pins[i]], PIN_FUNC_GPIO);
        gpio_set_direction(pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(pins[i], 1);
        gpio_hold_en(pins[i]);
    }
}

#endif /* CONFIG_BLUERETRO_IBLUE_RGB_LED */
