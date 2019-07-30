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
#ifndef _ZTE_CAM_EEPROM_DEV_H_
#define _ZTE_CAM_EEPROM_DEV_H_

#include <linux/i2c.h>
#include <linux/gpio.h>
#include <media/v4l2-event.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-ioctl.h>
#include <media/cam_sensor.h>
#include <cam_sensor_i2c.h>
#include <cam_sensor_spi.h>
#include <cam_sensor_io.h>
#include <cam_cci_dev.h>
#include <cam_req_mgr_util.h>
#include <cam_req_mgr_interface.h>
#include <cam_mem_mgr.h>
#include <cam_subdev.h>
#include "cam_soc_util.h"

#define DEFINE_MSM_MUTEX(mutexname) \
	static struct mutex mutexname = __MUTEX_INITIALIZER(mutexname)

#define PROPERTY_MAXSIZE 32

#define MSM_EEPROM_MEMORY_MAP_MAX_SIZE         80
#define MSM_EEPROM_MAX_MEM_MAP_CNT             8
#define MSM_EEPROM_MEM_MAP_PROPERTIES_CNT      8

struct cam_eeprom_ctrl_t;

enum cam_eeprom_state {
	CAM_EEPROM_INIT,
	CAM_EEPROM_ACQUIRE,
	CAM_EEPROM_CONFIG,
};

typedef struct {
	uint16_t id;
	const char *sensor_module_name;
	const char *chromtix_lib_name;
	const char *default_chromtix_lib_name;
} MODULE_Map_Table;

/**
 * struct cam_eeprom_map_t - eeprom map
 * @data_type       :   Data type
 * @addr_type       :   Address type
 * @addr            :   Address
 * @data            :   data
 * @delay           :   Delay
 *
 */
struct cam_eeprom_map_t {
	uint32_t valid_size;
	uint32_t addr;
	uint32_t addr_type;
	uint32_t data;
	uint32_t data_type;
	uint32_t delay;
};

/**
 * struct cam_eeprom_memory_map_t - eeprom memory map types
 * @page            :   page memory
 * @pageen          :   pageen memory
 * @poll            :   poll memory
 * @mem             :   mem
 * @saddr           :   slave addr
 *
 */
struct cam_eeprom_memory_map_t {
	struct cam_eeprom_map_t page;
	struct cam_eeprom_map_t pageen;
	struct cam_eeprom_map_t poll;
	struct cam_eeprom_map_t mem;
	uint32_t saddr;
};

/**
 * struct cam_eeprom_memory_block_t - eeprom mem block info
 * @map             :   eeprom memory map
 * @num_map         :   number of map blocks
 * @mapdata         :   map data
 * @cmd_type        :   size of total mapdata
 *
 */
struct cam_eeprom_memory_block_t {
	struct cam_eeprom_memory_map_t *map;
	uint32_t num_map;
	uint8_t *mapdata;
	uint32_t num_data;
};

/**
 * struct cam_eeprom_cmm_t - camera multimodule
 * @cmm_support     :   cmm support flag
 * @cmm_compression :   cmm compression flag
 * @cmm_offset      :   cmm data start offset
 * @cmm_size        :   cmm data size
 *
 */
struct cam_eeprom_cmm_t {
	uint32_t cmm_support;
	uint32_t cmm_compression;
	uint32_t cmm_offset;
	uint32_t cmm_size;
};

/**
 * struct cam_eeprom_i2c_info_t - I2C info
 * @slave_addr      :   slave address
 * @i2c_freq_mode   :   i2c frequency mode
 *
 */
struct cam_eeprom_i2c_info_t {
	uint16_t slave_addr;
	uint8_t i2c_freq_mode;
};

/**
 * struct cam_eeprom_soc_private - eeprom soc private data structure
 * @eeprom_name     :   eeprom name
 * @i2c_info        :   i2c info structure
 * @power_info      :   eeprom power info
 * @cmm_data        :   cmm data
 *
 */
struct cam_eeprom_soc_private {
	const char *eeprom_name;
	struct cam_eeprom_i2c_info_t i2c_info;
	struct cam_sensor_power_ctrl_t power_info;
	struct cam_eeprom_cmm_t cmm_data;
};

/**
 * struct cam_eeprom_intf_params - bridge interface params
 * @device_hdl   : Device Handle
 * @session_hdl  : Session Handle
 * @ops          : KMD operations
 * @crm_cb       : Callback API pointers
 */
struct cam_eeprom_intf_params {
	int32_t device_hdl;
	int32_t session_hdl;
	int32_t link_hdl;
	struct cam_req_mgr_kmd_ops ops;
	struct cam_req_mgr_crm_cb *crm_cb;
};

struct zte_eeprom_fn_t {
	int (*read_eeprom_memory)(struct cam_eeprom_ctrl_t *e_ctrl, struct cam_eeprom_memory_block_t *block);
	uint32_t (*eeprom_match_crc)(struct cam_eeprom_memory_block_t *data);
	int (*eeprom_checksum)(struct cam_eeprom_ctrl_t *e_ctrl);
	int (*validflag_check_eeprom)(struct cam_eeprom_ctrl_t *e_ctrl);
	void (*parse_module_name)(struct cam_eeprom_ctrl_t *e_ctrl);
	void (*set_cali_info_valid)(struct cam_eeprom_ctrl_t *e_ctrl);
	uint32_t (*read_id)(struct cam_sensor_cci_client *cci_client);
	uint8_t *(*get_default_eeprom_setting_filename)(struct cam_eeprom_ctrl_t *e_ctrl);
};

typedef struct {
	const char *sensor_module_name;
} zte_eeprom_module_info_t;

/**
 * struct cam_cmd_conditional_wait - Conditional wait command
 * @pdev            :   platform device
 * @spi             :   spi device
 * @eeprom_mutex    :   eeprom mutex
 * @soc_info        :   eeprom soc related info
 * @io_master_info  :   Information about the communication master
 * @gpio_num_info   :   gpio info
 * @cci_i2c_master  :   I2C structure
 * @v4l2_dev_str    :   V4L2 device structure
 * @bridge_intf     :   bridge interface params
 * @cam_eeprom_state:   eeprom_device_state
 * @userspace_probe :   flag indicates userspace or kernel probe
 * @cal_data        :   Calibration data
 * @device_name     :   Device name
 *
 */
struct cam_eeprom_ctrl_t {
	struct platform_device *pdev;
	struct spi_device *spi;
	struct mutex eeprom_mutex;
	struct cam_hw_soc_info soc_info;
	struct camera_io_master io_master_info;
	struct msm_camera_gpio_num_info *gpio_num_info;
	enum cci_i2c_master_t cci_i2c_master;
	enum cci_device_num cci_num;
	struct cam_subdev v4l2_dev_str;
	struct cam_eeprom_intf_params bridge_intf;
	enum msm_camera_device_type_t eeprom_device_type;
	enum cam_eeprom_state cam_eeprom_state;
	bool userspace_probe;
	struct cam_eeprom_memory_block_t cal_data;
	char device_name[20];

	struct zte_eeprom_fn_t *eeprom_fun_p;
	zte_eeprom_module_info_t module_info[2];
	uint32_t checksum;
	uint32_t valid_flag;
	bool probed;
};

int32_t zte_cam_eeprom_update_i2c_info(struct cam_eeprom_ctrl_t *e_ctrl,
	struct cam_eeprom_i2c_info_t *i2c_info);

int32_t cam_eeprom_platform_driver_probe(
	struct platform_device *pdev, const struct of_device_id *match);

int cam_eeprom_platform_driver_remove(struct platform_device *pdev);

#endif /*_ZTE_CAM_EEPROM_DEV_H_ */
