/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#include "zte_lcd_common.h"

struct dsi_panel *g_zte_ctrl_pdata;
#ifdef CONFIG_ZTE_LCD_REG_DEBUG
extern void zte_lcd_reg_debug_func(void);
#endif
/********************read lcm hardware info begin****************/
/*file path: proc/driver/lcd_id/ or proc/msm_lcd*/
static int zte_lcd_proc_info_show(struct seq_file *m, void *v)
{
	if (!g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_init_code_version) {
		seq_printf(m, "panel_name=%s\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_panel_name);
	} else {
		seq_printf(m, "panel_name=%s,version=%s\n",
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_panel_name,
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_init_code_version);
	}
	return 0;
}
static int zte_lcd_proc_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, zte_lcd_proc_info_show, NULL);
}
static const struct file_operations zte_lcd_common_func_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= zte_lcd_proc_info_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release		= single_release,
};
static int zte_lcd_proc_info_display(struct device_node *node)
{
	proc_create("driver/lcd_id", 0664, NULL, &zte_lcd_common_func_proc_fops);

	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_panel_name = of_get_property(node,
		"qcom,mdss-dsi-panel-name", NULL);
	if (!g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_panel_name) {
		pr_info("%s:%d, panel name not found!\n", __func__, __LINE__);
		return -ENODEV;
	}

	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_init_code_version = of_get_property(node,
		"zte,lcd-init-code-version", NULL);

	pr_info("[MSM_LCD]%s: Panel Name = %s,init code version=%s\n", __func__,
		g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_panel_name,
		g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_init_code_version);

	return 0;
}
/********************read lcm hardware info end***********************/

/********************HBM MODE begin****************/
/*file path: proc/driver/lcd_hbm/*/
#ifdef CONFIG_ZTE_LCD_HBM_CTRL

struct kgsl_device *ksgl_uevent_device = NULL;
#define PARTIAL_HBM_SOURCE_EM_THRESHOLD     (0x155 + 1)
#define PARTIAL_HBM_SOURCE_EM_THRESHOLD_240NIT    (0x400 + 1)

int panel_hbm_ctrl_kingdisplay_240nit(struct dsi_panel *panel, u32 setHbm)
{
	int i = 0;
	int err = 0;

	struct mipi_dsi_device *dsi;

	u8 cmd_default_page[2] = {0xfe, 0x00};

	u8 cmd_reg_addr_head[] = {0xfe, 0x1d, 0xfe, 0x71, 0xfb};
	u8 cmd_reg_addr[] = {
						0xfe, 0x0e, 0x10, 0x19, 0x38, 0x3a, 0x3d, 0x41, 0x45, 0x46,
						0x58, 0xfe, 0x83, 0xfe, 0x1d, 0xfe, 0x71, 0xfb
						};

	u8 source_em_data_head[] = {0x40, 0xb8, 0xd0, 0x01, 0xaa};
	u8 source_em_data[] = {
						0x74, 0x78, 0x04, 0x08, 0x38, 0xf2, 0x62, 0x00, 0x00, 0x00,
						0x78, 0x00, 0x00, 0x40, 0xf8, 0xd0, 0x02, 0xaa
						};

	u8 source_data_head[] = {0x40, 0xb8, 0xd0, 0x01, 0xaa};
	u8 source_data[] = {
						0x74, 0x60, 0x05, 0x20, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff,
						0x60, 0x00, 0x01, 0x40, 0xf8, 0xd0, 0x02, 0xaa
						};

	dsi = &panel->mipi_device;
	if (!dsi) {
		pr_info("HBM: No device");
		return -ENOMEM;
	}

	mutex_lock(&panel->panel_lock);
	if (setHbm == 0) {
			for (i = 0; i < sizeof(cmd_reg_addr_head); i++) {
				err = mipi_dsi_dcs_write(dsi, cmd_reg_addr_head[i], &source_em_data_head[i], 1);
				if (err < 0)
					goto error;
			}
			usleep_range(1000, 1100);
			for (i = 0; i < sizeof(cmd_reg_addr); i++) {
				err = mipi_dsi_dcs_write(dsi, cmd_reg_addr[i], &source_em_data[i], 1);
				if (err < 0)
					goto error;
			}

	} else {
			for (i = 0; i < sizeof(cmd_reg_addr_head); i++) {
				err = mipi_dsi_dcs_write(dsi, cmd_reg_addr_head[i], &source_data_head[i], 1);
				if (err < 0)
					goto error;
			}
			usleep_range(1000, 1100);
			for (i = 0; i < sizeof(cmd_reg_addr); i++) {
				err = mipi_dsi_dcs_write(dsi, cmd_reg_addr[i], &source_data[i], 1);
				if (err < 0)
					goto error;
			}
	}
	usleep_range(20000, 20100);
	err = mipi_dsi_dcs_write(dsi, cmd_default_page[0], &cmd_default_page[1], sizeof(cmd_default_page) - 1);
	if (err < 0)
		goto error;

	mutex_unlock(&panel->panel_lock);

	return err;

error:
	mutex_unlock(&panel->panel_lock);
	pr_err("HBM 240nit: send cmds failed");
	return err;

}

int panel_hbm_ctrl_kingdisplay(struct dsi_panel *panel, u32 setHbm)
{
	u8 cmd_enable[2] = {0x83, 0x01};
	u8 cmd_disable[2] = {0x83, 0x00};
	u8 cmd_default_page[2] = {0xfe, 0x00};
	u8 cmd_reg_addr[] = {0xfe, 0x38, 0x3a, 0x3d, 0x41, 0x46, 0xfe, 0x4c, 0xfe};
	u8 source_em_data[] = {0x74, 0x30, 0xf4, 0x4f, 0x00, 0x00, 0x72, 0x4e, 0x00};
	u8 source_data[] = {0x74, 0x00, 0xff, 0xff, 0xff, 0xff, 0x72, 0x4a, 0x00};
	int i = 0;
	int err = 0;

	struct mipi_dsi_device *dsi;

	dsi = &panel->mipi_device;
	if (!dsi) {
		pr_info("HBM: No device");
		return -ENOMEM;
	}

	mutex_lock(&panel->panel_lock);
	if (setHbm == 0) {
		if (panel->bl_config.real_bl_level_to_panel < PARTIAL_HBM_SOURCE_EM_THRESHOLD)
			for (i = 0; i < sizeof(cmd_reg_addr); i++) {
				err = mipi_dsi_dcs_write(dsi, cmd_reg_addr[i], &source_em_data[i], 1);
				if (err < 0)
					goto error;
			}
		err = mipi_dsi_dcs_write(dsi, cmd_disable[0], &cmd_disable[1], sizeof(cmd_disable) - 1);
		if (err < 0)
			goto error;
	} else {
		if (panel->bl_config.real_bl_level_to_panel < PARTIAL_HBM_SOURCE_EM_THRESHOLD)
			for (i = 0; i < sizeof(cmd_reg_addr); i++) {
				err = mipi_dsi_dcs_write(dsi, cmd_reg_addr[i], &source_data[i], 1);
				if (err < 0)
					goto error;
			}
		err = mipi_dsi_dcs_write(dsi, cmd_enable[0], &cmd_enable[1], sizeof(cmd_enable) - 1);
		if (err < 0)
			goto error;
	}
	usleep_range(20000, 20100);
	err = mipi_dsi_dcs_write(dsi, cmd_default_page[0], &cmd_default_page[1], sizeof(cmd_default_page) - 1);
	if (err < 0)
		goto error;

	mutex_unlock(&panel->panel_lock);

	return err;

error:
	mutex_unlock(&panel->panel_lock);
	pr_err("HBM: send cmds failed");
	return err;

}
static int zte_lcd_proc_hbm_show(struct seq_file *m, void *v)
{

	seq_printf(m, "%d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode);
	pr_info("HBM: read value = %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode);

	return 0;
}

static ssize_t zte_lcd_proc_hbm_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *f_pos)
{
	char *tmp = kzalloc((count+1), GFP_KERNEL);
	u32 mode;
	int ret = 0, retry_times = 3;
	char *envp[3];

	if (!tmp)
		return -ENOMEM;

	if (!g_zte_ctrl_pdata->panel_initialized) {
		pr_info("HBM: Panel not initialized\n");
		return -ENOMEM;
	}

	if (copy_from_user(tmp, buffer, count)) {
		kfree(tmp);
		return -EFAULT;
	}

	mode = *tmp - '0';
	if (g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode != mode) {
		while (retry_times--) {
			if (!strcmp(g_zte_ctrl_pdata->name, "Visionox-RM692A4-nootp240nit-1080-2340-6P5Inch"))
				ret = panel_hbm_ctrl_kingdisplay_240nit(g_zte_ctrl_pdata, mode);
			else
				ret = panel_hbm_ctrl_kingdisplay(g_zte_ctrl_pdata, mode);
			if (ret == 0) {
				g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode = mode;
				pr_info("HBM: Set new mode successful, new mode = %d\n",
						g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode);
				break;
			}
			pr_info("HBM: Set new mode failed, mode = %d, retry_times = %d\n",
						g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode, retry_times);
		}

		pr_info("HBM PanelName: %s\n", g_zte_ctrl_pdata->name);

		if (ret == 0) {
			if (mode == 0)
				envp[0] = "HBM_STATUS=OFF";
			else
				envp[0] = "HBM_STATUS=ON";
			envp[1] = "HBM_SET_RESULT=SUCCESSFUL";
		} else {
			if (mode == 0)
				envp[0] = "HBM_STATUS=ON";
			else
				envp[0] = "HBM_STATUS=OFF";
			envp[1] = "HBM_SET_RESULT=FAILED";
		}
		envp[2] = NULL;

		if (ksgl_uevent_device)
			kobject_uevent_env(&ksgl_uevent_device->dev->kobj, KOBJ_CHANGE, envp);
		else
			pr_info("HBM: ksgl_uevent_device is NULL\n");
	} else
		pr_info("HBM: New mode is same as old ,do nothing! mode = %d\n",
				g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode);

	kfree(tmp);
	return count;
}

static int zte_lcd_proc_hbm_open(struct inode *inode, struct file *file)
{
	return single_open(file, zte_lcd_proc_hbm_show, NULL);
}

static const struct file_operations zte_lcd_hbm_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= zte_lcd_proc_hbm_open,
	.read		= seq_read,
	.write		= zte_lcd_proc_hbm_write,
	.llseek		= seq_lseek,
	.release		= single_release,
};

static int zte_lcd_hbm_ctrl(struct device_node *node)
{
	proc_create("driver/lcd_hbm", 0664, NULL, &zte_lcd_hbm_proc_fops);

	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode = 0;
	pr_info("HBM:  = %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hbm_mode);

	return 0;
}
/*
setHdr flag for LCD brightness:
HDR OFF - 0
HDR ON - 1
AUTO SENSOR ON - 2
AUTO SENSOR OFF - 3
These should be same as the defination in sensor file, do not change.
*/
void panel_set_hdr_flag(struct dsi_panel *panel, u32 setHdr)
{
	u32 bl_lvl_bak = panel->bl_config.bl_level;
	char cal_value = g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hdr_on;

	switch (setHdr) {
	case 0:
		cal_value &= 0xfe;/* clear bit(0) */
		break;
	case 1:
		cal_value |= 0x01;/* set bit(0) */
		break;
	case 2:
		cal_value |= 0x02;/* set bit(1) */
		break;
	case 3:
		cal_value &= 0xfd;/* clear bit(1) */
		break;
	default:
		pr_err("HDR invalid flag:  %d\n", setHdr);
	break;
	}

	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hdr_on = cal_value;
	dsi_panel_set_backlight(panel, bl_lvl_bak);
	pr_info("HDR flag:  %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hdr_on);
}
static ssize_t zte_lcd_proc_hdr_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *f_pos)
{
	char *tmp = kzalloc((count+1), GFP_KERNEL);
	u32 mode;

	if (!tmp)
		return -ENOMEM;

	if (!g_zte_ctrl_pdata->panel_initialized) {
		pr_info("HBM: Panel not initialized\n");
		kfree(tmp);
		return -ENOMEM;
	}

	if (copy_from_user(tmp, buffer, count)) {
		kfree(tmp);
		return -EFAULT;
	}

	mode = *tmp - '0';

	panel_set_hdr_flag(g_zte_ctrl_pdata, mode);

	kfree(tmp);
	return count;
}
static int zte_lcd_proc_hdr_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hdr_on);
	pr_info("HDR flag:  %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hdr_on);
	return 0;
}
static int zte_lcd_proc_hdr_open(struct inode *inode, struct file *file)
{
	return single_open(file, zte_lcd_proc_hdr_show, NULL);
}
static const struct file_operations zte_lcd_hdr_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= zte_lcd_proc_hdr_open,
	.read		= seq_read,
	.write		= zte_lcd_proc_hdr_write,
	.llseek		= seq_lseek,
	.release		= single_release,
};
static int zte_lcd_set_hdr_flag(struct device_node *node)
{
	proc_create("driver/lcd_hdr", 0664, NULL, &zte_lcd_hdr_proc_fops);

	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hdr_on = 0;
	pr_info("HDR flag:  = %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_hdr_on);

	return 0;
}
#endif
/********************HBM MODE end***********************/

/********************AOD BRIGHTNESS begin****************/
/*file path: proc/driver/lcd_aod_bl/*/
#ifdef CONFIG_ZTE_LCD_AOD_BRIGHTNESS_CTRL
int panel_set_aod_brightness(struct dsi_panel *panel, u32 level)
{
	u8 reg_addr[] = {0xfe, 0x69, 0xfe};
	u8 data_level_l[] = {0x72, 0x04, 0x00};
	u8 data_level_m[] = {0x72, 0x30, 0x00};
	u8 data_level_h[] = {0x72, 0xff, 0x00};
	int i = 0;
	int err = 0;

	struct mipi_dsi_device *dsi;

	dsi = &panel->mipi_device;
	if (!dsi) {
		pr_info("AOD: No device");
		return -ENOMEM;
	}
	if (!panel->panel_initialized) {
		pr_info("AOD: Panel not initialized\n");
		return -ENOMEM;
	}
	mutex_lock(&panel->panel_lock);
	if (level == 0) {
		for (i = 0; i < sizeof(reg_addr); i++) {
			err = mipi_dsi_dcs_write(dsi, reg_addr[i], &data_level_l[i], 1);
			if (err < 0)
				goto error;
		}
	} else if (level == 1) {
		for (i = 0; i < sizeof(reg_addr); i++) {
			err = mipi_dsi_dcs_write(dsi, reg_addr[i], &data_level_m[i], 1);
			if (err < 0)
				goto error;
		}

	} else {
		for (i = 0; i < sizeof(reg_addr); i++) {
			err = mipi_dsi_dcs_write(dsi, reg_addr[i], &data_level_h[i], 1);
			if (err < 0)
				goto error;
		}
	}
	mutex_unlock(&panel->panel_lock);

	pr_info("AOD: set brightness successful, new level  = %d\n",
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_brightness);

	return err;

error:
	mutex_unlock(&panel->panel_lock);
	pr_err("AOD: send cmds failed");
	return err;

}

static ssize_t zte_lcd_proc_aod_bl_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *f_pos)
{
	char *tmp = kzalloc((count+1), GFP_KERNEL);
	u32 level;

	if (!tmp)
		return -ENOMEM;

	if (copy_from_user(tmp, buffer, count)) {
		kfree(tmp);
		return -EFAULT;
	}

	level = *tmp - '0';
	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_brightness = level;
	pr_info("AOD set new bl level: %d\n", level);

	if (g_zte_ctrl_pdata->zte_panel_state == 1) /* set brightness here if already in aod mode*/
		panel_set_aod_brightness(g_zte_ctrl_pdata, level);
	else
		pr_info("AOD save new bl level, not in aod mode now.\n");

	kfree(tmp);
	return count;
}
static int zte_lcd_proc_aod_bl_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_brightness);
	pr_info("AOD brightness:  = %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_brightness);
	return 0;
}
static int zte_lcd_proc_aod_bl_open(struct inode *inode, struct file *file)
{
	return single_open(file, zte_lcd_proc_aod_bl_show, NULL);
}
static const struct file_operations zte_lcd_aod_bl_proc_fops = {
	.owner		= THIS_MODULE,
	.open		= zte_lcd_proc_aod_bl_open,
	.read		= seq_read,
	.write		= zte_lcd_proc_aod_bl_write,
	.llseek		= seq_lseek,
	.release		= single_release,
};
static int zte_lcd_set_aod_brightness(struct device_node *node)
{
	proc_create("driver/lcd_aod_bl", 0664, NULL, &zte_lcd_aod_bl_proc_fops);

	/* 0 - low level, 1 - middle level(default), 2 - high level*/
	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_brightness = 1;

	pr_info("AOD brightness level:  = %d\n", g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_aod_brightness);

	return 0;
}
#endif
/********************AOD BRIGHTNESS end***********************/

/********************lcd backlight level curve begin*****************/
#ifdef CONFIG_ZTE_LCD_BACKLIGHT_LEVEL_CURVE
enum {	/* lcd curve mode */
	CURVE_MATRIX_MAX_350_LUX = 1,
	CURVE_MATRIX_MAX_400_LUX,
	CURVE_MATRIX_MAX_450_LUX,
};

int zte_backlight_curve_matrix_max_350_lux[256] = {
0, 1, 2, 3, 3, 3, 4, 5, 6, 6, 7, 7, 8, 8, 9, 9,
10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 16, 16, 17, 17, 18,
18, 19, 19, 20, 21, 21, 22, 22, 23, 23, 24, 25, 25, 26, 26, 27,
28, 28, 29, 29, 30, 31, 31, 32, 33, 33, 34, 35, 35, 36, 36, 37,
38, 38, 39, 40, 40, 41, 42, 43, 43, 44, 45, 45, 46, 47, 47, 48,
49, 50, 50, 51, 52, 52, 53, 54, 55, 55, 56, 57, 58, 58, 59, 60,
61, 61, 62, 63, 64, 64, 65, 66, 67, 68, 68, 69, 70, 71, 72, 73,
74, 75, 76, 77, 77, 78, 79, 80, 81, 82, 82, 83, 84, 85, 86, 87,
88, 88, 89, 90, 91, 92, 93, 94, 95, 96, 96, 97, 98, 99, 100, 101,
102, 103, 104, 105, 106, 107, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116,
117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132,
133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 146, 147,
148, 149, 151, 152, 153, 155, 156, 157, 159, 160, 162, 163, 164, 166, 167, 169,
170, 172, 173, 175, 176, 178, 179, 181, 183, 184, 186, 187, 189, 191, 192, 194,
196, 197, 199, 201, 203, 204, 206, 208, 210, 212, 214, 215, 217, 219, 221, 223,
225, 227, 229, 231, 233, 235, 237, 239, 241, 243, 245, 248, 250, 252, 254, 255
};

int zte_backlight_curve_matrix_max_400_lux[256] = {
0, 1, 2, 3, 3, 3, 4, 4, 5, 5, 6, 6, 6, 7, 7, 8,
8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16,
16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 23, 24,
25, 25, 26, 26, 27, 27, 28, 28, 29, 30, 30, 31, 31, 32, 32, 33,
34, 34, 35, 35, 36, 37, 37, 38, 38, 39, 40, 40, 41, 42, 42, 43,
43, 44, 45, 45, 46, 47, 47, 48, 49, 49, 50, 51, 51, 52, 53, 53,
54, 55, 55, 56, 57, 57, 58, 59, 59, 60, 61, 61, 62, 63, 64, 64,
65, 66, 66, 67, 68, 69, 69, 70, 71, 72, 72, 73, 74, 74, 75, 76,
77, 78, 78, 79, 80, 81, 81, 82, 83, 84, 84, 85, 86, 87, 88, 88,
89, 90, 91, 92, 92, 93, 94, 95, 96, 96, 97, 98, 99, 100, 101, 101,
102, 103, 104, 105, 106, 107, 107, 108, 109, 110, 111, 112, 113, 113, 114, 115,
116, 117, 118, 119, 120, 121, 121, 122, 123, 124, 125, 126, 127, 128, 129, 129,
130, 132, 133, 134, 136, 137, 139, 140, 142, 143, 145, 147, 148, 150, 151, 153,
155, 156, 158, 160, 162, 163, 165, 167, 169, 170, 172, 174, 176, 178, 180, 182,
184, 186, 188, 190, 192, 194, 196, 198, 200, 203, 205, 207, 209, 212, 214, 216,
219, 221, 223, 226, 228, 231, 233, 236, 238, 241, 243, 246, 249, 252, 254, 255
};

int zte_backlight_curve_matrix_max_450_lux[256] = {
0, 1, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 6, 7, 7,
8, 8, 8, 9, 9, 10, 10, 10, 11, 11, 12, 12, 13, 13, 13, 14,
14, 15, 15, 16, 16, 17, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21,
22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27, 27, 28, 28, 29, 29,
30, 30, 31, 31, 32, 32, 33, 33, 34, 34, 35, 36, 36, 37, 37, 38,
38, 39, 39, 40, 41, 41, 42, 42, 43, 43, 44, 45, 45, 46, 46, 47,
48, 48, 49, 49, 50, 51, 51, 52, 52, 53, 54, 54, 55, 56, 56, 57,
57, 58, 59, 59, 60, 61, 61, 62, 63, 63, 64, 65, 65, 66, 67, 67,
68, 69, 69, 70, 71, 71, 72, 73, 73, 74, 75, 75, 76, 77, 78, 78,
79, 80, 80, 81, 82, 83, 83, 84, 85, 85, 86, 87, 88, 88, 89, 90,
91, 91, 92, 93, 94, 94, 95, 96, 97, 97, 98, 99, 100, 101, 101, 102,
103, 104, 105, 105, 106, 107, 108, 109, 109, 110, 111, 112, 112, 112, 113, 113,
114, 116, 117, 119, 120, 122, 123, 125, 126, 128, 130, 131, 133, 135, 136, 138,
140, 142, 143, 145, 147, 149, 151, 153, 155, 157, 159, 161, 163, 165, 167, 169,
171, 173, 176, 178, 180, 183, 185, 187, 190, 192, 194, 197, 199, 202, 205, 207,
210, 213, 215, 218, 221, 224, 226, 229, 232, 235, 238, 241, 244, 248, 251, 255
};
static int zte_convert_backlevel_function(int level, u32 bl_max)
{
	int bl, convert_level;

	if (level == 0)
		return 0;

	if ((bl_max != 4095) && (bl_max != 1023) && (bl_max != 255)) {
		bl = level * 255 / bl_max;
	} else if (bl_max > 1023) {
		bl = level>>4;
	} else if (bl_max > 255) {
		bl = level >> 2;
	} else {
		bl = level;
	}

	if (!bl && level)
		bl = 1;/*ensure greater than 0 and less than 16 equal to 1*/

	switch (g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_bl_curve_mode) {
	case CURVE_MATRIX_MAX_350_LUX:
		convert_level = zte_backlight_curve_matrix_max_350_lux[bl];
		break;
	case CURVE_MATRIX_MAX_400_LUX:
		convert_level = zte_backlight_curve_matrix_max_400_lux[bl];
		break;
	case CURVE_MATRIX_MAX_450_LUX:
		convert_level = zte_backlight_curve_matrix_max_450_lux[bl];
		break;
	default:
		convert_level = zte_backlight_curve_matrix_max_450_lux[bl];
		break;
	}

	if ((bl_max != 4095) && (bl_max != 1023) && (bl_max != 255)) {
		convert_level = convert_level * bl_max / 255;
	} else if (bl_max > 1023) {
		convert_level = (convert_level >= 255) ? 4095 : (convert_level<<4);
	} else if (bl_max > 255) {
		convert_level = (convert_level >= 255) ? 1023 : (convert_level<<2);
	}

	return convert_level;
}
#endif
/********************lcd backlight level curve end*****************/

/********************lcd gpio power ctrl begin***********************/
#ifdef CONFIG_ZTE_LCD_GPIO_CTRL_POWER
static int zte_gpio_ctrl_lcd_power_enable(int enable)
{
	pr_info("[MSM_LCD] %s:%s\n", __func__, enable ? "enable":"disable");
	if (enable) {
		usleep_range(5000, 5100);
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio > 0) {
			gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio, 1);
			usleep_range(5000, 5100);
		}
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio > 0) {
			gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio, 1);
			usleep_range(5000, 5100);
		}
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio > 0) {
			gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio, 1);
			usleep_range(5000, 5100);
		}
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio > 0) {
			gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio, 1);
			usleep_range(5000, 5100);
		}
	} else {
		usleep_range(1000, 1100);
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio > 0) {
			gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio, 0);
			usleep_range(5000, 5100);
		}
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio > 0) {
			gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio, 0);
			usleep_range(5000, 5100);
		}
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio > 0) {
			gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio, 0);
			usleep_range(5000, 5100);
		}
		if (g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio > 0) {
			gpio_set_value(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio, 0);
			usleep_range(5000, 5100);
		}
	}
	return 0;
}
static int zte_gpio_ctrl_lcd_power_gpio_dt(struct device_node *node)
{
	g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio = of_get_named_gpio(node,
		"zte,disp_avdd_en_gpio", 0);
	if (!gpio_is_valid(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio)) {
		pr_info("%s:%d, zte,disp_avdd_en_gpio not specified\n", __func__, __LINE__);
	} else {
		if (gpio_request(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio, "disp_avdd_en_gpio")) {
			pr_info("request disp_avdd_en_gpio failed\n");
		} else {
			gpio_direction_output(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_avdd_en_gpio, 1);
			pr_info("%s:request disp_avdd_en_gpio success\n", __func__);
		}
	}
	g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio = of_get_named_gpio(node,
		"zte,disp_iovdd_en_gpio", 0);
	if (!gpio_is_valid(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio)) {
		pr_info("%s:%d, zte,disp_iovdd_en_gpio not specified\n", __func__, __LINE__);
	} else {
		if (gpio_request(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio, "disp_iovdd_en_gpio")) {
			pr_info("request disp_iovdd_en_gpio failed %d\n",
				g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio);
		} else {
			gpio_direction_output(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_iovdd_en_gpio, 1);
			pr_info("%s:request disp_iovdd_en_gpio success\n", __func__);
		}
	}

	g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio = of_get_named_gpio(node,
		"zte,disp_vsp_en_gpio", 0);
	if (!gpio_is_valid(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio)) {
		pr_info("%s:%d, zte,disp_vsp_en_gpio not specified\n", __func__, __LINE__);
	} else {
		if (gpio_request(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio, "disp_vsp_en_gpio")) {
			pr_info("request disp_vsp_en_gpio failed\n");
		} else {
			gpio_direction_output(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsp_en_gpio, 1);
			pr_info("%s:request disp_vsp_en_gpio success\n", __func__);
		}
	}
	g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio = of_get_named_gpio(node,
		"zte,disp_vsn_en_gpio", 0);
	if (!gpio_is_valid(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio)) {
		pr_info("%s:%d, zte,disp_vsn_en_gpio not specified\n", __func__, __LINE__);
	} else {
		if (gpio_request(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio, "disp_vsn_en_gpio")) {
			pr_info("request disp_vsn_en_gpio failed %d\n",
				g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio);
		} else {
			gpio_direction_output(g_zte_ctrl_pdata->zte_lcd_ctrl->disp_vsn_en_gpio, 1);
			pr_info("%s:request disp_vsn_en_gpio success\n", __func__);
		}
	}

	return 0;
}
#endif
/********************lcd gpio power ctrl end***********************/

/********************lcd common function start*****************/
static void zte_lcd_panel_parse_dt(struct device_node *node)
{
#ifdef CONFIG_ZTE_LCD_BACKLIGHT_LEVEL_CURVE
	const char *data;
#endif

	g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_reset_high_sleeping = of_property_read_bool(node,
										"zte,lcm_reset_pin_keep_high_sleeping");

#ifdef CONFIG_ZTE_LCD_GPIO_CTRL_POWER
	zte_gpio_ctrl_lcd_power_gpio_dt(node);
#endif

#ifdef CONFIG_ZTE_LCD_BACKLIGHT_LEVEL_CURVE
	data = of_get_property(node, "zte,lcm_backlight_curve_mode", NULL);
	if (data) {
		if (!strcmp(data, "lcd_brightness_max_350_lux"))
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_bl_curve_mode = CURVE_MATRIX_MAX_350_LUX;
		else if (!strcmp(data, "lcd_brightness_max_400_lux"))
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_bl_curve_mode = CURVE_MATRIX_MAX_400_LUX;
		else if (!strcmp(data, "lcd_brightness_max_450_lux"))
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_bl_curve_mode = CURVE_MATRIX_MAX_450_LUX;
		else
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_bl_curve_mode = CURVE_MATRIX_MAX_450_LUX;
	} else
		g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_bl_curve_mode = CURVE_MATRIX_MAX_450_LUX;

	pr_info("[MSM_LCD]%s:dtsi_mode=%s matrix_mode=%d\n", __func__, data,
			g_zte_ctrl_pdata->zte_lcd_ctrl->lcd_bl_curve_mode);
#endif


}
void zte_lcd_common_func(struct dsi_panel *panel, struct device_node *node)
{
	g_zte_ctrl_pdata = panel;
	/*kzalloc zte_lcd_ctrl,must use always in whole life,don't need to free zte_lcd_ctrl*/
	g_zte_ctrl_pdata->zte_lcd_ctrl = kzalloc(sizeof(struct zte_lcd_ctrl_data), GFP_KERNEL);

	if (!g_zte_ctrl_pdata->zte_lcd_ctrl) {
		pr_err("%s:kzalloc memory failed\n", __func__);
		return;
	}

	zte_lcd_panel_parse_dt(node);
	zte_lcd_proc_info_display(node);
#ifdef CONFIG_ZTE_LCD_HBM_CTRL
	zte_lcd_hbm_ctrl(node);
	zte_lcd_set_hdr_flag(node);
#endif
#ifdef CONFIG_ZTE_LCD_AOD_BRIGHTNESS_CTRL
	zte_lcd_set_aod_brightness(node);
#endif
#ifdef CONFIG_ZTE_LCD_GPIO_CTRL_POWER
	g_zte_ctrl_pdata->zte_lcd_ctrl->gpio_enable_lcd_power = zte_gpio_ctrl_lcd_power_enable;
#endif
#ifdef CONFIG_ZTE_LCD_BACKLIGHT_LEVEL_CURVE
	g_zte_ctrl_pdata->zte_lcd_ctrl->zte_convert_brightness = zte_convert_backlevel_function;
#endif
#ifdef CONFIG_ZTE_LCD_REG_DEBUG
	zte_lcd_reg_debug_func();
#endif
}
/********************lcd common function end*****************/


