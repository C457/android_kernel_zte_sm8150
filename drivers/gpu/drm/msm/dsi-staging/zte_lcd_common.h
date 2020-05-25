/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _ZTE_LCD_COMMON_H_
#define _ZTE_LCD_COMMON_H_
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/ctype.h>
#include <linux/debugfs.h>
#include <linux/sysfs.h>
#include <linux/proc_fs.h>
#include <linux/kobject.h>
#include <linux/mutex.h>
#ifdef CONFIG_ZTE_LCD_HBM_CTRL
#include <video/mipi_display.h>
#include <drm/drm_mipi_dsi.h>
#include "../../../msm/kgsl_device.h"
#endif

#include "dsi_panel.h"

#ifdef CONFIG_ZTE_LCD_REG_DEBUG
#define ZTE_REG_LEN 64
#define REG_MAX_LEN 16 /*one lcd reg display info max length*/
enum {	/* read or write mode */
	REG_WRITE_MODE = 0,
	REG_READ_MODE
};
struct zte_lcd_reg_debug {
	int is_read_mode;  /*if 1 read ,0 write*/
	unsigned char length;
	char rbuf[ZTE_REG_LEN];
	char wbuf[ZTE_REG_LEN];
};
/*WARNING: Single statement macros should not use a do {} while (0) loop*/
#define ZTE_LCD_INFO(fmt, args...) {pr_info("[MSM_LCD][Info]"fmt, ##args); }
#define ZTE_LCD_ERROR(fmt, args...) {pr_err("[MSM_LCD][Error]"fmt, ##args); }
#endif

#ifdef CONFIG_ZTE_LCD_AOD_BRIGHTNESS_CTRL
int panel_set_aod_brightness(struct dsi_panel *panel, u32 level);
#endif

#endif /* _ZTE_LCD_COMMON_H_ */
