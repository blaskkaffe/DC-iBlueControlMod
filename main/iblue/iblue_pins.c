/* iBlueControlMod: the controller port pins in a known state at start-up. See iblue_pins.h. */

#include <stdint.h>
#include "sdkconfig.h"
#include "soc/soc.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_periph.h"
#include "soc/io_mux_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "system/gpio.h"
#include "iblue/iblue_pins.h"

#ifdef CONFIG_BLUERETRO_HW_IBLUECONTROLMOD

/* the touch sensor number of a pin, or -1 (touch 8 and 9 are GPIO33 and GPIO32) */
static int touch_num(uint32_t pin) {
    static const int8_t map[40] = {
        [4] = 0, [0] = 1, [2] = 2, [15] = 3, [13] = 4, [12] = 5, [14] = 6, [27] = 7, [33] = 8, [32] = 9,
    };

    if (pin >= 40) {
        return -1;
    }
    if (map[pin] == 0 && pin != 4) {
        return -1;
    }
    return map[pin];
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
    uint32_t mux = GPIO_PIN_MUX_REG[pin];

    if (pin >= 40 || !mux) {
        return;
    }
    analog_off(pin);
    /* an RTC pad back to the GPIO matrix, its hold off (gpio.c) */
    gpio_rtc_pad_digital(pin);
    /* a digital pad's hold off */
    CLEAR_PERI_REG_MASK(RTC_IO_DIG_PAD_HOLD_REG, GPIO_HOLD_MASK[pin]);
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
