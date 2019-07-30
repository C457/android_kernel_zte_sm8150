/************************************************************************
*
* File Name: synaptics_common_interface.c
*
*  *   Version: v1.0
*
************************************************************************/
#include "synaptics_tcm_core.h"
#include "synaptics_tcm_testing.h"
#include "synaptics_common_interface.h"
#include "../tpd_sys.h"

#define SYNATCM_DEFAULT_OUT_PATH	"/sdcard/"
#define SYNATCM_DEFAULT_OUT_FILE	"syna_test_result.txt"

extern int syna_tcm_get_app_info(struct syna_tcm_hcd *tcm_hcd);
extern int syna_tcm_node_notifier_cb(bool enable);
extern struct testing_hcd *testing_hcd;

char synatcm_save_file_path[128] = {0};
char synatcm_save_file_name[128] = {0};

bool tpd_is_bsc_complete = true;

struct synatcm_tpd *synatcm_tpd_test;

static struct chip_id_l {
	int id;
	char *model;
} synap_chip_id_l[] = {
	{0x64, "S3908"},
	{0xff, " 0000"},
};

enum test_status {
	TP_TEST_INIT = 1,
	TP_TEST_START,
	TP_TEST_END
};

static struct chip_id_l *tpd_get_chip_id(int id)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(synap_chip_id_l); i++) {
		if (id == synap_chip_id_l[i].id)
			return &synap_chip_id_l[i];
	}
	return &synap_chip_id_l[i - 1];
}

static int tpd_init_tpinfo(struct tpd_classdev_t *cdev)
{
	int retval;
	int firmware;
	struct syna_tcm_hcd *tcm_hcd = (struct syna_tcm_hcd *)cdev->private;
	struct i2c_client *i2c = to_i2c_client(tcm_hcd->pdev->dev.parent);
	struct chip_id_l *chip_id;

	mutex_lock(&tcm_hcd->extif_mutex);

	pr_notice("%s: enter!\n", __func__);
	retval = syna_tcm_get_app_info(tcm_hcd);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get app info\n");
		goto exit;
	}

	firmware = (unsigned int)tcm_hcd->app_info.customer_config_id[3] +
				(unsigned int)tcm_hcd->app_info.customer_config_id[2] * 0x100;

	chip_id = tpd_get_chip_id((int)tcm_hcd->app_info.customer_config_id[0]);
	snprintf(cdev->ic_tpinfo.tp_name, sizeof(cdev->ic_tpinfo.tp_name), "Synaptics_%s",
			chip_id->model);
	cdev->ic_tpinfo.chip_model_id = 1;
	cdev->ic_tpinfo.module_id = (unsigned int)(tcm_hcd->app_info.customer_config_id[1]);
	cdev->ic_tpinfo.firmware_ver = firmware;
	cdev->ic_tpinfo.i2c_addr = i2c->addr;

	pr_notice("%s: end!\n", __func__);
exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

#ifdef WAKEUP_GESTURE
static int tpd_get_singletapgesture(struct tpd_classdev_t *cdev)
{
	struct syna_tcm_hcd *tcm_hcd = (struct syna_tcm_hcd *)cdev->private;

	cdev->b_single_tap_enable = tcm_hcd->is_single_tap;

	return 0;
}

static int tpd_set_singletapgesture(struct tpd_classdev_t *cdev, int enable)
{
	struct syna_tcm_hcd *tcm_hcd = (struct syna_tcm_hcd *)cdev->private;

	pr_notice("%s: tcm_hcd->is_single_tap = %d\n", __func__, enable);
	tcm_hcd->is_single_tap = enable;

	return 0;
}

static int tpd_get_wakegesture(struct tpd_classdev_t *cdev)
{
	struct syna_tcm_hcd *tcm_hcd = (struct syna_tcm_hcd *)cdev->private;

	cdev->b_gesture_enable = tcm_hcd->in_wakeup_gesture;

	return 0;
}

static int tpd_enable_wakegesture(struct tpd_classdev_t *cdev, int enable)
{
	struct syna_tcm_hcd *tcm_hcd = (struct syna_tcm_hcd *)cdev->private;

	tcm_hcd->in_wakeup_gesture = enable;

	return 0;
}
#endif

static int tpd_get_smart_cover(struct tpd_classdev_t *cdev)
{
	unsigned short value;
	int retval;
	struct syna_tcm_hcd *tcm_hcd = (struct syna_tcm_hcd *)cdev->private;

	mutex_lock(&tcm_hcd->extif_mutex);

	pr_notice("%s: start\n", __func__);
	retval = tcm_hcd->get_dynamic_config(tcm_hcd, DC_ENABLE_CLOSED_COVER, &value);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get dynamic config\n");
		value = -1;
	}

	cdev->b_smart_cover_enable = value;
	pr_notice("%s: end!\n", __func__);

	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static int tpd_set_smart_cover(struct tpd_classdev_t *cdev, int enable)
{
	int retval = 0;
	struct syna_tcm_hcd *tcm_hcd = (struct syna_tcm_hcd *)cdev->private;

	mutex_lock(&tcm_hcd->extif_mutex);

	pr_notice("%s: start\n", __func__);
	tcm_hcd->is_smart_cover = enable;
	if (tcm_hcd->in_suspend) {
		pr_notice("%s: in suspend!\n", __func__);
	} else {
		retval = tcm_hcd->set_dynamic_config(tcm_hcd, DC_ENABLE_CLOSED_COVER, enable);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to set dynamic config\n");
		}
	}

	pr_notice("%s: end!\n", __func__);
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static int tpd_test_save_file_path_store(struct tpd_classdev_t *cdev, const char *buf)
{
	memset(synatcm_save_file_path, 0, sizeof(synatcm_save_file_path));
	snprintf(synatcm_save_file_path, sizeof(synatcm_save_file_path), "%s", buf);

	pr_notice("%s: save file path: %s\n", __func__, synatcm_save_file_path);

	return 0;
}

static int tpd_test_save_file_path_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	num_read_chars = snprintf(buf, PAGE_SIZE, "%s", synatcm_save_file_path);
	pr_notice("%s: save file path: %s\n", __func__, synatcm_save_file_path);

	return num_read_chars;
}

static int tpd_test_save_file_name_store(struct tpd_classdev_t *cdev, const char *buf)
{
	memset(synatcm_save_file_name, 0, sizeof(synatcm_save_file_name));

	snprintf(synatcm_save_file_name, sizeof(synatcm_save_file_name), "%s", buf);
	pr_notice("%s: save file path: %s\n", __func__, synatcm_save_file_name);

	return 0;
}

static int tpd_test_save_file_name_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	num_read_chars = snprintf(buf, PAGE_SIZE, "%s\n", synatcm_save_file_name);
	pr_notice("%s: save file path: %s\n", __func__, synatcm_save_file_name);

	return num_read_chars;
}

static int syna_get_col_row(struct tpd_classdev_t *cdev, unsigned int *col, unsigned int *row)
{
	struct syna_tcm_hcd *tcm_hcd = (struct syna_tcm_hcd *)cdev->private;
	int retval;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = syna_tcm_get_app_info(tcm_hcd);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get app info\n");
		goto exit;
	}

	*col = le2_to_uint(tcm_hcd->app_info.num_of_image_cols);
	*row = le2_to_uint(tcm_hcd->app_info.num_of_image_rows);

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static int syna_test_init(struct tpd_classdev_t *cdev)
{
	unsigned int col, row;
	int retval = 0;

	pr_notice("tpd_%s: enter!\n", __func__);
	if (syna_get_col_row(cdev, &col, &row) < 0) {
		pr_err("%s: Failed to get image cols and rows!\n");
		retval = -EIO;
		goto err_syna_get_col_row;
	}

	synatcm_tpd_test = kzalloc(sizeof(struct synatcm_tpd), GFP_KERNEL);
	if (!synatcm_tpd_test) {
		pr_err("synatcm_tpd_test request memory failed!\n");
		retval = -ENOMEM;
		goto err_synatcm_tpd_test;
	}

	synatcm_tpd_test->failed_node_count = 0;
	synatcm_tpd_test->failed_node_buffer_len = 0;
	synatcm_tpd_test->cols = col;
	synatcm_tpd_test->rows = row;
	synatcm_tpd_test->test_result = 0;

	synatcm_tpd_test->synatcm_test_failed_node = kzalloc(col * row * sizeof(char), GFP_KERNEL);
	if (!synatcm_tpd_test->synatcm_test_failed_node) {
		pr_err("synatcm_test_failed_node request memory failed!\n");
		retval = -ENOMEM;
		goto err_synatcm_test_failed_node;
	}

	synatcm_tpd_test->synatcm_test_temp_buffer = kzalloc(col * row * 6 * sizeof(char) + 1, GFP_KERNEL);
	if (!synatcm_tpd_test->synatcm_test_temp_buffer) {
		pr_err("synatcm_test_failed_node request memory failed!\n");
		retval = -ENOMEM;
		goto err_synatcm_test_temp_buffer;
	}

	synatcm_tpd_test->synatcm_test_failed_node_buffer = kzalloc(col * row * 6 * sizeof(char) + 1, GFP_KERNEL);
	if (!synatcm_tpd_test->synatcm_test_failed_node_buffer) {
		pr_err("synatcm_test_failed_node request memory failed!\n");
		retval = -ENOMEM;
		goto err_synatcm_test_failed_node_buffer;
	}

	pr_notice("tpd_%s: success end!\n", __func__);
	return retval;

err_synatcm_test_failed_node_buffer:
	kfree(synatcm_tpd_test->synatcm_test_temp_buffer);

err_synatcm_test_temp_buffer:
	kfree(synatcm_tpd_test->synatcm_test_failed_node);

err_synatcm_test_failed_node:
	kfree(synatcm_tpd_test);

err_synatcm_tpd_test:
err_syna_get_col_row:

	pr_err("tpd_%s: failed end!\n", __func__);
	return retval;
}

static int tpd_test_start(void)
{
	int retval;

	pr_notice("%s: start!\n", __func__);

	if (!synatcm_tpd_test) {
		pr_err("%s: synatcm_tpd_test is null!\n", __func__);
		return -ENOMEM;
	}

	retval = testing_full_raw_cap();
	if (retval < 0)
		pr_err("%s: testing_full_raw_cap is failed!\n", __func__);

	retval = testing_noise();
	if (retval < 0)
		pr_err("%s: testing_noise is failed!\n", __func__);

	pr_notice("%s: success end!\n", __func__);
	return 0;
}

static int tpd_test_free(void)
{
	if (!synatcm_tpd_test) {
		pr_err("%s: not alloc!\n", __func__);
		return 0;
	}

	kfree(synatcm_tpd_test->synatcm_test_failed_node_buffer);
	kfree(synatcm_tpd_test->synatcm_test_temp_buffer);
	kfree(synatcm_tpd_test->synatcm_test_failed_node);
	kfree(synatcm_tpd_test);
	synatcm_tpd_test = NULL;

	pr_notice("%s: success free!\n", __func__);
	return 0;
}

static int tpd_test_cmd_store(struct tpd_classdev_t *cdev, const char *buf)
{
	unsigned int command = 0;
	int retval = 0;

	if (sscanf(buf, "%u", &command) != 1) {
		pr_err("%s: Invalid param: %s\n", __func__, buf);
		return -EIO;
	}

	switch (command) {
	case TP_TEST_INIT:
		retval = syna_test_init(cdev);
		if (retval < 0) {
			pr_err("%s: alloc memory failed!\n", __func__);
			return -ENOMEM;
		}
		break;
	case TP_TEST_START:
		retval = tpd_test_start();
		if (retval < 0) {
			pr_err("%s: test failed!\n", __func__);
		}
		break;
	case TP_TEST_END:
		tpd_test_free();
		break;
	default:
		pr_err("%s: invalid command %u", __func__, command);
	}

	return 0;
}

static int tpd_test_cmd_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;

	if (!synatcm_tpd_test) {
		pr_notice("%s: not alloc!\n", __func__);
		return 0;
	}

	num_read_chars += snprintf(buf, PAGE_SIZE, "%d,%d,%d,%d", synatcm_tpd_test->test_result,
			synatcm_tpd_test->cols, synatcm_tpd_test->rows,
			synatcm_tpd_test->failed_node_count);

	pr_notice("%s: tpd test result:%d && rawdata node failed count: %d\n",
			__func__, synatcm_tpd_test->test_result, synatcm_tpd_test->failed_node_count);

	num_read_chars += snprintf(buf + num_read_chars, PAGE_SIZE - num_read_chars, "%s",
			synatcm_tpd_test->synatcm_test_failed_node_buffer);

	pr_notice("tpd_test: %s\n", buf);

	return num_read_chars;
}

static int tpd_test_channel_show(struct tpd_classdev_t *cdev, char *buf)
{
	ssize_t num_read_chars = 0;
	unsigned int col, row;

	if (syna_get_col_row(cdev, &col, &row) < 0) {
		pr_err("%s: Failed to get image cols and rows!\n");
		return num_read_chars;
	}

	num_read_chars = snprintf(buf, PAGE_SIZE, "%d, %d", col, row);
	pr_notice("%s: col=%d, row=%d\n", __func__, col, row);

	return num_read_chars;
}

static int tpd_set_tp_state(struct tpd_classdev_t *cdev, int enable)
{
#ifndef ZTE_FEATURE_TP_SINGLE_TAP
	struct syna_tcm_hcd *tcm_hcd = (struct syna_tcm_hcd *)cdev->private;

	spin_lock(&tcm_hcd->fp_logo_lock);
	tcm_hcd->in_fp_logo = enable;
	if (tcm_hcd->in_low_power) {
		spin_unlock(&tcm_hcd->fp_logo_lock);
		pr_notice("syna:tp_switch is %d!\n", enable);
		if (enable)
			syna_tcm_node_notifier_cb(true);
		else
			syna_tcm_node_notifier_cb(false);
	} else if (enable) {
		spin_unlock(&tcm_hcd->fp_logo_lock);
		pr_notice("syna:tp_switch %d is remeberd!\n", enable);
	} else {
		spin_unlock(&tcm_hcd->fp_logo_lock);
		pr_notice("syna:tp_switch %d ignored!\n", enable);
	}
#endif
	return 0;
}

static int tpd_test_bsc_calibration_store(struct tpd_classdev_t *cdev, const char *buf)
{
	struct syna_tcm_hcd *tcm_hcd = (struct syna_tcm_hcd *)cdev->private;
	int retval;
	unsigned int command = 0;
	int firmware;

	pr_notice("%s: start!\n", __func__);

	if (sscanf(buf, "%u", &command) != 1) {
		pr_err("%s: Invalid param: %s\n", __func__, buf);
		return -EIO;
	}

	if (!command) {
		pr_notice("%s: command is %d! exit!\n", __func__, command);
		return 0;
	}

	if (!tpd_is_bsc_complete) {
		pr_notice("%s: previous test is still in progress, exit!\n", __func__);
		return 0;
	}
	tpd_is_bsc_complete = false;
	testing_hcd->result = false;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = syna_tcm_get_app_info(tcm_hcd);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to get app info\n");
		goto exit;
	}
	firmware = (unsigned int)tcm_hcd->app_info.customer_config_id[3] +
				(unsigned int)tcm_hcd->app_info.customer_config_id[2] * 0x100;
	pr_notice("%s: firmware is %d\n", __func__, firmware);

	pr_notice("%s: reset start!", __func__);
	retval = tcm_hcd->reset(tcm_hcd, false, true);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do reset\n");
		goto exit;
	}
	pr_notice("%s: reset success!", __func__);

	msleep(100);

	pr_notice("%s: bsc start!", __func__);
	retval = testing_bsc_calibration();
	if (retval < 0) {
		pr_err("%s: test failed!\n");
		goto exit;
	}
	pr_notice("%s: bsc success!", __func__);

	tpd_is_bsc_complete = true;
	pr_notice("%s: result is %d", __func__, testing_hcd->result);
exit:
	mutex_unlock(&tcm_hcd->extif_mutex);
	pr_notice("%s: end!\n", __func__);

	return retval;
}

static int tpd_test_bsc_calibration_show(struct tpd_classdev_t *cdev, char *buf)
{
	int retval;

	pr_notice("%s: start!\n", __func__);

	if (tpd_is_bsc_complete) {
		retval = snprintf(buf, PAGE_SIZE,
			"%s\n",
			testing_hcd->result ? "Passed" : "Failed");
	} else {
		retval = snprintf(buf, PAGE_SIZE, "%s\n", "Wait");
	}
	pr_notice("%s: result is %s, end!\n", __func__, buf);

	return retval;
}

static int tpd_get_noise(struct tpd_classdev_t *cdev, struct list_head *head)
{
	int retval;
	int i = 0;
	char *buf_arry[RT_DATA_NUM];
	struct tp_runtime_data *tp_rt;
	struct syna_tcm_hcd *tcm_hcd = (struct syna_tcm_hcd *)cdev->private;

	if (tcm_hcd->in_suspend)
		return -EIO;

	list_for_each_entry(tp_rt, head, list) {
		buf_arry[i++] = tp_rt->rt_data;
		tp_rt->is_empty = false;
	}

	retval = testing_delta_raw_report(buf_arry, RT_DATA_NUM >> 2);
	if (retval < 0) {
		pr_err("%s: get_raw_noise failed!\n");
		return retval;
	}

	return 0;
}

void synaptics_tpd_register_fw_class(struct syna_tcm_hcd *tcm_hcd)
{
	pr_notice("%s: entry\n", __func__);

	tpd_fw_cdev.private = (void *)tcm_hcd;
	tpd_fw_cdev.get_tpinfo = tpd_init_tpinfo;
#ifdef WAKEUP_GESTURE
	tpd_fw_cdev.get_gesture = tpd_get_wakegesture;
	tpd_fw_cdev.wake_gesture = tpd_enable_wakegesture;

	tpd_fw_cdev.get_singletap = tpd_get_singletapgesture;
	tpd_fw_cdev.set_singletap = tpd_set_singletapgesture;
#endif
	tpd_fw_cdev.get_smart_cover = tpd_get_smart_cover;
	tpd_fw_cdev.set_smart_cover = tpd_set_smart_cover;

	tpd_fw_cdev.set_tp_state = tpd_set_tp_state;
	tpd_fw_cdev.get_noise = tpd_get_noise;

	tpd_fw_cdev.tpd_test_set_save_filepath = tpd_test_save_file_path_store;
	tpd_fw_cdev.tpd_test_get_save_filepath = tpd_test_save_file_path_show;
	tpd_fw_cdev.tpd_test_set_save_filename = tpd_test_save_file_name_store;
	tpd_fw_cdev.tpd_test_get_save_filename = tpd_test_save_file_name_show;
	tpd_fw_cdev.tpd_test_set_cmd = tpd_test_cmd_store;
	tpd_fw_cdev.tpd_test_get_cmd = tpd_test_cmd_show;
	tpd_fw_cdev.tpd_test_get_channel_info = tpd_test_channel_show;
	tpd_fw_cdev.tpd_test_set_bsc_calibration = tpd_test_bsc_calibration_store;
	tpd_fw_cdev.tpd_test_get_bsc_calibration = tpd_test_bsc_calibration_show;

	snprintf(synatcm_save_file_path, sizeof(synatcm_save_file_path),
			"%s", SYNATCM_DEFAULT_OUT_PATH);
	snprintf(synatcm_save_file_name, sizeof(synatcm_save_file_name),
			"%s", SYNATCM_DEFAULT_OUT_FILE);

	pr_notice("%s: end\n", __func__);
}
