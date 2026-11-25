/* iBlueControlMod: the controller port pins in a known state at start-up. See iblue_pins.h. */

#include <stdint.h>
#include "sdkconfig.h"
#include "soc/soc.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_sig_map.h"
#include "soc/io_mux_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "system/gpio.h"
#include "iblue/iblue_pins.h"

#ifdef CONFIG_BLUERETRO_HW_IBLUECONTROLMOD

/* All in IRAM / DRAM (linker.lf): maple_init calls it, which runs on the core without the flash cache guard. */

/* the touch sensor number + 1 of a pin, or 0 (touch 8 and 9 are GPIO33 and GPIO32) */
static const uint8_t touch_map[40] = {
    [4] = 1, [0] = 2, [2] = 3, [15] = 4, [13] = 5, [12] = 6, [14] = 7, [27] = 8, [33] = 9, [32] = 10,
};

/* the hold bit of a digital pad in RTC_IO_DIG_PAD_HOLD_REG (ESP-IDF's GPIO_HOLD_MASK for the ESP32) */
static const uint32_t dig_hold_mask[40] = {
    [1] = BIT(1), [3] = BIT(0), [5] = BIT(8), [6] = BIT(2), [7] = BIT(3), [8] = BIT(4), [9] = BIT(5), [10] = BIT(6),
    [11] = BIT(7), [16] = BIT(9), [17] = BIT(10), [18] = BIT(11), [19] = BIT(12), [21] = BIT(14), [22] = BIT(15),
    [23] = BIT(16),
};

static int touch_num(uint32_t pin) {
    return pin < 40 ? (int)touch_map[pin] - 1 : -1;
}

static void analog_off(uint32_t pin) {
    int t = touch_num(pin);

    /* the DACs drive their pad when powered */
    if (pin == 25) {
        CLEAR_PERI_REG_MASK(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_XPD_DAC_M | RTC_IO_PDAC1_DAC_XPD_FORCE_M | RTC_IO_PDAC1_DAC_M);
        CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN1_M);
    }
    else if (pin == 26) {
        CLEAR_PERI_REG_MASK(RTC_IO_PAD_DAC2_REG, RTC_IO_PDAC2_XPD_DAC_M | RTC_IO_PDAC2_DAC_XPD_FORCE_M | RTC_IO_PDAC2_DAC_M);
        CLEAR_PERI_REG_MASK(SENS_SAR_DAC_CTRL2_REG, SENS_DAC_CW_EN2_M);
    }
    /* the touch sensor charges and discharges its pad when it is on */
    if (t >= 0) {
        uint32_t reg = RTC_IO_TOUCH_PAD0_REG + 4 * (uint32_t)t;

        if (t <= 7) {
            CLEAR_PERI_REG_MASK(reg, RTC_IO_TOUCH_PAD0_XPD_M | RTC_IO_TOUCH_PAD0_TIE_OPT_M | RTC_IO_TOUCH_PAD0_TO_GPIO_M);
        }
        else {
            CLEAR_PERI_REG_MASK(reg, RTC_IO_TOUCH_PAD8_XPD_M | RTC_IO_TOUCH_PAD8_TO_GPIO_M);
        }
        CLEAR_PERI_REG_MASK(SENS_SAR_TOUCH_ENABLE_REG, (BIT(t) << SENS_TOUCH_PAD_WORKEN_S) | (BIT(t) << SENS_TOUCH_PAD_OUTEN1_S)
            | (BIT(t) << SENS_TOUCH_PAD_OUTEN2_S));
    }
#ifndef CONFIG_RTC_CLK_SRC_EXT_CRYS
    /* the 32 kHz crystal oscillator drives GPIO32 / GPIO33 when on (this build runs on the internal RC) */
    if (pin == 32 || pin == 33) {
        CLEAR_PERI_REG_MASK(RTC_IO_XTAL_32K_PAD_REG, RTC_IO_XPD_XTAL_32K_M);
    }
#endif
}

void iblue_pin_clean(uint32_t pin, int input) {
    uint32_t mux;

    if (pin >= 40 || !GPIO_PIN_MUX_REG_IRAM[pin]) {
        return;
    }
    mux = GPIO_PIN_MUX_REG_IRAM[pin];
    analog_off(pin);
    /* an RTC pad back to the GPIO matrix, its hold off (gpio.c) */
    gpio_rtc_pad_digital(pin);
    /* a digital pad's hold off */
    CLEAR_PERI_REG_MASK(RTC_IO_DIG_PAD_HOLD_REG, dig_hold_mask[pin]);
    /* IO_MUX: GPIO function, no sleep settings, no pulls, input as asked */
    REG_CLR_BIT(mux, SLP_OE | SLP_SEL | SLP_PD | SLP_PU | SLP_IE | FUN_PU | FUN_PD);
    if (input) {
        REG_SET_BIT(mux, FUN_IE);
    }
    else {
        REG_CLR_BIT(mux, FUN_IE);
    }
    PIN_FUNC_SELECT(mux, PIN_FUNC_GPIO);
    if (pin < 34) {
        /* output off, plain GPIO output signal, push-pull, no interrupt, no wake-up */
        if (pin < 32) {
            REG_WRITE(GPIO_ENABLE_W1TC_REG, BIT(pin));
        }
        else {
            REG_WRITE(GPIO_ENABLE1_W1TC_REG, BIT(pin - 32));
        }
        REG_WRITE(GPIO_FUNC0_OUT_SEL_CFG_REG + 4 * pin, SIG_GPIO_OUT_IDX);
        REG_WRITE(GPIO_PIN0_REG + 4 * pin, 0);
    }
}

void iblue_pins_release_holds(void) {
    /* the hold of all digital pads (deep sleep) */
    CLEAR_PERI_REG_MASK(RTC_CNTL_DIG_ISO_REG, RTC_CNTL_DG_PAD_FORCE_HOLD_M | RTC_CNTL_DG_PAD_AUTOHOLD_EN_M);
    SET_PERI_REG_MASK(RTC_CNTL_DIG_ISO_REG, RTC_CNTL_CLR_DG_PAD_AUTOHOLD_M);
}

#endif /* CONFIG_BLUERETRO_HW_IBLUECONTROLMOD */
