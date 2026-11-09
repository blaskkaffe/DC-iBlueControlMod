/* iBlueControlMod: the port LEDs. See iblue_port_led.h. */

#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include "driver/ledc.h"
#include "esp_rom_gpio.h"
#include "hal/ledc_hal.h"
#include "iblue/iblue_anim_curve.h"
#include "iblue/iblue_anim_cfg.h"
#include "iblue/iblue_port_led.h"

#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED

#define MODE LEDC_HIGH_SPEED_MODE
#define TIMER LEDC_TIMER_3
#define FIRST_CHANNEL LEDC_CHANNEL_4
#define FREQ_HZ 5000
#define FRAME_MS 20
#define MAX_DUTY 1023
#define GAMMA 2.2f
#define HOLD_DUTY (MAX_DUTY * CONFIG_BLUERETRO_IBLUE_LED_MAX_PCT / 100) /* A while the animation is not started yet */
#define HOLD_MAX_US 500000 /* ... but not longer than this */
/* Per frame, how much of the way to the goal the level goes: 1 - exp(-20 ms / 80 ms) up (95 % after
 * 0.24 s), slower down. The level is what the eye sees, the duty is that to the power 2.2. */
#define STEP_UP 0.22f
#define STEP_DOWN 0.12f
#define ANIM_MIN_LEFT_MS 400 /* not worth starting for less */

static uint8_t led_pin[IBLUE_ANIM_LEDS];
static uint32_t led_count;
static volatile bool want_on[IBLUE_ANIM_LEDS];
static float level[IBLUE_ANIM_LEDS];
static volatile bool anim_active;
static volatile int64_t anim_t0_us;
static volatile uint32_t anim_run_ms;    /* how long the whole animation (intro and drops) runs from its start */
static volatile uint32_t anim_drops_ms;  /* ... of which the drops get this */
static volatile bool hold;
static volatile int64_t hold_until_us;
static volatile uint32_t intro_ms; /* length of the intro of the running animation, 0 = none */
static bool inited;

static void attach(uint32_t i) {
    esp_rom_gpio_connect_out_signal(led_pin[i], ledc_periph_signal[MODE].sig_out0_idx + FIRST_CHANNEL + i, 0, 0);
}

static void put(uint32_t i, uint32_t duty) {
    ledc_set_duty(MODE, FIRST_CHANNEL + i, duty);
    ledc_update_duty(MODE, FIRST_CHANNEL + i);
}

static void port_led_task(void *arg) {
    (void)arg;
    while (1) {
        float cap = (float)iblue_anim_get_cfg()->ctrl_pct / 100.0f; /* brightness of a connected controller */

        if (hold) {
            /* A is lit by the chip while it starts and stays lit until the animation (or a timeout) takes over */
            if (esp_timer_get_time() > hold_until_us) {
                hold = false;
            }
            else {
                for (uint32_t i = 0; i < led_count; i++) {
                    put(i, i == 0 ? HOLD_DUTY : 0);
                }
                vTaskDelay(FRAME_MS / portTICK_PERIOD_MS);
                continue;
            }
        }
        if (anim_active) {
            uint32_t run = (uint32_t)((esp_timer_get_time() - anim_t0_us) / 1000);
            uint32_t intro = intro_ms;

            if (run >= anim_run_ms) {
                anim_active = false; /* the real state takes over below, from dark */
                for (uint32_t i = 0; i < led_count; i++) {
                    level[i] = 0.0f;
                }
            }
            else if (run < intro) {
                /* the fireworks, then the drops */
                for (uint32_t i = 0; i < led_count; i++) {
                    put(i, iblue_anim_intro_duty(run, (int)i) * CONFIG_BLUERETRO_IBLUE_LED_MAX_PCT / 100);
                }
            }
            else {
                /* the drops have an animation of their own: it starts when the intro is over and ends with the whole */
                for (uint32_t i = 0; i < led_count; i++) {
                    put(i, iblue_anim_duty(run - intro, anim_drops_ms, (int)i) * CONFIG_BLUERETRO_IBLUE_LED_MAX_PCT / 100);
                }
            }
        }
        if (!anim_active) {
            for (uint32_t i = 0; i < led_count; i++) {
                float goal = want_on[i] ? 1.0f : 0.0f;

                level[i] += (goal - level[i]) * (goal > level[i] ? STEP_UP : STEP_DOWN);
                if (fabsf(goal - level[i]) < 0.004f) {
                    level[i] = goal;
                }
                put(i, (uint32_t)(powf(level[i], GAMMA) * cap * (float)MAX_DUTY + 0.5f));
            }
        }
        vTaskDelay(FRAME_MS / portTICK_PERIOD_MS);
    }
}

void iblue_port_led_init(const uint8_t *pins, uint32_t count) {
    ledc_timer_config_t timer = {
        .speed_mode = MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = TIMER,
        .freq_hz = FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };

    if (inited) {
        return;
    }
    if (count > IBLUE_ANIM_LEDS) {
        count = IBLUE_ANIM_LEDS;
    }
    led_count = count;
    ledc_timer_config(&timer);
    for (uint32_t i = 0; i < count; i++) {
        ledc_channel_config_t ch = {
            .gpio_num = pins[i],
            .speed_mode = MODE,
            .channel = FIRST_CHANNEL + i,
            .timer_sel = TIMER,
#ifdef CONFIG_BLUERETRO_IBLUE_BOOT_INTRO
            /* A is lit by the chip while it starts and the intro begins with A lit: keep it on, no dip */
            .duty = (i == 0) ? (MAX_DUTY * CONFIG_BLUERETRO_IBLUE_LED_MAX_PCT / 100) : 0,
#else
            .duty = 0,
#endif
            .hpoint = 0,
        };

        led_pin[i] = pins[i];
        ledc_channel_config(&ch);
    }
    inited = true;
#ifdef CONFIG_BLUERETRO_IBLUE_BOOT_INTRO
    /* A stays lit (the chip lit it) until the animation starts, a moment from now */
    hold_until_us = esp_timer_get_time() + HOLD_MAX_US;
    hold = true;
#endif
    /* Above wl_init_task (10), which keeps core 0 busy for seconds while it starts: below it this task would
     * not run until that is over and the intro and the first part of the drops would be skipped. */
    xTaskCreatePinnedToCore(port_led_task, "iblue_port_led", 2048, NULL, 12, NULL, 0);
}

void iblue_port_led_set(uint32_t index, bool on) {
    if (!inited || index >= led_count) {
        return;
    }
    want_on[index] = on;
    attach(index);
}

void iblue_port_led_boot_anim(uint32_t elapsed_ms) {
    uint32_t total = (uint32_t)iblue_anim_get_cfg()->total_s * 1000u;
    uint32_t remain;

    hold = false;
    if (!inited || anim_active || elapsed_ms + ANIM_MIN_LEFT_MS >= total) {
        return;
    }
    remain = total - elapsed_ms; /* the animation ends `total` after the console's power-on */
    for (uint32_t i = 0; i < led_count; i++) {
        attach(i);
    }
#ifdef CONFIG_BLUERETRO_IBLUE_BOOT_INTRO
    /* the intro only when the drops still get a good time after it */
    intro_ms = (remain > IBLUE_ANIM_INTRO_MS + 2000) ? IBLUE_ANIM_INTRO_MS : 0;
#else
    intro_ms = 0;
#endif
    anim_run_ms = remain;
    anim_drops_ms = remain - intro_ms;
    anim_t0_us = esp_timer_get_time();
    anim_active = true;
}

void iblue_port_led_play(void) {
    iblue_port_led_boot_anim(0); /* from the start, to see the settings */
}

void iblue_port_led_reclaim(void) {
    for (uint32_t i = 0; inited && i < led_count; i++) {
        attach(i);
    }
}

bool iblue_port_led_busy(void) {
    return anim_active;
}

#endif /* CONFIG_BLUERETRO_IBLUE_RGB_LED */
