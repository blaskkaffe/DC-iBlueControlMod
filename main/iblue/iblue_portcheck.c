/* iBlueControlMod: the port check. See iblue_portcheck.h. */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <rom/ets_sys.h>
#include "soc/gpio_struct.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_periph.h"
#include "soc/io_mux_reg.h"
#include "soc/rtc_io_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "system/manager.h"
#include "wired/maple.h"
#include "adapter/config.h"
#include "iblue/iblue_portcheck.h"

#ifdef CONFIG_BLUERETRO_HW_IBLUECONTROLMOD

#define PORTS 4
#define REPORT_MAX 2400
#define SAMPLE_US 200000

static char report[REPORT_MAX];
static uint32_t report_len;
static volatile uint8_t state = IBLUE_PORTCHECK_NONE;

static void add(const char *fmt, ...) {
    va_list ap;
    int n;

    if (report_len >= REPORT_MAX - 1) {
        return;
    }
    va_start(ap, fmt);
    n = vsnprintf(report + report_len, REPORT_MAX - report_len, fmt, ap);
    va_end(ap);
    if (n > 0) {
        report_len += (uint32_t)n;
        if (report_len > REPORT_MAX - 1) {
            report_len = REPORT_MAX - 1;
        }
    }
}

static inline uint32_t level(uint32_t pin) {
    return pin < 32 ? (GPIO.in >> pin) & 1 : (GPIO.in1.data >> (pin - 32)) & 1;
}

static inline uint32_t out_enabled(uint32_t pin) {
    return pin < 32 ? (GPIO.enable >> pin) & 1 : (GPIO.enable1.data >> (pin - 32)) & 1;
}

/* the RTC pad register of a pin, or 0 (the pins of the ports that are RTC pins) */
static uint32_t rtc_pad_reg(uint32_t pin) {
    switch (pin) {
        case 25: return RTC_IO_PAD_DAC1_REG;
        case 26: return RTC_IO_PAD_DAC2_REG;
        case 27: return RTC_IO_TOUCH_PAD7_REG;
        case 32:
        case 33: return RTC_IO_XTAL_32K_PAD_REG;
        case 12: return RTC_IO_TOUCH_PAD5_REG;
        default: return 0;
    }
}

/* the pin's pulls: IO_MUX for a digital pin, the RTC pad for an RTC pin (as gpio.c does) */
static void set_pulls(uint32_t pin, int up, int down) {
    uint32_t mux = GPIO_PIN_MUX_REG[pin];
    uint32_t up_m = 0, down_m = 0, reg = 0;

    switch (pin) {
        case 26: reg = RTC_IO_PAD_DAC2_REG; up_m = RTC_IO_PDAC2_RUE_M; down_m = RTC_IO_PDAC2_RDE_M; break;
        case 27: reg = RTC_IO_TOUCH_PAD7_REG; up_m = RTC_IO_TOUCH_PAD7_RUE_M; down_m = RTC_IO_TOUCH_PAD7_RDE_M; break;
        case 32: reg = RTC_IO_XTAL_32K_PAD_REG; up_m = RTC_IO_X32P_RUE_M; down_m = RTC_IO_X32P_RDE_M; break;
        case 33: reg = RTC_IO_XTAL_32K_PAD_REG; up_m = RTC_IO_X32N_RUE_M; down_m = RTC_IO_X32N_RDE_M; break;
        default: break;
    }
    if (reg) {
        if (up) { SET_PERI_REG_MASK(reg, up_m); } else { CLEAR_PERI_REG_MASK(reg, up_m); }
        if (down) { SET_PERI_REG_MASK(reg, down_m); } else { CLEAR_PERI_REG_MASK(reg, down_m); }
    }
    else if (pin < 34) {
        if (up) { REG_SET_BIT(mux, FUN_PU); } else { REG_CLR_BIT(mux, FUN_PU); }
        if (down) { REG_SET_BIT(mux, FUN_PD); } else { REG_CLR_BIT(mux, FUN_PD); }
    }
}

static void pin_regs(const char *name, uint32_t pin) {
    uint32_t rtc = rtc_pad_reg(pin);

    add(" %s=GPIO%u in%u oe%u mux=%08x pin=%08x osel=%08x", name, (unsigned)pin, (unsigned)level(pin), (unsigned)out_enabled(pin),
        (unsigned)REG_READ(GPIO_PIN_MUX_REG[pin]), (unsigned)REG_READ(GPIO_PIN0_REG + 4 * pin),
        (unsigned)(pin < 34 ? REG_READ(GPIO_FUNC0_OUT_SEL_CFG_REG + 4 * pin) : 0));
    if (rtc) {
        add(" rtc=%08x", (unsigned)REG_READ(rtc));
    }
    add("\n");
}

/* level with the pull-down only, then the pull-up only (each bit: 1 = high); restores the pulls it found */
static uint32_t pull_test(uint32_t pin) {
    uint32_t mux = GPIO_PIN_MUX_REG[pin];
    uint32_t mux_before = REG_READ(mux);
    uint32_t rtc = rtc_pad_reg(pin);
    uint32_t rtc_before = rtc ? REG_READ(rtc) : 0;
    uint32_t r = 0;

    PIN_INPUT_ENABLE(mux);
    set_pulls(pin, 0, 1);
    ets_delay_us(200);
    r |= level(pin);
    set_pulls(pin, 1, 0);
    ets_delay_us(200);
    r |= level(pin) << 1;
    REG_WRITE(mux, mux_before);
    if (rtc) {
        REG_WRITE(rtc, rtc_before);
    }
    return r;
}

static void check_task(void *arg) {
    uint16_t claimed = sys_mgr_port_state();
    uint32_t rx[PORTS], tx[PORTS], ab[PORTS];

    (void)arg;
    report_len = 0;
    for (uint32_t p = 0; p < PORTS; p++) {
        rx[p] = maple_diag_rx[p];
        tx[p] = maple_diag_tx[p];
        ab[p] = maple_diag_abort[p];
    }
    add("iBlue port check 1, up %us\n", (unsigned)(esp_timer_get_time() / 1000000));
    add("answers on: %04x (bit0=A); ports %u\n", (unsigned)claimed, (unsigned)hw_config.port_cnt);

    for (uint32_t p = 0; p < PORTS && p < hw_config.port_cnt; p++) {
        uint8_t a, b;
        uint32_t s = sys_mgr_sense_pin(p);
        uint32_t mux_a, mux_b, edges_a = 0, edges_b = 0, high_a = 0, high_b = 0, n = 0;
        uint32_t la, lb, pa = 0xFF, pb = 0xFF, ps = 0xFF;
        int64_t end, next_yield;
        int mine = claimed != 0xFFFF && (claimed & (1u << p));

        maple_port_pins(p, &a, &b);
        add("port %c: %s, sense GPIO%u=%u (%s)\n", 'A' + (int)p, mine ? "ADAPTER ANSWERS HERE" : "adapter quiet", (unsigned)s,
            (unsigned)level(s), sys_mgr_sense_empty(p) ? "empty" : "plugged");
        pin_regs("m0", a);
        pin_regs("m1", b);
        pin_regs("sn", s);

        /* the console's traffic on the two Maple pins (input on for a quiet port, as it is reset with input off) */
        mux_a = REG_READ(GPIO_PIN_MUX_REG[a]);
        mux_b = REG_READ(GPIO_PIN_MUX_REG[b]);
        PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[a]);
        PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[b]);
        ets_delay_us(10);
        la = level(a);
        lb = level(b);
        end = esp_timer_get_time() + SAMPLE_US;
        next_yield = esp_timer_get_time() + 2000;
        while (esp_timer_get_time() < end) {
            uint32_t va = level(a), vb = level(b);

            edges_a += va != la;
            edges_b += vb != lb;
            high_a += va;
            high_b += vb;
            la = va;
            lb = vb;
            n++;
            if (esp_timer_get_time() > next_yield) {
                vTaskDelay(1);
                next_yield = esp_timer_get_time() + 2000;
            }
        }
        if (!mine) {
            pa = pull_test(a);
            pb = pull_test(b);
        }
        if (s < 34) {
            ps = pull_test(s);
        }
        REG_WRITE(GPIO_PIN_MUX_REG[a], mux_a);
        REG_WRITE(GPIO_PIN_MUX_REG[b], mux_b);
        add(" traffic 200ms: m0 %u edges %u%% high, m1 %u edges %u%% high (%u samples)\n", (unsigned)edges_a,
            (unsigned)(n ? high_a * 100 / n : 0), (unsigned)edges_b, (unsigned)(n ? high_b * 100 / n : 0), (unsigned)n);
        add(" pulls (down,up): m0 %s m1 %s sn %s\n",
            pa == 0xFF ? "-" : (pa == 2 ? "follow" : (pa == 3 ? "HIGH" : (pa == 0 ? "LOW" : "odd"))),
            pb == 0xFF ? "-" : (pb == 2 ? "follow" : (pb == 3 ? "HIGH" : (pb == 0 ? "LOW" : "odd"))),
            ps == 0xFF ? "-" : (ps == 2 ? "follow" : (ps == 3 ? "HIGH" : (ps == 0 ? "LOW" : "odd"))));
        add(" maple since boot: rx %u tx %u abort %u\n", (unsigned)rx[p], (unsigned)tx[p], (unsigned)ab[p]);
    }
    add("gpio en=%08x en1=%08x out=%08x in=%08x in1=%08x\n", (unsigned)GPIO.enable, (unsigned)GPIO.enable1.data,
        (unsigned)GPIO.out, (unsigned)GPIO.in, (unsigned)GPIO.in1.data);
    add("hold=%08x dighold=%08x touch=%08x dac=%08x\n", (unsigned)REG_READ(RTC_CNTL_HOLD_FORCE_REG),
        (unsigned)REG_READ(RTC_IO_DIG_PAD_HOLD_REG), (unsigned)REG_READ(SENS_SAR_TOUCH_ENABLE_REG),
        (unsigned)REG_READ(SENS_SAR_DAC_CTRL2_REG));
    state = IBLUE_PORTCHECK_DONE;
    printf("# iblue port check:\n%s", report);
    vTaskDelete(NULL);
}

int iblue_portcheck_start(void) {
    if (state == IBLUE_PORTCHECK_RUNNING) {
        return 0;
    }
    state = IBLUE_PORTCHECK_RUNNING;
    if (xTaskCreatePinnedToCore(check_task, "iblue_pcheck", 4096, NULL, 1, NULL, 0) != pdPASS) {
        state = IBLUE_PORTCHECK_NONE;
        return -1;
    }
    return 0;
}

uint8_t iblue_portcheck_state(void) {
    return state;
}

uint32_t iblue_portcheck_len(void) {
    return state == IBLUE_PORTCHECK_DONE ? report_len : 0;
}

uint32_t iblue_portcheck_read(uint32_t offset, uint8_t *out, uint32_t max) {
    uint32_t len = iblue_portcheck_len();

    if (offset >= len) {
        return 0;
    }
    if (max > len - offset) {
        max = len - offset;
    }
    memcpy(out, report + offset, max);
    return max;
}

#endif /* CONFIG_BLUERETRO_HW_IBLUECONTROLMOD */
