/*
 * Copyright (c) 2019-2023, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _SYS_MANAGER_H_
#define _SYS_MANAGER_H_ 

#include <stdint.h>

enum {
    SYS_MGR_CMD_RST = 0,
    SYS_MGR_CMD_PWR_ON,
    SYS_MGR_CMD_PWR_OFF,
    SYS_MGR_CMD_INQ_TOOGLE,
    SYS_MGR_CMD_FACTORY_RST,
    SYS_MGR_CMD_DEEP_SLEEP,
    SYS_MGR_CMD_ADAPTER_RST,
    SYS_MGR_CMD_WIRED_RST,
};

void sys_mgr_cmd(uint8_t cmd);
void sys_mgr_init(uint32_t package);
#ifdef CONFIG_BLUERETRO_IBLUE_RGB_LED
void sys_mgr_iblue_early(void);
#endif

#if defined(CONFIG_BLUERETRO_HW_IBLUECONTROLMOD) && (CONFIG_BLUERETRO_IBLUE_PAIR_WINDOW_S > 0)
#define IBLUE_PAIR_WINDOW 1
#include <stdbool.h>
void sys_mgr_pair_window_open(void);
bool sys_mgr_pairing_allowed(void);
#endif


#endif /* _SYS_MANAGER_H_ */
