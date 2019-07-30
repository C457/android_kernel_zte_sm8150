/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _ZTE_CAM_EEPROM_CORE_H_
#define _ZTE_CAM_EEPROM_CORE_H_

#include "zte_cam_eeprom_dev.h"

#define EEPROM_MODULE_INFO_INVALID        0x1
#define EEPROM_AF_INFO_INVALID                 0x2
#define EEPROM_AWB_INFO_INVALID              0x4
#define EEPROM_LSC_INFO_INVALID               0x8
#define EEPROM_SPC_INFO_INVALID               0x10
#define EEPROM_DCC_INFO_INVALID               0x20
#define EEPROM_OIS_INFO_INVALID               0x40
#define EEPROM_DUAL_CAM_INFO_INVALID    0x80
#define EEPROM_REMOSAIC_INFO_INVALID    0x100

#define EEPROM_MODULE_INFO_CHECKSUM_INVALID                0x1
#define EEPROM_AF_INFO_CHECKSUM_INVALID                         0x2
#define EEPROM_AWB_INFO_CHECKSUM_INVALID                      0x4
#define EEPROM_LSC_INFO_CHECKSUM_INVALID                       0x8
#define EEPROM_SPC_INFO_CHECKSUM_INVALID                       0x10
#define EEPROM_DCC_INFO_CHECKSUM_INVALID                       0x20
#define EEPROM_OIS_INFO_CHECKSUM_INVALID                        0x40
#define EEPROM_DUAL_CAM_INFO_CHECKSUM_INVALID            0x80
#define EEPROM_REMOSAIC_INFO_CHECKSUM_INVALID            0x100

int32_t zte_cam_eeprom_driver_cmd(struct cam_eeprom_ctrl_t *e_ctrl, void *arg);
int32_t zte_cam_eeprom_parse_read_memory_map(struct device_node *of_node,
	struct cam_eeprom_ctrl_t *e_ctrl);
/**
 * @e_ctrl: EEPROM ctrl structure
 *
 * This API handles the shutdown ioctl/close
 */
void zte_cam_eeprom_shutdown(struct cam_eeprom_ctrl_t *e_ctrl);
void parse_module_name(zte_eeprom_module_info_t *module_info,
	MODULE_Map_Table *map, uint16_t len, uint16_t  sensor_module_id);

#endif
/* _ZTE_CAM_EEPROM_CORE_H_ */
