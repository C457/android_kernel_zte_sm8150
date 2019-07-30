#include <linux/module.h>
#include <linux/crc32.h>
#include <media/cam_sensor.h>
#include "zte_cam_eeprom_dev.h"
#include "zte_cam_eeprom_core.h"
#include "cam_debug_util.h"

#define S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_SUNNY		0x01
#define S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_TRULY		0x02
#define S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_A_KERR		0x03
#define S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_LITEARRAY	0x04
#define S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_DARLING	0x05
#define S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_QTECH		0x06
#define S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_OFLIM		0x07
#define S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_FOXCONN	0x11
#define S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_IMPORTEK	0x12
#define S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_ALTEK		0x13
#define S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_ABICO		0x14
#define S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_LITE_ON	0x15
#define S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_CHICONY	0x16
#define S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_PRIMAX		0x17
#define S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_SHARP		0x21
#define S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_LITE_ON_N	0x22
#define S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_MCNEX		0x31
#define S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_MCNEX_LSC	0xA0

MODULE_Map_Table S5K3T1SP_AUX_MODULE_MAP[] = {
	{ S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_SUNNY,
		"sunny_s5k3t1sp_aux", "sunny_s5k3t1sp_aux", NULL},
	{ S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_TRULY,
		"truly_s5k3t1sp_aux", "truly_s5k3t1sp_aux", NULL},
	{ S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_A_KERR,
		"a_kerr_s5k3t1sp_aux", "a_kerr_s5k3t1sp_aux", NULL},
	{ S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_LITEARRAY,
		"litearray_s5k3t1sp_aux", "litearray_s5k3t1sp_aux", NULL},
	{ S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_DARLING,
		"darling_s5k3t1sp_aux", "darling_s5k3t1sp_aux", NULL},
	{ S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_QTECH,
		"qtech_s5k3t1sp_aux", "qtech_s5k3t1sp_aux", NULL},
	{ S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_OFLIM,
		"oflim_s5k3t1sp_aux", "oflim_s5k3t1sp_aux", NULL},
	{ S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_FOXCONN,
		"foxconn_s5k3t1sp_aux", "foxconn_s5k3t1sp_aux", NULL},
	{ S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_IMPORTEK,
		"importek_s5k3t1sp_aux", "importek_s5k3t1sp_aux", NULL},
	{ S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_ALTEK,
		"altek_s5k3t1sp_aux", "altek_s5k3t1sp_aux", NULL},
	{ S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_ABICO,
		"abico_s5k3t1sp_aux", "abico_s5k3t1sp_aux", NULL},
	{ S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_LITE_ON,
		"lite_on_s5k3t1sp_aux", "lite_on_s5k3t1sp_aux", NULL},
	{ S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_CHICONY,
		"chicony_s5k3t1sp_aux", "chicony_s5k3t1sp_aux", NULL},
	{ S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_PRIMAX,
		"primax_s5k3t1sp_aux", "primax_s5k3t1sp_aux", NULL},
	{ S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_SHARP,
		"sharp_s5k3t1sp_aux", "sharp_s5k3t1sp_aux", NULL},
	{ S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_LITE_ON_N,
		"lite_on_new_s5k3t1sp_aux", "lite_on_new_s5k3t1sp_aux", NULL},
	{ S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_MCNEX,
		"mcnex_s5k3t1sp_aux", "mcnex_s5k3t1sp_aux", NULL},
	{ S5K3T1SP_AUX_SENSOR_INFO_MODULE_ID_MCNEX_LSC,
		"mcnex_lsc_s5k3t1sp_aux", "mcnex_lsc_s5k3t1sp_aux", NULL},
};

#define DEFAULT_EEPROM_SETTING_FILE_NAME "s5k3t1sp_aux_default_eeprom_setting.fw"

#define S5K3T1SP_AUX_ID_ADDR 0x1

#define FLAG_MODULE_INFO_ADDR 0x0
#define FLAG_AWB_ADDR 0x1A
#define FLAG_LSC_ADDR 0x28
#define FLAG_REMOSAIC_ADDR 0x0712

#define FLAG_VALID_VALUE 0x01


#define CS_MODULE_INFO_S_ADDR 0x01
#define CS_MODULE_INFO_E_ADDR 0x07
#define CS_MODULE_INFO_ADDR 0x09

#define CS_AWB_S_ADDR 0x1B
#define CS_AWB_E_ADDR 0x26
#define CS_AWB_ADDR (CS_AWB_E_ADDR+1)

#define CS_LSC_S_ADDR 0x29
#define CS_LSC_E_ADDR 0x710
#define CS_LSC_ADDR (CS_LSC_E_ADDR+1)

#define CS_REMOSAIC_S_ADDR 0x0713
#define CS_REMOSAIC_E_ADDR 0x1312
#define CS_REMOSAIC_ADDR (CS_REMOSAIC_E_ADDR+1)

void s5k3t1sp_aux_parse_module_name(struct cam_eeprom_ctrl_t *e_ctrl)
{
	uint16_t sensor_module_id = e_ctrl->cal_data.mapdata[S5K3T1SP_AUX_ID_ADDR];

	parse_module_name(&(e_ctrl->module_info[0]), S5K3T1SP_AUX_MODULE_MAP,
		sizeof(S5K3T1SP_AUX_MODULE_MAP) / sizeof(MODULE_Map_Table),
		sensor_module_id);
}

void s5k3t1sp_aux_set_cali_info_valid(struct cam_eeprom_ctrl_t *e_ctrl)
{
	uint8_t *peepromdata = &(e_ctrl->cal_data.mapdata[0]);
	uint32_t num = e_ctrl->cal_data.num_data;

	pr_info("%s :%d:num = %d valid_flag=0x%x  checksum=0x%x\n",
		__func__, __LINE__, num, e_ctrl->valid_flag, e_ctrl->checksum);

	if ((e_ctrl->checksum & EEPROM_REMOSAIC_INFO_CHECKSUM_INVALID) ||
		(e_ctrl->valid_flag & EEPROM_REMOSAIC_INFO_INVALID)) {
		pr_info("%s :%d: REMOSAIC info invalid\n", __func__, __LINE__);
		peepromdata[num - 1] = 1;
	} else {
		peepromdata[num - 1] = 0;
	}

	if ((e_ctrl->checksum & EEPROM_LSC_INFO_CHECKSUM_INVALID) ||
		(e_ctrl->valid_flag & EEPROM_LSC_INFO_INVALID)) {
		pr_info("%s :%d: LSC info invalid\n", __func__, __LINE__);
		peepromdata[num - 2] = 1;
	} else {
		peepromdata[num - 2] = 0;
	}

	if ((e_ctrl->checksum & EEPROM_AWB_INFO_CHECKSUM_INVALID) ||
		(e_ctrl->valid_flag & EEPROM_AWB_INFO_INVALID)) {
		peepromdata[num - 3] = 1;
		pr_info("%s :%d: AWB info invalid\n", __func__, __LINE__);
	} else {
		peepromdata[num - 3] = 0;
	}
}


int s5k3t1sp_aux_validflag_check_eeprom(struct cam_eeprom_ctrl_t *e_ctrl)
{
	int  flag = 0;

	if (e_ctrl->cal_data.mapdata[FLAG_MODULE_INFO_ADDR] != FLAG_VALID_VALUE) {
		pr_err("%s :%d: module info flag invalid 0x%x\n",
			__func__, __LINE__, e_ctrl->cal_data.mapdata[FLAG_MODULE_INFO_ADDR]);
		flag |= EEPROM_MODULE_INFO_INVALID;
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

	if (e_ctrl->cal_data.mapdata[FLAG_REMOSAIC_ADDR] != FLAG_VALID_VALUE) {
		pr_err("%s :%d: REMOSIC flag invalid 0x%x\n",
			__func__, __LINE__, e_ctrl->cal_data.mapdata[FLAG_REMOSAIC_ADDR]);
		flag |= EEPROM_REMOSAIC_INFO_INVALID;
	}
	pr_info("%s :%d: valid info flag = 0x%x  %s\n",
		__func__, __LINE__, flag, (flag == 0) ? "true" : "false");

	return flag;

}

int s5k3t1sp_aux_checksum_eeprom(struct cam_eeprom_ctrl_t *e_ctrl)
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
	for (j = CS_REMOSAIC_S_ADDR; j <= CS_REMOSAIC_E_ADDR; j++)
		checksum += e_ctrl->cal_data.mapdata[j];

	if ((checksum % 256) != e_ctrl->cal_data.mapdata[CS_REMOSAIC_ADDR]) {
		pr_err("%s :%d: REMOSAIC info checksum fail\n", __func__, __LINE__);
		rc  |= EEPROM_REMOSAIC_INFO_CHECKSUM_INVALID;
	}

	pr_info("%s :%d: cal info checksum rc = 0x%x %s\n",
			__func__, __LINE__, rc, (rc == 0) ? "true" : "false");

	return rc;
}

uint8_t *get_s5k3t1sp_aux_default_setting_filename(struct cam_eeprom_ctrl_t *e_ctrl)
{
	return DEFAULT_EEPROM_SETTING_FILE_NAME;
}

static struct zte_eeprom_fn_t s5k3t1sp_aux_eeprom_func_tbl = {
	.read_eeprom_memory = NULL,
	.eeprom_match_crc = NULL,
	.eeprom_checksum = s5k3t1sp_aux_checksum_eeprom,
	.validflag_check_eeprom = s5k3t1sp_aux_validflag_check_eeprom,
	.parse_module_name = s5k3t1sp_aux_parse_module_name,
	.set_cali_info_valid = s5k3t1sp_aux_set_cali_info_valid,
	.read_id = NULL,
	.get_default_eeprom_setting_filename = get_s5k3t1sp_aux_default_setting_filename,
};

static const struct of_device_id s5k3t1sp_aux_eeprom_dt_match[] = {
	{ .compatible = "zte,s5k3t1sp_aux-eeprom", .data = &s5k3t1sp_aux_eeprom_func_tbl},
	{ }
};
MODULE_DEVICE_TABLE(of, s5k3t1sp_aux_eeprom_dt_match);

static int s5k3t1sp_aux_eeprom_platform_probe(struct platform_device *pdev)
{
	int32_t rc = 0;
	const struct of_device_id *match;

	pr_info("%s:%d %s E", __func__, __LINE__, pdev->name);
	match = of_match_device(s5k3t1sp_aux_eeprom_dt_match, &pdev->dev);
	if (match)
		rc = cam_eeprom_platform_driver_probe(pdev, match);
	else {
		pr_err("%s:%d match is null\n", __func__, __LINE__);
		rc = -EINVAL;
	}
	pr_info("%s:%d rc=%d X", __func__, __LINE__, rc);
	return rc;
}

static int s5k3t1sp_aux_eeprom_platform_remove(struct platform_device *pdev)
{
	const struct of_device_id *match;
	int32_t rc = 0;

	match = of_match_device(s5k3t1sp_aux_eeprom_dt_match, &pdev->dev);
	if (match)
		rc = cam_eeprom_platform_driver_remove(pdev);
	else {
		pr_err("%s:%d match is null\n", __func__, __LINE__);
		rc = -EINVAL;
	}

	return rc;
}

static struct platform_driver s5k3t1sp_aux_eeprom_platform_driver = {
	.driver = {
		.name = "zte,s5k3t1sp_aux-eeprom",
		.owner = THIS_MODULE,
		.of_match_table = s5k3t1sp_aux_eeprom_dt_match,
	},
	.probe = s5k3t1sp_aux_eeprom_platform_probe,
	.remove = s5k3t1sp_aux_eeprom_platform_remove,
};

static int __init s5k3t1sp_aux_eeprom_init_module(void)
{
	int rc = 0;

	rc = platform_driver_register(&s5k3t1sp_aux_eeprom_platform_driver);
	pr_info("%s:%d platform rc %d\n", __func__, __LINE__, rc);
	return rc;
}

static void __exit s5k3t1sp_aux_eeprom_exit_module(void)
{
	platform_driver_unregister(&s5k3t1sp_aux_eeprom_platform_driver);

}

module_init(s5k3t1sp_aux_eeprom_init_module);
module_exit(s5k3t1sp_aux_eeprom_exit_module);
MODULE_DESCRIPTION("ZTE EEPROM driver");
MODULE_LICENSE("GPL v2");

