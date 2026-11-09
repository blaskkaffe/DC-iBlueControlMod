/*
 * Copyright (c) 2019-2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <soc/efuse_reg.h>
#include "zephyr/atomic.h"
#include "system/gpio.h"
#include "driver/ledc.h"
#include "adapter/config.h"
#include "driver/gpio.h"
#include "hal/ledc_hal.h"
#include "hal/gpio_hal.h"
#include "soc/io_mux_reg.h"
#include "esp_rom_gpio.h"
#include "sdkconfig.h"
#include "led.h"

#if defined(CONFIG_BLUERETRO_HW_IBLUECONTROLMOD)
/* iBlueControlMod: the HS ch0 "pulse" PWM goes to GPIO14, one side of the
 * status LED (other side on GPIO13). GPIO17 is NOT an LED on this board. */
#define ERR_LED_PIN 14
#define IBLUE_STATUS_LED_A_PIN 13
#define IBLUE_STATUS_LED_B_PIN 14
#elif defined(CONFIG_BLUERETRO_SYSTEM_SEA_BOARD)
#define ERR_LED_PIN 32
#else
#define ERR_LED_PIN 17
#endif
#define PICO_ERR_LED_PIN 20

/* LED flags */
enum {
    ERR_LED_SET = 0,
};

static atomic_t led_flags = 0;
static TaskHandle_t err_led_task_hdl;
static uint8_t err_led_pin = ERR_LED_PIN;

static void err_led_task(void *param) {
    while (1) {
        if (!atomic_test_bit(&led_flags, ERR_LED_SET)) {
            ledc_set_fade_time_and_start(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, hw_config.led_pulse_duty_max,
                hw_config.led_pulse_fade_time_ms, LEDC_FADE_NO_WAIT);
            vTaskDelay(hw_config.led_pulse_fade_cycle_delay_ms / portTICK_PERIOD_MS);
            ledc_set_fade_time_and_start(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, hw_config.led_pulse_duty_min,
                hw_config.led_pulse_fade_time_ms, LEDC_FADE_NO_WAIT);
            vTaskDelay(hw_config.led_pulse_fade_cycle_delay_ms / portTICK_PERIOD_MS);
        }
        else {
            vTaskSuspend(err_led_task_hdl);
        }
    }
}

void err_led_init(uint32_t package) {
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = hw_config.led_pulse_hz,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_channel_config_t ledc_channel = {
        .channel    = LEDC_CHANNEL_0,
        .duty       = hw_config.led_pulse_off_duty_cycle,
        .gpio_num   = ERR_LED_PIN,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER_0,
    };

    if (package == EFUSE_RD_CHIP_VER_PKG_ESP32PICOV302) {
        ledc_channel.gpio_num = PICO_ERR_LED_PIN;
        err_led_pin = PICO_ERR_LED_PIN;
    }

    ledc_timer_config(&ledc_timer);
#ifndef CONFIG_BLUERETRO_IBLUE_RGB_LED
    ledc_channel_config(&ledc_channel);
#endif
    /* (On the iBlueControlMod the error LED channel is not set up: GPIO14 is the green of the RGB LED and the
     * channel would take the pin from the chip's state, changing the colour, until iblue_led takes over.) */
    ledc_fade_func_install(0);
    ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, hw_config.led_pulse_off_duty_cycle, 0);

#ifdef CONFIG_BLUERETRO_HW_IBLUECONTROLMOD
    xTaskCreatePinnedToCore(&err_led_task, "err_led_task", 2048, NULL, 5, &err_led_task_hdl, 0);
#else
    xTaskCreatePinnedToCore(&err_led_task, "err_led_task", 768, NULL, 5, &err_led_task_hdl, 0);
#endif
    err_led_clear();
}

void err_led_cfg_update(void) {
    ledc_set_freq(LEDC_HIGH_SPEED_MODE, LEDC_TIMER_0, hw_config.led_pulse_hz);
}

#ifdef CONFIG_BLUERETRO_HW_IBLUECONTROLMOD
/* Vendor firmware turns the generic error LED API into no-ops and drives
 * the status LED from the system manager instead (see manager.c). */
void err_led_set(void) {}
void err_led_clear(void) {}
void err_led_pulse(void) {}

/* Vendor fw: pairing pattern, GPIO14 <- HS ch0 (pulse, inverted), GPIO13 <- LS ch1 */
void iblue_status_led_pairing(void) {
    esp_rom_gpio_connect_out_signal(IBLUE_STATUS_LED_B_PIN,
        ledc_periph_signal[LEDC_HIGH_SPEED_MODE].sig_out0_idx + LEDC_CHANNEL_0, 1, 0);
    esp_rom_gpio_connect_out_signal(IBLUE_STATUS_LED_A_PIN,
        ledc_periph_signal[LEDC_LOW_SPEED_MODE].sig_out0_idx + LEDC_CHANNEL_1, 0, 0);
}

/* Vendor fw: connected pattern, GPIO13 = GPIO low, GPIO14 <- LS ch1 */
void iblue_status_led_connected(void) {
    PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[IBLUE_STATUS_LED_A_PIN], PIN_FUNC_GPIO);
    gpio_set_direction(IBLUE_STATUS_LED_A_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(IBLUE_STATUS_LED_A_PIN, 0);
    esp_rom_gpio_connect_out_signal(IBLUE_STATUS_LED_B_PIN,
        ledc_periph_signal[LEDC_LOW_SPEED_MODE].sig_out0_idx + LEDC_CHANNEL_1, 0, 0);
}
#else
void err_led_set(void) {
    vTaskSuspend(err_led_task_hdl);
    ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, hw_config.led_pulse_on_duty_cycle, 0);
    atomic_set_bit(&led_flags, ERR_LED_SET);
}

void err_led_clear(void) {
    /* When error is set it stay on until power cycle */
    if (!atomic_test_bit(&led_flags, ERR_LED_SET)) {
        vTaskSuspend(err_led_task_hdl);
        ledc_set_duty_and_update(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, hw_config.led_pulse_off_duty_cycle, 0);
    }
}

void err_led_pulse(void) {
    vTaskResume(err_led_task_hdl);
}
#endif /* CONFIG_BLUERETRO_HW_IBLUECONTROLMOD */

uint32_t err_led_get_pin(void) {
    return err_led_pin;
}
