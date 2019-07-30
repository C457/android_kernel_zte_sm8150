#include <linux/module.h>
#include <linux/crc32.h>
#include <media/cam_sensor.h>
#include "zte_cam_eeprom_dev.h"
#include "zte_cam_eeprom_core.h"
#include "cam_debug_util.h"

#define OV08A10_SENSOR_INFO_MODULE_ID_SUNNY		0x01
#define OV08A10_SENSOR_INFO_MODULE_ID_TRULY		0x02
#define OV08A10_SENSOR_INFO_MODULE_ID_A_KERR		0x03
#define OV08A10_SENSOR_INFO_MODULE_ID_LITEARRAY	0x04
#define OV08A10_SENSOR_INFO_MODULE_ID_DARLING	0x05
#define OV08A10_SENSOR_INFO_MODULE_ID_QTECH		0x06
#define OV08A10_SENSOR_INFO_MODULE_ID_OFLIM		0x07
#define OV08A10_SENSOR_INFO_MODULE_ID_FOXCONN	0x11
#define OV08A10_SENSOR_INFO_MODULE_ID_IMPORTEK	0x12
#define OV08A10_SENSOR_INFO_MODULE_ID_ALTEK		0x13
#define OV08A10_SENSOR_INFO_MODULE_ID_ABICO		0x14
#define OV08A10_SENSOR_INFO_MODULE_ID_LITE_ON	0x15
#define OV08A10_SENSOR_INFO_MODULE_ID_CHICONY	0x16
#define OV08A10_SENSOR_INFO_MODULE_ID_PRIMAX		0x17
#define OV08A10_SENSOR_INFO_MODULE_ID_SHARP		0x21
#define OV08A10_SENSOR_INFO_MODULE_ID_LITE_ON_N	0x22
#define OV08A10_SENSOR_INFO_MODULE_ID_MCNEX		0x31
#define OV08A10_SENSOR_INFO_MODULE_ID_MCNEX_LSC	0xA0
#define OV08A10_SENSOR_INFO_MODULE_ID_NEWSUNNY	0x60

MODULE_Map_Table OV08A10_MODULE_MAP[] = {
	{ OV08A10_SENSOR_INFO_MODULE_ID_SUNNY,
		"sunny_ov08a10", "sunny_ov08a10", NULL},
	{ OV08A10_SENSOR_INFO_MODULE_ID_TRULY,
		"truly_ov08a10", "truly_ov08a10", NULL},
	{ OV08A10_SENSOR_INFO_MODULE_ID_A_KERR,
		"a_kerr_ov08a10", "a_kerr_ov08a10", NULL},
	{ OV08A10_SENSOR_INFO_MODULE_ID_LITEARRAY,
		"litearray_ov08a10", "litearray_ov08a10", NULL},
	{ OV08A10_SENSOR_INFO_MODULE_ID_DARLING,
		"darling_ov08a10", "darling_ov08a10", NULL},
	{ OV08A10_SENSOR_INFO_MODULE_ID_QTECH,
		"qtech_ov08a10", "qtech_ov08a10", NULL},
	{ OV08A10_SENSOR_INFO_MODULE_ID_OFLIM,
		"oflim_ov08a10", "oflim_ov08a10", NULL},
	{ OV08A10_SENSOR_INFO_MODULE_ID_FOXCONN,
		"foxconn_ov08a10", "foxconn_ov08a10", NULL},
	{ OV08A10_SENSOR_INFO_MODULE_ID_IMPORTEK,
		"importek_ov08a10", "importek_ov08a10", NULL},
	{ OV08A10_SENSOR_INFO_MODULE_ID_ALTEK,
		"altek_ov08a10", "altek_ov08a10", NULL},
	{ OV08A10_SENSOR_INFO_MODULE_ID_ABICO,
		"abico_ov08a10", "abico_ov08a10", NULL},
	{ OV08A10_SENSOR_INFO_MODULE_ID_LITE_ON,
		"lite_on_ov08a10", "lite_on_ov08a10", NULL},
	{ OV08A10_SENSOR_INFO_MODULE_ID_CHICONY,
		"chicony_ov08a10", "chicony_ov08a10", NULL},
	{ OV08A10_SENSOR_INFO_MODULE_ID_PRIMAX,
		"primax_ov08a10", "primax_ov08a10", NULL},
	{ OV08A10_SENSOR_INFO_MODULE_ID_SHARP,
		"sharp_ov08a10", "sharp_ov08a10", NULL},
	{ OV08A10_SENSOR_INFO_MODULE_ID_LITE_ON_N,
		"lite_on_new_ov08a10", "lite_on_new_ov08a10", NULL},
	{ OV08A10_SENSOR_INFO_MODULE_ID_MCNEX,
		"mcnex_ov08a10", "mcnex_ov08a10", NULL},
	{ OV08A10_SENSOR_INFO_MODULE_ID_MCNEX_LSC,
		"mcnex_lsc_ov08a10", "mcnex_lsc_ov08a10", NULL},
	{ OV08A10_SENSOR_INFO_MODULE_ID_NEWSUNNY,
		"sunny_ov08a10", "sunny_ov08a10", NULL},

};


#define OV08A10_ID_ADDR 0x1

#define OV08A10_VCM_ID_ADDR 0x3

#define FLAG_MODULE_INFO_ADDR 0x0
#define FLAG_AF_ADDR 0xC
#define FLAG_AWB_ADDR 0x1A
#define FLAG_LSC_ADDR 0x28
#define FLAG_SPC_ADDR 0x712
#define FLAG_DCC_ADDR 0xD36

#define FLAG_VALID_VALUE 0x01

#define CS_MODULE_INFO_S_ADDR 0x01
#define CS_MODULE_INFO_E_ADDR 0x07
#define CS_MODULE_INFO_ADDR 0x09

#define CS_AF_S_ADDR 0x0D
#define CS_AF_E_ADDR 0x12
#define CS_AF_ADDR (CS_AF_E_ADDR+1)

#define CS_AWB_S_ADDR 0x1B
#define CS_AWB_E_ADDR 0x26
#define CS_AWB_ADDR (CS_AWB_E_ADDR+1)

#define CS_LSC_S_ADDR 0x29
#define CS_LSC_E_ADDR 0x710
#define CS_LSC_ADDR (CS_LSC_E_ADDR+1)

#define CS_SPC_S_ADDR 0x713
#define CS_SPC_E_ADDR 0xA8C
#define CS_SPC_ADDR (CS_SPC_E_ADDR+1)

#define CS_DCC_S_ADDR 0xD37
#define CS_DCC_E_ADDR 0xD9C
#define CS_DCC_ADDR (CS_DCC_E_ADDR+1)

#define DEFAULT_EEPROM_SETTING_FILE_NAME "ov08a10_default_eeprom_setting.fw"

#define DUAL_AEC_S_ADDR 0xDA0
#define DUAL_AEC_LEN 10

void ov08a10_parse_module_name(struct cam_eeprom_ctrl_t *e_ctrl)
{
	uint16_t sensor_module_id = e_ctrl->cal_data.mapdata[OV08A10_ID_ADDR];

	parse_module_name(&(e_ctrl->module_info[0]), OV08A10_MODULE_MAP,
		sizeof(OV08A10_MODULE_MAP) / sizeof(MODULE_Map_Table),
		sensor_module_id);
}

void ov08a10_write_fix_info_eeprom(struct cam_eeprom_ctrl_t *e_ctrl)
{
	e_ctrl->cal_data.mapdata[DUAL_AEC_S_ADDR] = 0x03;
	e_ctrl->cal_data.mapdata[DUAL_AEC_S_ADDR + 1] = 0xF2;

	e_ctrl->cal_data.mapdata[DUAL_AEC_S_ADDR + 2] = 0x04;
	e_ctrl->cal_data.mapdata[DUAL_AEC_S_ADDR + 3] = 0x00;

	e_ctrl->cal_data.mapdata[DUAL_AEC_S_ADDR + 4] = 0x13;
	e_ctrl->cal_data.mapdata[DUAL_AEC_S_ADDR + 5] = 0xF0;

	e_ctrl->cal_data.mapdata[DUAL_AEC_S_ADDR + 6] = 0x04;
	e_ctrl->cal_data.mapdata[DUAL_AEC_S_ADDR + 7] = 0x00;

	e_ctrl->cal_data.mapdata[DUAL_AEC_S_ADDR + 8] = 0x48;
	e_ctrl->cal_data.mapdata[DUAL_AEC_S_ADDR + 9] = 0xDA;
}

void ov08a10_set_cali_info_valid(struct cam_eeprom_ctrl_t *e_ctrl)
{
	uint8_t *peepromdata = &(e_ctrl->cal_data.mapdata[0]);
	uint32_t num = e_ctrl->cal_data.num_data;

	pr_info("%s :%d:num = %d valid_flag=0x%x  checksum=0x%x\n",
		__func__, __LINE__, num, e_ctrl->valid_flag, e_ctrl->checksum);


	if ((e_ctrl->checksum & EEPROM_SPC_INFO_CHECKSUM_INVALID) ||
		(e_ctrl->valid_flag & EEPROM_SPC_INFO_INVALID) ||
		(e_ctrl->checksum & EEPROM_DCC_INFO_CHECKSUM_INVALID) ||
		(e_ctrl->valid_flag & EEPROM_DCC_INFO_INVALID)) {
		pr_info("%s :%d: PDAF info invalid\n", __func__, __LINE__);
		peepromdata[num - 1] = 1;
	} else {
		peepromdata[num - 1] = 0;
	}

	if ((e_ctrl->checksum & EEPROM_LSC_INFO_CHECKSUM_INVALID) ||
		(e_ctrl->valid_flag & EEPROM_LSC_INFO_INVALID)) {
		peepromdata[num - 2] = 1;
		pr_info("%s :%d: LSC info invalid\n", __func__, __LINE__);
	} else {
		peepromdata[num - 2] = 0;
	}

	if ((e_ctrl->checksum & EEPROM_AWB_INFO_CHECKSUM_INVALID) ||
		(e_ctrl->valid_flag & EEPROM_AWB_INFO_INVALID)) {
		pr_info("%s :%d: AWB info invalid\n", __func__, __LINE__);
		peepromdata[num - 3] = 1;
	} else {
		peepromdata[num - 3] = 0;
	}

	if ((e_ctrl->checksum & EEPROM_AF_INFO_CHECKSUM_INVALID) ||
		(e_ctrl->valid_flag & EEPROM_AF_INFO_INVALID)) {
		pr_info("%s :%d: AF info invalid\n", __func__, __LINE__);
		peepromdata[num - 4] = 1;
	} else {
		peepromdata[num - 4] = 0;
	}
}

int ov08a10_validflag_check_eeprom(struct cam_eeprom_ctrl_t *e_ctrl)
{
	int  flag = 0;

	if (e_ctrl->cal_data.mapdata[FLAG_MODULE_INFO_ADDR] != FLAG_VALID_VALUE) {
		pr_err("%s :%d: module info flag invalid 0x%x\n",
			__func__, __LINE__, e_ctrl->cal_data.mapdata[FLAG_MODULE_INFO_ADDR]);
		flag |= EEPROM_MODULE_INFO_INVALID;
	}

	if (e_ctrl->cal_data.mapdata[FLAG_AF_ADDR] != FLAG_VALID_VALUE) {
		pr_err("%s :%d: AF flag invalid 0x%x\n",
			__func__, __LINE__, e_ctrl->cal_data.mapdata[FLAG_AF_ADDR]);
		flag |= EEPROM_AF_INFO_INVALID;
	}

	if (e_ctrl->cal_data.mapdata[FLAG_AWB_ADDR] != FLAG_VALID_VALUE) {
		pr_err("%s :%d: AWB flag invalid 0x%x\n",
			__func__, __LINE__, e_ctrl->cal_data.mapdata[FLAG_AWB_ADDR]);
		flag |= EEPROM_AWB_INFO_INVALID;
	}

	if (e_ctrl->cal_data.mapdata[FLAG_LSC_ADDR] != FLAG_VALID_VALUE) {
		pr_err("%s :%d: LSC flag invalid 0x%x\n",
			__func__, __LINE__, e_ctrl->cal_data.mapdata[FLAG_LSC_ADDR]);
		flag |= EEPROM_LSC_INFO_INVALID;
	}

	if (e_ctrl->cal_data.mapdata[FLAG_SPC_ADDR] != FLAG_VALID_VALUE) {
		pr_err("%s :%d: SPC flag invalid 0x%x\n",
			__func__, __LINE__, e_ctrl->cal_data.mapdata[FLAG_SPC_ADDR]);
		flag |= EEPROM_SPC_INFO_INVALID;
	}

	if (e_ctrl->cal_data.mapdata[FLAG_DCC_ADDR] != FLAG_VALID_VALUE) {
		pr_err("%s :%d: DCC flag invalid 0x%x\n",
			__func__, __LINE__, e_ctrl->cal_data.mapdata[FLAG_DCC_ADDR]);
		flag |= EEPROM_DCC_INFO_INVALID;
	}

	pr_info("%s :%d: valid info flag = 0x%x  %s\n",
		__func__, __LINE__, flag, (flag == 0) ? "true" : "false");

	return flag;

}

int ov08a10_checksum_eeprom(struct cam_eeprom_ctrl_t *e_ctrl)
{
	int  checksum = 0;
	int j;
	int rc = 0;

	pr_info("%s :%d: E", __func__, __LINE__);

	for (j = CS_MODULE_INFO_S_ADDR; j <= CS_MODULE_INFO_E_ADDR; j++)
		checksum += e_ctrl->cal_data.mapdata[j];

	if ((checksum % 256) != e_ctrl->cal_data.mapdata[CS_MODULE_INFO_ADDR]) {
		pr_err("%s :%d: module info checksum fail\n", __func__, __LINE__);
		rc  |= EEPROM_MODULE_INFO_CHECKSUM_INVALID;
	}
	checksum = 0;

	for (j = CS_AF_S_ADDR; j <= CS_AF_E_ADDR; j++)
		checksum += e_ctrl->cal_data.mapdata[j];

	if ((checksum % 256) != e_ctrl->cal_data.mapdata[CS_AF_ADDR]) {
		pr_err("%s :%d: af info  checksum fail\n", __func__, __LINE__);
		rc  |= EEPROM_AF_INFO_CHECKSUM_INVALID;
	}
	checksum = 0;

	for (j = CS_AWB_S_ADDR; j <= CS_AWB_E_ADDR; j++)
		checksum += e_ctrl->cal_data.mapdata[j];

	if ((checksum % 256) != e_ctrl->cal_data.mapdata[CS_AWB_ADDR]) {
		pr_err("%s :%d: awb info checksum fail\n", __func__, __LINE__);
		rc  |= EEPROM_AWB_INFO_CHECKSUM_INVALID;
	}
	checksum = 0;

	for (j = CS_LSC_S_ADDR; j <= CS_LSC_E_ADDR; j++)
		checksum += e_ctrl->cal_data.mapdata[j];

	if ((checksum % 256) != e_ctrl->cal_data.mapdata[CS_LSC_ADDR]) {
		pr_err("%s :%d: lsc info checksum fail\n", __func__, __LINE__);
		rc  |= EEPROM_LSC_INFO_CHECKSUM_INVALID;
	}
	checksum = 0;

	for (j = CS_SPC_S_ADDR; j <= CS_SPC_E_ADDR; j++)
		checksum += e_ctrl->cal_data.mapdata[j];

	if ((checksum % 256) != e_ctrl->cal_data.mapdata[CS_SPC_ADDR]) {
		pr_err("%s :%d: SPC info checksum fail\n", __func__, __LINE__);
		rc  |= EEPROM_SPC_INFO_CHECKSUM_INVALID;
	}
	checksum = 0;

	for (j = CS_DCC_S_ADDR; j <= CS_DCC_E_ADDR; j++)
		checksum += e_ctrl->cal_data.mapdata[j];

	if ((checksum % 256) != e_ctrl->cal_data.mapdata[CS_DCC_ADDR]) {
		pr_err("%s :%d: DCC info checksum fail\n", __func__, __LINE__);
		rc  |= EEPROM_DCC_INFO_CHECKSUM_INVALID;
	}

	if (e_ctrl->cal_data.num_data >= (DUAL_AEC_S_ADDR + DUAL_AEC_LEN))
		ov08a10_write_fix_info_eeprom(e_ctrl);

	pr_info("%s :%d: cal info checksum rc = 0x%x %s\n",
			__func__, __LINE__, rc, (rc == 0) ? "true" : "false");
	return rc;
}

uint8_t *get_ov08a10_default_setting_filename(struct cam_eeprom_ctrl_t *e_ctrl)
{
	return DEFAULT_EEPROM_SETTING_FILE_NAME;
}

#if 0
uint32_t ov08a10_read_vcm_id(struct cam_sensor_cci_client *cci_client)
{
	int rc = 0;
	struct cam_sensor_cci_client local_cci_client;
	uint32_t reg_addr = OV08A10_VCM_ID_ADDR;
	uint32_t data = 0;
	uint32_t vcm_id = 0;
	enum camera_sensor_i2c_type addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	enum camera_sensor_i2c_type data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;

	memcpy(&local_cci_client, cci_client, sizeof(struct cam_sensor_cci_client));
	local_cci_client.i2c_freq_mode = I2C_STANDARD_MODE;
	local_cci_client.sid = 0xa0 >> 1;


	rc = cam_cci_i2c_read(&local_cci_client, reg_addr, &data, addr_type, data_type);
	if (rc < 0) {
		pr_err("%s :%d: read module id fail\n", __func__, __LINE__);
	}

	switch (data) {
	case 1:
		vcm_id = 1;
		break;
	case 2:
		vcm_id = 2;
		break;
	case 3:
		vcm_id = 3;
		break;
	default:
		vcm_id = 1;
		break;
	}

	return vcm_id;
}
#endif

static struct zte_eeprom_fn_t ov08a10_eeprom_func_tbl = {
	.read_eeprom_memory = NULL,
	.eeprom_match_crc = NULL,
	.eeprom_checksum = ov08a10_checksum_eeprom,
	.validflag_check_eeprom = ov08a10_validflag_check_eeprom,
	.parse_module_name = ov08a10_parse_module_name,
	.set_cali_info_valid = ov08a10_set_cali_info_valid,
	.read_id = NULL,
	.get_default_eeprom_setting_filename = get_ov08a10_default_setting_filename,
};

static const struct of_device_id ov08a10_eeprom_dt_match[] = {
	{ .compatible = "zte,ov08a10-eeprom", .data = &ov08a10_eeprom_func_tbl},
	{ }
};
MODULE_DEVICE_TABLE(of, ov08a10_eeprom_dt_match);

static int ov08a10_eeprom_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;

	pr_info("%s:%d %s E", __func__, __LINE__, pdev->name);
	match = of_match_device(ov08a10_eeprom_dt_match, &pdev->dev);
	if (match)
		rc = cam_eeprom_platform_driver_probe(pdev, match);
	else {
		pr_err("%s:%d match is null\n", __func__, __LINE__);
		rc = -EINVAL;
	}
	pr_info("%s:%d rc=%d X", __func__, __LINE__, rc);
	return rc;
}

static int ov08a10_eeprom_platform_remove(struct platform_device *pdev)
{
	const struct of_device_id *match;
	int32_t rc = 0;

	pr_info("%s:%d E", __func__, __LINE__);
	match = of_match_device(ov08a10_eeprom_dt_match, &pdev->dev);
	if (match)
		rc = cam_eeprom_platform_driver_remove(pdev);
	else {
		pr_err("%s:%d match is null\n", __func__, __LINE__);
		rc = -EINVAL;
	}
	pr_info("%s:%d X", __func__, __LINE__);
	return rc;
}

static struct platform_driver ov08a10_eeprom_platform_driver = {
	.driver = {
		.name = "zte,ov08a10-eeprom",
		.owner = THIS_MODULE,
		.of_match_table = ov08a10_eeprom_dt_match,
	},
	.probe = ov08a10_eeprom_platform_probe,
	.remove = ov08a10_eeprom_platform_remove,
};

static int __init ov08a10_eeprom_init_module(void)
{
	int rc = 0;

	rc = platform_driver_register(&ov08a10_eeprom_platform_driver);
	pr_info("%s:%d platform rc %d\n", __func__, __LINE__, rc);
	return rc;
}

static void __exit ov08a10_eeprom_exit_module(void)
{
	platform_driver_unregister(&ov08a10_eeprom_platform_driver);

}

module_init(ov08a10_eeprom_init_module);
module_exit(ov08a10_eeprom_exit_module);
MODULE_DESCRIPTION("ZTE EEPROM driver");
MODULE_LICENSE("GPL v2");
