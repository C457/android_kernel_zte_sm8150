#include <linux/module.h>
#include <linux/crc32.h>
#include <media/cam_sensor.h>
#include "zte_cam_eeprom_dev.h"
#include "zte_cam_eeprom_core.h"
#include "cam_debug_util.h"

#define IMX363_SENSOR_INFO_MODULE_ID_SUNNY		0x01
#define IMX363_SENSOR_INFO_MODULE_ID_TRULY		0x02
#define IMX363_SENSOR_INFO_MODULE_ID_A_KERR		0x03
#define IMX363_SENSOR_INFO_MODULE_ID_LITEARRAY	0x04
#define IMX363_SENSOR_INFO_MODULE_ID_DARLING	0x05
#define IMX363_SENSOR_INFO_MODULE_ID_QTECH		0x06
#define IMX363_SENSOR_INFO_MODULE_ID_OFLIM		0x07
#define IMX363_SENSOR_INFO_MODULE_ID_FOXCONN	0x11
#define IMX363_SENSOR_INFO_MODULE_ID_IMPORTEK	0x12
#define IMX363_SENSOR_INFO_MODULE_ID_ALTEK		0x13
#define IMX363_SENSOR_INFO_MODULE_ID_ABICO		0x14
#define IMX363_SENSOR_INFO_MODULE_ID_LITE_ON	0x15
#define IMX363_SENSOR_INFO_MODULE_ID_CHICONY	0x16
#define IMX363_SENSOR_INFO_MODULE_ID_PRIMAX		0x17
#define IMX363_SENSOR_INFO_MODULE_ID_SHARP		0x21
#define IMX363_SENSOR_INFO_MODULE_ID_LITE_ON_N	0x22
#define IMX363_SENSOR_INFO_MODULE_ID_MCNEX		0x31
#define IMX363_SENSOR_INFO_MODULE_ID_MCNEX_LSC	0xA0
#define IMX363_SENSOR_INFO_MODULE_ID_NEWSUNNY	0x60

MODULE_Map_Table IMX363_MODULE_MAP[] = {
	{ IMX363_SENSOR_INFO_MODULE_ID_SUNNY,
		"sunny_imx363", "sunny_imx363", NULL},
	{ IMX363_SENSOR_INFO_MODULE_ID_TRULY,
		"truly_imx363", "truly_imx363", NULL},
	{ IMX363_SENSOR_INFO_MODULE_ID_A_KERR,
		"a_kerr_imx363", "a_kerr_imx363", NULL},
	{ IMX363_SENSOR_INFO_MODULE_ID_LITEARRAY,
		"litearray_imx363", "litearray_imx363", NULL},
	{ IMX363_SENSOR_INFO_MODULE_ID_DARLING,
		"darling_imx363", "darling_imx363", NULL},
	{ IMX363_SENSOR_INFO_MODULE_ID_QTECH,
		"qtech_imx363", "qtech_imx363", NULL},
	{ IMX363_SENSOR_INFO_MODULE_ID_OFLIM,
		"oflim_imx363", "oflim_imx363", NULL},
	{ IMX363_SENSOR_INFO_MODULE_ID_FOXCONN,
		"foxconn_imx363", "foxconn_imx363", NULL},
	{ IMX363_SENSOR_INFO_MODULE_ID_IMPORTEK,
		"importek_imx363", "importek_imx363", NULL},
	{ IMX363_SENSOR_INFO_MODULE_ID_ALTEK,
		"altek_imx363", "altek_imx363", NULL},
	{ IMX363_SENSOR_INFO_MODULE_ID_ABICO,
		"abico_imx363", "abico_imx363", NULL},
	{ IMX363_SENSOR_INFO_MODULE_ID_LITE_ON,
		"lite_on_imx363", "lite_on_imx363", NULL},
	{ IMX363_SENSOR_INFO_MODULE_ID_CHICONY,
		"chicony_imx363", "chicony_imx363", NULL},
	{ IMX363_SENSOR_INFO_MODULE_ID_PRIMAX,
		"primax_imx363", "primax_imx363", NULL},
	{ IMX363_SENSOR_INFO_MODULE_ID_SHARP,
		"sharp_imx363", "sharp_imx363", NULL},
	{ IMX363_SENSOR_INFO_MODULE_ID_LITE_ON_N,
		"lite_on_new_imx363", "lite_on_new_imx363", NULL},
	{ IMX363_SENSOR_INFO_MODULE_ID_MCNEX,
		"mcnex_imx363", "mcnex_imx363", NULL},
	{ IMX363_SENSOR_INFO_MODULE_ID_MCNEX_LSC,
		"mcnex_lsc_imx363", "mcnex_lsc_imx363", NULL},
	{ IMX363_SENSOR_INFO_MODULE_ID_NEWSUNNY,
		"sunny_imx363", "sunny_imx363", NULL},

};


#define IMX363_ID_ADDR 0x1

#define IMX363_VCM_ID_ADDR 0x3

#define FLAG_MODULE_INFO_ADDR 0x0
#define FLAG_AF_ADDR 0xC
#define FLAG_AWB_ADDR 0x1A
#define FLAG_LSC_ADDR 0x28
#define FLAG_SPC_ADDR 0x712
#define FLAG_DCC_ADDR 0xD36
#define FLAG_OIS_ADDR 0xD9E
#define FLAG_DUAL_CAEMRA_ADDR 0x100C

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

#define CS_OIS_S_ADDR 0xD9F
#define CS_OIS_E_ADDR 0xDC4
#define CS_OIS_ADDR 0xDD5

#define CS_DUAL_CAMERA_S_ADDR 0x100D
#define CS_DUAL_CAMERA_E_ADDR 0x180C
#define CS_DUAL_CAMERA_ADDR (CS_DUAL_CAMERA_E_ADDR+1)

#define CS_AEC_CALIBRATION_AUX_EXPOSURE_H_ADDR 0x1009
#define CS_AEC_CALIBRATION_AUX_EXPOSURE_L_ADDR 0x100A

void imx363_parse_module_name(struct cam_eeprom_ctrl_t *e_ctrl)
{
	uint16_t sensor_module_id = e_ctrl->cal_data.mapdata[IMX363_ID_ADDR];

	parse_module_name(&(e_ctrl->module_info[0]), IMX363_MODULE_MAP,
		sizeof(IMX363_MODULE_MAP) / sizeof(MODULE_Map_Table),
		sensor_module_id);

	/*The bad sunny module is compatibility here by zte*/
	if (sensor_module_id == IMX363_SENSOR_INFO_MODULE_ID_SUNNY) {
		pr_info("%s :%d: Bad Sunny module\n", __func__, __LINE__);
		e_ctrl->cal_data.mapdata[CS_AEC_CALIBRATION_AUX_EXPOSURE_H_ADDR] = 0x0b;
		e_ctrl->cal_data.mapdata[CS_AEC_CALIBRATION_AUX_EXPOSURE_L_ADDR] = 0x8a;
	}

}

void imx363_set_cali_info_valid(struct cam_eeprom_ctrl_t *e_ctrl)
{
	uint8_t *peepromdata = &(e_ctrl->cal_data.mapdata[0]);
	uint32_t num = e_ctrl->cal_data.num_data;

	pr_info("%s :%d:num = %d valid_flag=0x%x  checksum=0x%x\n",
		__func__, __LINE__, num, e_ctrl->valid_flag, e_ctrl->checksum);
	if ((e_ctrl->checksum & EEPROM_DUAL_CAM_INFO_CHECKSUM_INVALID) ||
		(e_ctrl->valid_flag & EEPROM_DUAL_CAM_INFO_INVALID)) {
		pr_info("%s :%d: DUAL info invalid\n", __func__, __LINE__);
		peepromdata[num - 1] = 1;
	} else {
		peepromdata[num - 1] = 0;
	}

	if ((e_ctrl->checksum & EEPROM_OIS_INFO_CHECKSUM_INVALID) ||
		(e_ctrl->valid_flag & EEPROM_OIS_INFO_INVALID)) {
		pr_info("%s :%d: OIS info invalid\n", __func__, __LINE__);
		peepromdata[num - 2] = 1;
	} else {
		peepromdata[num - 2] = 0;
	}

	if ((e_ctrl->checksum & EEPROM_SPC_INFO_CHECKSUM_INVALID) ||
		(e_ctrl->valid_flag & EEPROM_SPC_INFO_INVALID) ||
		(e_ctrl->checksum & EEPROM_DCC_INFO_CHECKSUM_INVALID) ||
		(e_ctrl->valid_flag & EEPROM_DCC_INFO_INVALID)) {
		pr_info("%s :%d: PDAF info invalid\n", __func__, __LINE__);
		peepromdata[num - 3] = 1;
	} else {
		peepromdata[num - 3] = 0;
	}

	if ((e_ctrl->checksum & EEPROM_LSC_INFO_CHECKSUM_INVALID) ||
		(e_ctrl->valid_flag & EEPROM_LSC_INFO_INVALID)) {
		peepromdata[num - 4] = 1;
		pr_info("%s :%d: LSC info invalid\n", __func__, __LINE__);
	} else {
		peepromdata[num - 4] = 0;
	}

	if ((e_ctrl->checksum & EEPROM_AWB_INFO_CHECKSUM_INVALID) ||
		(e_ctrl->valid_flag & EEPROM_AWB_INFO_INVALID)) {
		pr_info("%s :%d: AWB info invalid\n", __func__, __LINE__);
		peepromdata[num - 5] = 1;
	} else {
		peepromdata[num - 5] = 0;
	}

	if ((e_ctrl->checksum & EEPROM_AF_INFO_CHECKSUM_INVALID) ||
		(e_ctrl->valid_flag & EEPROM_AF_INFO_INVALID)) {
		pr_info("%s :%d: AF info invalid\n", __func__, __LINE__);
		peepromdata[num - 6] = 1;
	} else {
		peepromdata[num - 6] = 0;
	}
}

int imx363_validflag_check_eeprom(struct cam_eeprom_ctrl_t *e_ctrl)
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

	if (e_ctrl->cal_data.mapdata[FLAG_OIS_ADDR] != FLAG_VALID_VALUE) {
		pr_err("%s :%d: OIS flag invalid 0x%x\n",
			__func__, __LINE__, e_ctrl->cal_data.mapdata[FLAG_OIS_ADDR]);
		flag |= EEPROM_OIS_INFO_INVALID;
	}

	if (e_ctrl->cal_data.mapdata[FLAG_DUAL_CAEMRA_ADDR] != FLAG_VALID_VALUE) {
		pr_err("%s :%d: dual camera flag invalid 0x%x\n",
			__func__, __LINE__, e_ctrl->cal_data.mapdata[FLAG_DUAL_CAEMRA_ADDR]);
		flag |= EEPROM_DUAL_CAM_INFO_INVALID;
	}
	pr_info("%s :%d: valid info flag = 0x%x  %s\n",
		__func__, __LINE__, flag, (flag == 0) ? "true" : "false");

	return flag;

}

int imx363_checksum_eeprom(struct cam_eeprom_ctrl_t *e_ctrl)
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
	checksum = 0;

	for (j = CS_OIS_S_ADDR; j <= CS_OIS_E_ADDR; j++)
		checksum += e_ctrl->cal_data.mapdata[j];

	if ((checksum % 256) != e_ctrl->cal_data.mapdata[CS_OIS_ADDR]) {
		pr_err("%s :%d: OIS checksum fail\n", __func__, __LINE__);
		rc  |= EEPROM_OIS_INFO_CHECKSUM_INVALID;
	}

	checksum = 0;
	for (j = CS_DUAL_CAMERA_S_ADDR; j <= CS_DUAL_CAMERA_E_ADDR; j++)
		checksum += e_ctrl->cal_data.mapdata[j];

	if ((checksum % 256) != e_ctrl->cal_data.mapdata[CS_DUAL_CAMERA_ADDR]) {
		pr_err("%s :%d: DUAL CAMERA checksum fail\n", __func__, __LINE__);
		rc  |= EEPROM_DUAL_CAM_INFO_CHECKSUM_INVALID;
	}

	pr_info("%s :%d: cal info checksum rc = 0x%x %s\n",
			__func__, __LINE__, rc, (rc == 0) ? "true" : "false");
	return rc;
}


uint32_t imx363_read_vcm_id(struct cam_sensor_cci_client *cci_client)
{
	int rc = 0;
	struct cam_sensor_cci_client local_cci_client;
	uint32_t reg_addr = IMX363_VCM_ID_ADDR;
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

static struct zte_eeprom_fn_t imx363_eeprom_func_tbl = {
	.read_eeprom_memory = NULL,
	.eeprom_match_crc = NULL,
	.eeprom_checksum = imx363_checksum_eeprom,
	.validflag_check_eeprom = imx363_validflag_check_eeprom,
	.parse_module_name = imx363_parse_module_name,
	.set_cali_info_valid = imx363_set_cali_info_valid,
	.read_id = imx363_read_vcm_id,
};

static const struct of_device_id imx363_eeprom_dt_match[] = {
	{ .compatible = "zte,imx363-eeprom", .data = &imx363_eeprom_func_tbl},
	{ }
};
MODULE_DEVICE_TABLE(of, imx363_eeprom_dt_match);

static int imx363_eeprom_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;

	pr_info("%s:%d %s E", __func__, __LINE__, pdev->name);
	match = of_match_device(imx363_eeprom_dt_match, &pdev->dev);
	if (match)
		rc = cam_eeprom_platform_driver_probe(pdev, match);
	else {
		pr_err("%s:%d match is null\n", __func__, __LINE__);
		rc = -EINVAL;
	}
	pr_info("%s:%d rc=%d X", __func__, __LINE__, rc);
	return rc;
}

static int imx363_eeprom_platform_remove(struct platform_device *pdev)
{
	const struct of_device_id *match;
	int32_t rc = 0;

	pr_info("%s:%d E", __func__, __LINE__);
	match = of_match_device(imx363_eeprom_dt_match, &pdev->dev);
	if (match)
		rc = cam_eeprom_platform_driver_remove(pdev);
	else {
		pr_err("%s:%d match is null\n", __func__, __LINE__);
		rc = -EINVAL;
	}
	pr_info("%s:%d X", __func__, __LINE__);
	return rc;
}

static struct platform_driver imx363_eeprom_platform_driver = {
	.driver = {
		.name = "zte,imx363-eeprom",
		.owner = THIS_MODULE,
		.of_match_table = imx363_eeprom_dt_match,
	},
	.probe = imx363_eeprom_platform_probe,
	.remove = imx363_eeprom_platform_remove,
};

static int __init imx363_eeprom_init_module(void)
{
	int rc = 0;

	rc = platform_driver_register(&imx363_eeprom_platform_driver);
	pr_info("%s:%d platform rc %d\n", __func__, __LINE__, rc);
	return rc;
}

static void __exit imx363_eeprom_exit_module(void)
{
	platform_driver_unregister(&imx363_eeprom_platform_driver);

}

module_init(imx363_eeprom_init_module);
module_exit(imx363_eeprom_exit_module);
MODULE_DESCRIPTION("ZTE EEPROM driver");
MODULE_LICENSE("GPL v2");
