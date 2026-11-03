/*
 * iBlueControlMod: Joy-Con motion as a position (see iblue_motion.h): the settings and the calibration kept in NVS,
 * one motion state per connected controller, the poses on request (from the web page or by holding a button).
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _IBLUE_MOTION_CTL_H_
#define _IBLUE_MOTION_CTL_H_

#include <stdint.h>
#include "sdkconfig.h"
#include "iblue/iblue_motion.h"

#define IBLUE_MOTION_DEVS 4          /* the controllers that are followed */
#define IBLUE_MOTION_IMU_OFFSET 12   /* in a Switch 0x30 report: three samples of accel xyz and gyro xyz, 16 bit each */
#define IBLUE_MOTION_IMU_LEN 36

#ifdef CONFIG_BLUERETRO_IBLUE_MOTION
void iblue_motion_ctl_init(void);
int iblue_motion_ctl_mode(void);                                   /* 1: motion mode is on */
void iblue_motion_ctl_get_cfg(struct iblue_motion_cfg *c);
void iblue_motion_ctl_set_cfg(const struct iblue_motion_cfg *c);   /* clamped, kept in NVS */
void iblue_motion_ctl_reset_cal(void);                             /* forget the saved axes and ranges */
/* Ask for a pose after delay_s seconds (all followed controllers must be held still then). */
void iblue_motion_ctl_request_pose(int pose, int delay_s);
/* A report of controller `dev` (side -1 left Joy-Con, +1 right). Returns 1 if x, y, shake are valid. */
int iblue_motion_ctl_feed(int dev, int side, const uint8_t *imu, int cal_pressed, int16_t *x, int16_t *y, int *shake);
void iblue_motion_ctl_gone(int dev);                                /* the controller disconnected */
/* Status of up to IBLUE_MOTION_DEVS controllers, 6 bytes each: side, flags (1 forward known, 2 pointing axis learned,
 * 4 shaking, 8 range learned, 16 last pose failed), x, y (-127..127), height deg, heading deg / 2. Returns the count. */
int iblue_motion_ctl_status(uint8_t *out, int max);
#else
#define iblue_motion_ctl_init()
#define iblue_motion_ctl_mode() 0
#define iblue_motion_ctl_gone(dev)
#endif

#endif
