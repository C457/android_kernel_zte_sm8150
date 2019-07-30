/*
 * Synaptics TCM touchscreen driver
 *
 * Copyright (C) 2017-2018 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2017-2018 Scott Lin <scott.lin@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <linux/err.h>


#include "synaptics_tcm_core.h"
#include "synaptics_tcm_testing.h"
#include "synaptics_common_interface.h"

#define SYSFS_DIR_NAME "testing"

#define REPORT_TIMEOUT_MS 5000
/* FOR PR2901381 */
#define CFG_SATURATION_LEVEL_OFFSET (8736)
#define CFG_SATURATION_LEVEL_LENGTH (16)
#define CFG_TAG_TOUCH_THRESHOLD_PCT_OFFSET (13128)
#define CFG_TAG_TOUCH_THRESHOLD_PCT_LENGTH (8)

#define CHECK_BIT(var, pos) ((var) & (1<<(pos)))

#define DYNAMIC_DATA_TEST_CSV_FILE "/sdcard/tpdata/DynamicDataTest.csv"
#define NOISE_TEST_CSV_FILE "/sdcard/tpdata/NoiseTest.csv"
#define PT11_TEST_CSV_FILE "/sdcard/tpdata/Pt11Test.csv"

#ifdef CONFIG_TP_COMMON_INTERFACE
extern struct synatcm_tpd *synatcm_tpd_test;
#endif

#define testing_sysfs_show(t_name) \
static ssize_t testing_sysfs_##t_name##_show(struct device *dev, \
		struct device_attribute *attr, char *buf) \
{ \
	int retval; \
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd; \
\
	mutex_lock(&tcm_hcd->extif_mutex); \
\
	retval = testing_##t_name(); \
	if (retval < 0) { \
		LOGE(tcm_hcd->pdev->dev.parent, \
				"Failed to do "#t_name" test\n"); \
		goto exit; \
	} \
\
	retval = snprintf(buf, PAGE_SIZE, \
			"%s\n", \
			testing_hcd->result ? "Passed" : "Failed"); \
\
exit: \
	mutex_unlock(&tcm_hcd->extif_mutex); \
\
	return retval; \
}

DECLARE_COMPLETION(report_complete);

DECLARE_COMPLETION(testing_remove_complete);
/*
static int testing_full_raw_cap(void);

static int testing_dynamic_range(void);

static int testing_dynamic_range_lpwg(void);

static int testing_dynamic_range_doze(void);

static int testing_noise(void);

static int testing_noise_lpwg(void);

static int testing_noise_doze(void);

static int testing_open_short_detector(void);

static int testing_pt11(void);

static int testing_pt12(void);

static int testing_pt13(void);

static int testing_reset_open(void);

static int testing_lockdown(void);

static int testing_trx(enum test_code test_code);
*/
#ifdef CONFIG_TP_COMMON_INTERFACE
static void syna_save_failed_node_to_buffer(struct synatcm_tpd *synatcm_tpd_test);
#endif

struct testing_hcd *testing_hcd;

SHOW_PROTOTYPE(testing, full_raw_cap)
SHOW_PROTOTYPE(testing, dynamic_range)
SHOW_PROTOTYPE(testing, dynamic_range_lpwg)
SHOW_PROTOTYPE(testing, dynamic_range_doze)
SHOW_PROTOTYPE(testing, noise)
SHOW_PROTOTYPE(testing, noise_lpwg)
SHOW_PROTOTYPE(testing, noise_doze)
SHOW_PROTOTYPE(testing, open_short_detector)
SHOW_PROTOTYPE(testing, pt11)
SHOW_PROTOTYPE(testing, pt12)
SHOW_PROTOTYPE(testing, pt13)
SHOW_PROTOTYPE(testing, reset_open)
SHOW_PROTOTYPE(testing, lockdown)
SHOW_PROTOTYPE(testing, trx_trx_shorts)
SHOW_PROTOTYPE(testing, trx_sensor_opens)
SHOW_PROTOTYPE(testing, trx_ground_shorts)
SHOW_PROTOTYPE(testing, size)
SHOW_PROTOTYPE(testing, bsc_calibration)

static struct device_attribute *attrs[] = {
	ATTRIFY(full_raw_cap),
	ATTRIFY(dynamic_range),
	ATTRIFY(dynamic_range_lpwg),
	ATTRIFY(dynamic_range_doze),
	ATTRIFY(noise),
	ATTRIFY(noise_lpwg),
	ATTRIFY(noise_doze),
	ATTRIFY(open_short_detector),
	ATTRIFY(pt11),
	ATTRIFY(pt12),
	ATTRIFY(pt13),
	ATTRIFY(reset_open),
	ATTRIFY(lockdown),
	ATTRIFY(trx_trx_shorts),
	ATTRIFY(trx_sensor_opens),
	ATTRIFY(trx_ground_shorts),
	ATTRIFY(size),
	ATTRIFY(bsc_calibration),
};

static ssize_t testing_sysfs_data_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static struct bin_attribute bin_attr = {
	.attr = {
		.name = "data",
		.mode = S_IRUGO,
	},
	.size = 0,
	.read = testing_sysfs_data_show,
};

testing_sysfs_show(full_raw_cap)

testing_sysfs_show(dynamic_range)

testing_sysfs_show(dynamic_range_lpwg)

testing_sysfs_show(dynamic_range_doze)

testing_sysfs_show(noise)

testing_sysfs_show(noise_lpwg)

testing_sysfs_show(noise_doze)

testing_sysfs_show(open_short_detector)

testing_sysfs_show(pt11)

testing_sysfs_show(pt12)

testing_sysfs_show(pt13)

testing_sysfs_show(reset_open)

testing_sysfs_show(lockdown)

testing_sysfs_show(bsc_calibration)

static ssize_t testing_sysfs_trx_trx_shorts_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = testing_trx(TEST_TRX_TRX_SHORTS);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do TRX-TRX shorts test\n");
		goto exit;
	}

	retval = snprintf(buf, PAGE_SIZE,
			"%s\n",
			testing_hcd->result ? "Passed" : "Failed");

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t testing_sysfs_trx_sensor_opens_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = testing_trx(TEST_TRX_SENSOR_OPENS);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do TRX-sensor opens test\n");
		goto exit;
	}

	retval = snprintf(buf, PAGE_SIZE,
			"%s\n",
			testing_hcd->result ? "Passed" : "Failed");

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t testing_sysfs_trx_ground_shorts_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	retval = testing_trx(TEST_TRX_GROUND_SHORTS);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do TRX-ground shorts test\n");
		goto exit;
	}

	retval = snprintf(buf, PAGE_SIZE,
			"%s\n",
			testing_hcd->result ? "Passed" : "Failed");

exit:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t testing_sysfs_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	LOCK_BUFFER(testing_hcd->output);

	retval = snprintf(buf, PAGE_SIZE,
			"%u\n",
			testing_hcd->output.data_length);

	UNLOCK_BUFFER(testing_hcd->output);

	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static ssize_t testing_sysfs_data_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	unsigned int readlen;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	mutex_lock(&tcm_hcd->extif_mutex);

	LOCK_BUFFER(testing_hcd->output);

	readlen = MIN(count, testing_hcd->output.data_length - pos);

	retval = secure_memcpy(buf,
			count,
			&testing_hcd->output.buf[pos],
			testing_hcd->output.buf_size - pos,
			readlen);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"Failed to copy report data\n");
	} else {
		retval = readlen;
	}

	UNLOCK_BUFFER(testing_hcd->output);

	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

static int testing_run_prod_test_item(enum test_code test_code)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	if (tcm_hcd->features.dual_firmware &&
			tcm_hcd->id_info.mode != MODE_PRODUCTION_TEST) {
		retval = tcm_hcd->switch_mode(tcm_hcd, FW_MODE_PRODUCTION_TEST);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to run production test firmware\n");
			return retval;
		}
	} else if (tcm_hcd->id_info.mode != MODE_APPLICATION ||
			tcm_hcd->app_status != APP_STATUS_OK) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Application firmware not running\n");
		return -ENODEV;
	}

	LOCK_BUFFER(testing_hcd->out);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&testing_hcd->out,
			1);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd->out.buf\n");
		UNLOCK_BUFFER(testing_hcd->out);
		return retval;
	}

	testing_hcd->out.buf[0] = test_code;

	LOCK_BUFFER(testing_hcd->resp);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_PRODUCTION_TEST,
			testing_hcd->out.buf,
			1,
			&testing_hcd->resp.buf,
			&testing_hcd->resp.buf_size,
			&testing_hcd->resp.data_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_PRODUCTION_TEST));
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->out);
		return retval;
	}

	UNLOCK_BUFFER(testing_hcd->resp);
	UNLOCK_BUFFER(testing_hcd->out);

	return 0;
}

static int testing_collect_reports(enum report_type report_type,
		unsigned int num_of_reports)
{
	int retval;
	bool completed;
	unsigned int timeout;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	testing_hcd->report_index = 0;
	testing_hcd->report_type = report_type;
	testing_hcd->num_of_reports = num_of_reports;

#if (KERNEL_VERSION(3, 13, 0) <= LINUX_VERSION_CODE)
	reinit_completion(&report_complete);
#else
	INIT_COMPLETION(report_complete);
#endif

	LOCK_BUFFER(testing_hcd->out);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&testing_hcd->out,
			1);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd->out.buf\n");
		UNLOCK_BUFFER(testing_hcd->out);
		goto exit;
	}

	testing_hcd->out.buf[0] = testing_hcd->report_type;

	LOCK_BUFFER(testing_hcd->resp);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_ENABLE_REPORT,
			testing_hcd->out.buf,
			1,
			&testing_hcd->resp.buf,
			&testing_hcd->resp.buf_size,
			&testing_hcd->resp.data_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_ENABLE_REPORT));
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->out);
		goto exit;
	}

	UNLOCK_BUFFER(testing_hcd->resp);
	UNLOCK_BUFFER(testing_hcd->out);

	completed = false;
	timeout = REPORT_TIMEOUT_MS * num_of_reports;

	retval = wait_for_completion_timeout(&report_complete,
			msecs_to_jiffies(timeout));
	if (retval == 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Timed out waiting for report collection\n");
	} else {
		completed = true;
	}

	LOCK_BUFFER(testing_hcd->out);

	testing_hcd->out.buf[0] = testing_hcd->report_type;

	LOCK_BUFFER(testing_hcd->resp);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_DISABLE_REPORT,
			testing_hcd->out.buf,
			1,
			&testing_hcd->resp.buf,
			&testing_hcd->resp.buf_size,
			&testing_hcd->resp.data_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_DISABLE_REPORT));
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->out);
		goto exit;
	}

	UNLOCK_BUFFER(testing_hcd->resp);
	UNLOCK_BUFFER(testing_hcd->out);

	if (completed)
		retval = 0;
	else
		retval = -EIO;

exit:
	testing_hcd->report_type = 0;

	return retval;
}

static void testing_get_frame_size_words(unsigned int *size, bool image_only)
{
	unsigned int rows;
	unsigned int cols;
	unsigned int hybrid;
	unsigned int buttons;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);
	hybrid = le2_to_uint(app_info->has_hybrid_data);
	buttons = le2_to_uint(app_info->num_of_buttons);

	*size = rows * cols;

	if (!image_only) {
		if (hybrid)
			*size += rows + cols;
		*size += buttons;
	}
}

static void testing_doze_frame_output(unsigned int rows, unsigned int cols)
{
	int retval;
	unsigned int data_size;
	unsigned int header_size;
	unsigned int output_size;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	header_size = 2;

	data_size = rows * cols;

	if (le2_to_uint(app_info->num_of_buttons))
		data_size++;

	output_size = header_size + data_size * 2;

	LOCK_BUFFER(testing_hcd->output);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&testing_hcd->output,
			output_size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd->output.buf\n");
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	testing_hcd->output.buf[0] = rows;
	testing_hcd->output.buf[1] = cols;

	output_size = header_size;

	LOCK_BUFFER(testing_hcd->resp);

	retval = secure_memcpy(testing_hcd->output.buf + header_size,
			testing_hcd->output.buf_size - header_size,
			testing_hcd->resp.buf,
			testing_hcd->resp.buf_size,
			testing_hcd->resp.data_length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy test data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	output_size += testing_hcd->resp.data_length;

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_hcd->output.data_length = output_size;

	UNLOCK_BUFFER(testing_hcd->output);
}

static int test_save_data_to_csv(short *data, int x_ch, int y_ch, const char *file_path, int offset)
{
	int x = 0;
	int y = 0;
	int iArrayIndex = 0;
	struct file *fp = NULL;
	char *fbufp = NULL;
	mm_segment_t org_fs;
	int write_ret = 0;
	unsigned int output_len = 0;
	loff_t pos = 0;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	kfree(fbufp);
	fbufp = NULL;

	fbufp = kzalloc(8192, GFP_KERNEL);
	if (!fbufp) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"kzalloc for fbufp failed!\n");
		return -ENOMEM;
	}

	for (y = 0; y < y_ch; y++) {
		for (x = 0; x < x_ch; x++) {
			iArrayIndex = y * x_ch + x;
			pr_notice("%5d, ", data[iArrayIndex]);
			snprintf(fbufp + iArrayIndex * 7 + y * 2, sizeof(fbufp) - iArrayIndex * 7 + y * 2,
					"%5d, ", data[iArrayIndex]);
		}
		pr_notice("\n");
		snprintf(fbufp + (iArrayIndex + 1) * 7 + y * 2,
				sizeof(fbufp) - (iArrayIndex + 1) * 7 + y * 2,
				"\r\n");
	}
	snprintf(fbufp + (iArrayIndex + 1) * 7 + y_ch * 2,
			sizeof(fbufp) - (iArrayIndex + 1) * 7 + y_ch * 2,
			"\r\n");

	org_fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(file_path, O_RDWR | O_CREAT, 0644);
	if (IS_ERR_OR_NULL(fp)) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"open %s failed\n", file_path);
		set_fs(org_fs);
		kfree(fbufp);
		fbufp = NULL;
		return -EPERM;
	}

	output_len = strlen(fbufp); /* y_ch * x_ch * 7 + y_ch * 2; */

	pos = offset * output_len;
	write_ret = vfs_write(fp, (char __user *)fbufp, output_len, &pos);
	if (write_ret <= 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"write %s failed\n", file_path);
		set_fs(org_fs);
		if (fp) {
			filp_close(fp, NULL);
			fp = NULL;
		}
		kfree(fbufp);
		fbufp = NULL;
		return -EPERM;
	}

	set_fs(org_fs);

	if (fp) {
		filp_close(fp, NULL);
		fp = NULL;
	}

	kfree(fbufp);
	fbufp = NULL;

	return 0;
}

static void testing_standard_frame_output(bool image_only)
{
	int retval;
	unsigned int data_size;
	unsigned int header_size;
	unsigned int output_size;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	testing_get_frame_size_words(&data_size, image_only);

	header_size = sizeof(app_info->num_of_buttons) +
			sizeof(app_info->num_of_image_rows) +
			sizeof(app_info->num_of_image_cols) +
			sizeof(app_info->has_hybrid_data);

	output_size = header_size + data_size * 2;

	LOCK_BUFFER(testing_hcd->output);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&testing_hcd->output,
			output_size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd->output.buf\n");
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	retval = secure_memcpy(testing_hcd->output.buf,
			testing_hcd->output.buf_size,
			&app_info->num_of_buttons[0],
			header_size,
			header_size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy header data\n");
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	output_size = header_size;

	LOCK_BUFFER(testing_hcd->resp);

	retval = secure_memcpy(testing_hcd->output.buf + header_size,
			testing_hcd->output.buf_size - header_size,
			testing_hcd->resp.buf,
			testing_hcd->resp.buf_size,
			testing_hcd->resp.data_length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy test data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	output_size += testing_hcd->resp.data_length;

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_hcd->output.data_length = output_size;

	UNLOCK_BUFFER(testing_hcd->output);
}

int testing_dynamic_range_doze(void)
{
	int retval;
	unsigned char *buf;
	unsigned int idx;
	unsigned int row;
	unsigned int col;
	unsigned int data;
	unsigned int rows;
	unsigned int cols;
	unsigned int data_size;
	unsigned int limits_rows;
	unsigned int limits_cols;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	cols = le2_to_uint(app_info->num_of_image_cols);

	retval = testing_run_prod_test_item(TEST_DYNAMIC_RANGE_DOZE);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	data_size = testing_hcd->resp.data_length / 2;

	if (le2_to_uint(app_info->num_of_buttons))
		data_size--;

	if (data_size % cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid max number of rows per burst\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	rows = data_size / cols;

	limits_rows = ARRAY_SIZE(drt_hi_limits);
	limits_cols = ARRAY_SIZE(drt_hi_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows = ARRAY_SIZE(drt_lo_limits);
	limits_cols = ARRAY_SIZE(drt_lo_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	idx = 0;
	buf = testing_hcd->resp.buf;
	testing_hcd->result = true;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			data = le2_to_uint(&buf[idx * 2]);
			if (data > drt_hi_limits[row][col] ||
					data < drt_lo_limits[row][col]) {
				testing_hcd->result = false;
				break;
			}
			idx++;
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_doze_frame_output(rows, cols);

	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	return retval;
}

int testing_dynamic_range_lpwg(void)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	retval = tcm_hcd->set_dynamic_config(tcm_hcd,
			DC_IN_WAKEUP_GESTURE_MODE,
			1);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to enable wakeup gesture mode\n");
		return retval;
	}

	retval = testing_dynamic_range();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do dynamic range test\n");
		return retval;
	}

	retval = tcm_hcd->set_dynamic_config(tcm_hcd,
			DC_IN_WAKEUP_GESTURE_MODE,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to disable wakeup gesture mode\n");
		return retval;
	}

	return 0;
}

static int print_data2buffer(char *buff_arry[], unsigned int cols, unsigned int rows,
		unsigned int image_size_words, unsigned int *frame_data_words, int idex, int idx, char *name)
{
	size_t count = 0;
	size_t retlen;
	int retval = 0;
	unsigned int x;
	unsigned int y;

	count = 0;
	retlen = snprintf(buff_arry[idex] + count, RT_DATA_LEN - count,
				"\n%s image:\n", name);
	if (retlen < 0) {
		retval = -EINVAL;
		goto exit;
	}
	count += retlen;

	for (y = 0; y < rows; y++) {
		for (x = 0; x < cols; x++) {
			retlen = snprintf(buff_arry[idex] + count, RT_DATA_LEN - count,
						"%d, ", frame_data_words[y * cols + x + idx * image_size_words]);
			if (retlen < 0) {
				retval = -EINVAL;
				goto exit;
			}
			count += retlen;
		}
		retlen = snprintf(buff_arry[idex] + count, RT_DATA_LEN - count, "\n");
		if (retlen < 0) {
			retval = -EINVAL;
			goto exit;
		}
		count += retlen;
	}

	count = 0;
	retlen = snprintf(buff_arry[idex + 1] + count, RT_DATA_LEN - count,
				"\n%s hybrid data:\n", name);
	if (retlen < 0) {
		retval = -EINVAL;
		goto exit;
	}
	count += retlen;

	for (x = 0; x < cols; x++) {
		retlen = snprintf(buff_arry[idex + 1] + count, RT_DATA_LEN - count,
					"%d, ", frame_data_words[rows * cols + x + idx * image_size_words]);
		if (retlen < 0) {
			retval = -EINVAL;
			goto exit;
		}
		count += retlen;
	}
	retlen = snprintf(buff_arry[idex + 1] + count, RT_DATA_LEN - count,
				"\n\n");
	if (retlen < 0) {
		retval = -EINVAL;
		goto exit;
	}
	count += retlen;

	for (y = 0; y < rows; y++) {
		retlen = snprintf(buff_arry[idex + 1] + count, RT_DATA_LEN - count,
					"%d, ", frame_data_words[rows * cols + cols + y + idx * image_size_words]);
		if (retlen < 0) {
			retval = -EINVAL;
			goto exit;
		}
		count += retlen;
	}
	retlen = snprintf(buff_arry[idex + 1] + count, RT_DATA_LEN - count,
				"\n\n");
	if (retlen < 0) {
		retval = -EINVAL;
		goto exit;
	}
	retval = 0;

exit:
	return retval;
}

static int data_request(unsigned int **frame_data_words, unsigned int num_of_reports,
		unsigned int image_size_words, enum report_type report_type)
{
	int retval = 0;
	unsigned char *frame_data;
	unsigned int idx;

	retval = testing_hcd->collect_reports(report_type, num_of_reports);
	if (retval < 0) {
		pr_err("tpd: Failed to collect delta report\n");
		goto exit;
	}

	frame_data = testing_hcd->report.buf;

	*frame_data_words = kcalloc(num_of_reports * image_size_words, sizeof(unsigned int), GFP_KERNEL);
	if (!(*frame_data_words)) {
		pr_err("tpd: memory access failed!\n");
		retval = -ENOMEM;
		goto exit;
	}

	for (idx = 0; idx < image_size_words * num_of_reports; idx++)
			(*frame_data_words)[idx] = (short)le2_to_uint(&frame_data[idx * 2]);

	retval = 0;
exit:
	return retval;
}

ssize_t testing_delta_raw_report(char *buff_arry[], unsigned int num_of_reports)
{
	int retval;
	int idex;
	unsigned int *frame_data_words = NULL;
	unsigned int *frame_data_words1 = NULL;
	unsigned int cols;
	unsigned int rows;
	unsigned int idx;
	unsigned int image_size_words;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	image_size_words = rows * cols + rows + cols;

	mutex_lock(&tcm_hcd->extif_mutex);

	tcm_hcd->set_dynamic_config(tcm_hcd, DC_NO_DOZE, 1);
	retval = data_request(&frame_data_words, num_of_reports, image_size_words, REPORT_DELTA);
	if (retval < 0) {
		pr_err("data_request failed!\n");
		goto DATA_REQUEST_FAILED;
	}

	retval = data_request(&frame_data_words1, num_of_reports, image_size_words, REPORT_RAW);
	if (retval < 0) {
		pr_err("data_request failed!\n");
		goto DATA_REQUEST1_FAILED;
	}
	tcm_hcd->set_dynamic_config(tcm_hcd, DC_NO_DOZE, 0);

	for (idx = 0; idx < num_of_reports; idx++) {

		idex = idx << 2;
		retval = print_data2buffer(buff_arry, cols, rows, image_size_words, frame_data_words,
						idex, idx, "Delta");
		if (retval < 0) {
			pr_err("print_data2buffer Delta failed!\n");
			goto DATA_PRINT_FAILED;
		}

		idex += 2;
		retval = print_data2buffer(buff_arry, cols, rows, image_size_words, frame_data_words1,
						idex, idx, "Raw");
		if (retval < 0) {
			pr_err("print_data2buffer Delta failed!\n");
			goto DATA_PRINT_FAILED;
		}
	}

	retval = 0;

DATA_PRINT_FAILED:
	kfree(frame_data_words1);
	frame_data_words1 = NULL;

DATA_REQUEST1_FAILED:
	kfree(frame_data_words);
	frame_data_words = NULL;

DATA_REQUEST_FAILED:
	mutex_unlock(&tcm_hcd->extif_mutex);

	return retval;
}

int testing_full_raw_cap(void)
{
	int retval;
	unsigned char *buf;
	unsigned int idx;
	unsigned int row;
	unsigned int col;
	unsigned int data;
	unsigned int rows;
	unsigned int cols;
	unsigned int limits_rows;
	unsigned int limits_cols;
	unsigned int frame_size_words;
	int len = 0;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	testing_get_frame_size_words(&frame_size_words, true);

	pr_notice("tpd: testing_full_raw_cap start!\n");
	retval = testing_run_prod_test_item(TEST_FULL_RAW_CAP);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (frame_size_words != testing_hcd->resp.data_length / 2) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Frame size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows = ARRAY_SIZE(raw_hi_limits);
	limits_cols = ARRAY_SIZE(raw_hi_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows = ARRAY_SIZE(raw_lo_limits);
	limits_cols = ARRAY_SIZE(raw_lo_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	idx = 0;
	buf = testing_hcd->resp.buf;
	testing_hcd->result = true;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			data = le2_to_uint(&buf[idx * 2]);
			pr_notice("syna: raw_cap_data = %d\n", data);
			if (data > raw_hi_limits[row][col] ||
					data < raw_lo_limits[row][col]) {
				testing_hcd->result = false;
				pr_notice("tpd_syna: %s is failed!, row = %d, col = %d\n",
						__func__, row, col);
#ifdef CONFIG_TP_COMMON_INTERFACE
				if (synatcm_tpd_test) {
					synatcm_tpd_test->test_result = 1;
					if (synatcm_tpd_test->synatcm_test_failed_node[row * cols + col] == 0) {
						synatcm_tpd_test->synatcm_test_failed_node[row * cols + col] = 1;
						len += snprintf(synatcm_tpd_test->synatcm_test_temp_buffer + len,
							sizeof(synatcm_tpd_test->synatcm_test_temp_buffer) - len,
							",%d,%d", row, col);
						synatcm_tpd_test->failed_node_count++;
					}
				} else
#endif
				{
					break;
				}
			}
			idx++;
		}
		pr_notice("\n");
	}
#ifdef CONFIG_TP_COMMON_INTERFACE
	if (!testing_hcd->result && synatcm_tpd_test) {
		syna_save_failed_node_to_buffer(synatcm_tpd_test);
	}
#endif
	pr_notice("\n");

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_standard_frame_output(false);

	pr_notice("tpd: testing_full_raw_cap success end!\n");
	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	pr_notice("tpd: testing_full_raw_cap end!\n");
	return retval;
}

#ifdef CONFIG_TP_COMMON_INTERFACE
static void syna_save_failed_node_to_buffer(struct synatcm_tpd *synatcm_tpd_test)
{
	if (!synatcm_tpd_test) {
		pr_err("synatcm_tpd_test is null!\n");
		return;
	}

	synatcm_tpd_test->failed_node_buffer_len += snprintf(
		synatcm_tpd_test->synatcm_test_failed_node_buffer + synatcm_tpd_test->failed_node_buffer_len,
			sizeof(synatcm_tpd_test->synatcm_test_failed_node_buffer)
			- synatcm_tpd_test->failed_node_buffer_len,
			"%s", synatcm_tpd_test->synatcm_test_temp_buffer);
}
#endif

int testing_dynamic_range(void)
{
	int retval;
	unsigned char *buf;
	unsigned int idx;
	unsigned int row;
	unsigned int col;
	unsigned int data;
	unsigned int rows;
	unsigned int cols;
	unsigned int limits_rows;
	unsigned int limits_cols;
	unsigned int frame_size_words;
	static unsigned int count = 0;
	short *dynamic_data;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	testing_get_frame_size_words(&frame_size_words, false);

	retval = testing_run_prod_test_item(TEST_DYNAMIC_RANGE);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (frame_size_words != testing_hcd->resp.data_length / 2) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Frame size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows = ARRAY_SIZE(drt_hi_limits);
	limits_cols = ARRAY_SIZE(drt_hi_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows = ARRAY_SIZE(drt_lo_limits);
	limits_cols = ARRAY_SIZE(drt_lo_limits[0]);
	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	idx = 0;
	buf = testing_hcd->resp.buf;
	testing_hcd->result = true;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			data = le2_to_uint(&buf[idx * 2]);
			pr_notice("%d, ", data);
			if (data > drt_hi_limits[row][col] ||
					data < drt_lo_limits[row][col]) {
				testing_hcd->result = false;
				break;
			}
			idx++;
		}
		pr_notice("\n");
	}
	pr_notice("\n");

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_standard_frame_output(false);

	dynamic_data = (short *)buf;

	retval = test_save_data_to_csv(dynamic_data, cols, rows, DYNAMIC_DATA_TEST_CSV_FILE, count);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"save raw test data to CSV file failed\n");
		/*return -EAGAIN;*/
	}

	count++;

	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	return retval;
}

int testing_noise_doze(void)
{
	int retval;
	short data;
	unsigned char *buf;
	unsigned int idx;
	unsigned int row;
	unsigned int col;
	unsigned int rows;
	unsigned int cols;
	unsigned int data_size;
	unsigned int limits_rows;
	unsigned int limits_cols;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	cols = le2_to_uint(app_info->num_of_image_cols);

	retval = testing_run_prod_test_item(TEST_NOISE_DOZE);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	data_size = testing_hcd->resp.data_length / 2;

	if (le2_to_uint(app_info->num_of_buttons))
		data_size--;

	if (data_size % cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Invalid max number of rows per burst\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	rows = data_size / cols;

	limits_rows = ARRAY_SIZE(noise_limits);
	limits_cols = ARRAY_SIZE(noise_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	idx = 0;
	buf = testing_hcd->resp.buf;
	testing_hcd->result = true;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			data = (short)le2_to_uint(&buf[idx * 2]);
			if (data > noise_limits[row][col]) {
				testing_hcd->result = false;
				break;
			}
			idx++;
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_doze_frame_output(rows, cols);

	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	return retval;
}

int testing_noise_lpwg(void)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	retval = tcm_hcd->set_dynamic_config(tcm_hcd,
			DC_IN_WAKEUP_GESTURE_MODE,
			1);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to enable wakeup gesture mode\n");
		return retval;
	}

	retval = testing_noise();
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do noise test\n");
		return retval;
	}

	retval = tcm_hcd->set_dynamic_config(tcm_hcd,
			DC_IN_WAKEUP_GESTURE_MODE,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to disable wakeup gesture mode\n");
		return retval;
	}

	return 0;
}

int noise_report(char *buffer)
{
	int retval;
	short data;
	unsigned char *buf;
	unsigned int idx;
	unsigned int row;
	unsigned int col;
	unsigned int rows;
	unsigned int cols;
	int count = 0;
	unsigned int frame_size_words;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	pr_notice("tpd: noise report start!\n");
	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	testing_get_frame_size_words(&frame_size_words, true);

	retval = testing_run_prod_test_item(TEST_NOISE);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (frame_size_words != testing_hcd->resp.data_length / 2) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Frame size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	idx = 0;
	buf = testing_hcd->resp.buf;
	testing_hcd->result = true;

	retval = snprintf(buffer + count, PAGE_SIZE - count,
				"%s:\n", __func__);
	if (retval < 0)
		goto exit;
	count += retval;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			data = (short)le2_to_uint(&buf[idx * 2]);
			retval = snprintf(buffer + count, PAGE_SIZE - count,
						"%d, ", data);
			if (retval < 0)
				goto exit;
			count += retval;
			idx++;
		}
		retval = snprintf(buffer + count, PAGE_SIZE - count, "\n");
		if (retval < 0)
			goto exit;
		count += retval;
	}
	UNLOCK_BUFFER(testing_hcd->resp);

	testing_standard_frame_output(false);

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	pr_notice("tpd: noise report end!\n");
	return count;
}

int testing_noise(void)
{
	int retval;
	short data;
	unsigned char *buf;
	unsigned int idx;
	unsigned int row;
	unsigned int col;
	unsigned int rows;
	unsigned int cols;
	unsigned int len;
	unsigned int limits_rows;
	unsigned int limits_cols;
	unsigned int frame_size_words;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	pr_notice("tpd: testing_noise start!\n");
	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	testing_get_frame_size_words(&frame_size_words, true);

	retval = testing_run_prod_test_item(TEST_NOISE);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (frame_size_words != testing_hcd->resp.data_length / 2) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Frame size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows = ARRAY_SIZE(noise_limits);
	limits_cols = ARRAY_SIZE(noise_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	idx = 0;
	buf = testing_hcd->resp.buf;
	testing_hcd->result = true;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			data = (short)le2_to_uint(&buf[idx * 2]);
			pr_notice("syna_noise: data = %d\n", data);
			if (data > noise_limits[row][col]) {
				testing_hcd->result = false;
				pr_notice("tpd_syna: %s is failed!, row = %d, col = %d\n",
						__func__, row, col);
#ifdef CONFIG_TP_COMMON_INTERFACE
				if (synatcm_tpd_test) {
					synatcm_tpd_test->test_result = 1;
					if (synatcm_tpd_test->synatcm_test_failed_node[row * cols + col] == 0) {
						synatcm_tpd_test->synatcm_test_failed_node[row * cols + col] = 1;
						len += snprintf(synatcm_tpd_test->synatcm_test_temp_buffer + len,
							sizeof(synatcm_tpd_test->synatcm_test_temp_buffer) - len,
							",%d,%d", row, col);
						synatcm_tpd_test->failed_node_count++;
					}
				} else
#endif
				{
					break;
				}

			}
			idx++;
		}
	}
#ifdef CONFIG_TP_COMMON_INTERFACE
	if (!testing_hcd->result && synatcm_tpd_test) {
		syna_save_failed_node_to_buffer(synatcm_tpd_test);
	}
#endif
	UNLOCK_BUFFER(testing_hcd->resp);

	testing_standard_frame_output(false);

	pr_notice("tpd: testing_noise success end!\n");
	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	pr_notice("tpd: testing_noise end!\n");
	return retval;
}

void testing_open_short_detector_output(void)
{
	int retval;
	unsigned int rows;
	unsigned int cols;
	unsigned int data_size;
	unsigned int header_size;
	unsigned int output_size;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);
	data_size = (rows * cols + 7) / 8;

	header_size = sizeof(app_info->num_of_buttons) +
			sizeof(app_info->num_of_image_rows) +
			sizeof(app_info->num_of_image_cols) +
			sizeof(app_info->has_hybrid_data);

	output_size = header_size + data_size * 2;

	LOCK_BUFFER(testing_hcd->output);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&testing_hcd->output,
			output_size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd->output.buf\n");
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	retval = secure_memcpy(testing_hcd->output.buf,
			testing_hcd->output.buf_size,
			&app_info->num_of_buttons[0],
			header_size,
			header_size);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy header data\n");
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	output_size = header_size;

	LOCK_BUFFER(testing_hcd->resp);

	retval = secure_memcpy(testing_hcd->output.buf + header_size,
			testing_hcd->output.buf_size - header_size,
			testing_hcd->resp.buf,
			testing_hcd->resp.buf_size,
			testing_hcd->resp.data_length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy test data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	output_size += testing_hcd->resp.data_length;

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_hcd->output.data_length = output_size;

	UNLOCK_BUFFER(testing_hcd->output);
}

int testing_open_short_detector(void)
{
	int retval;
	unsigned int bit;
	unsigned int byte;
	unsigned int row;
	unsigned int col;
	unsigned int rows;
	unsigned int cols;
	unsigned int data_size;
	unsigned char *data;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);
	data_size = (rows * cols + 7) / 8;

	retval = testing_run_prod_test_item(TEST_OPEN_SHORT_DETECTOR);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (data_size * 2 != testing_hcd->resp.data_length) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Data size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	testing_hcd->result = true;

	bit = 0;
	byte = 0;
	data = &testing_hcd->resp.buf[0];
	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			if (data[byte] & (1 << bit)) {
				testing_hcd->result = false;
				break;
			}
			if (bit++ > 7) {
				bit = 0;
				byte++;
			}
		}
	}

	if (testing_hcd->result == true) {
		bit = 0;
		byte = 0;
		data = &testing_hcd->resp.buf[data_size];
		for (row = 0; row < rows; row++) {
			for (col = 0; col < cols; col++) {
				if (data[byte] & (1 << bit)) {
					testing_hcd->result = false;
					break;
				}
				if (bit++ > 7) {
					bit = 0;
					byte++;
				}
			}
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_open_short_detector_output();

	retval = 0;

exit:
	if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to do reset\n");
	}

	return retval;
}

int testing_pt11(void)
{
	int retval;
	short data;
	unsigned char *buf;
	unsigned int idx;
	unsigned int row;
	unsigned int col;
	unsigned int rows;
	unsigned int cols;
	unsigned int limits_rows;
	unsigned int limits_cols;
	unsigned int image_size_words;
	short *pt11_data;
	static unsigned int count;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	testing_get_frame_size_words(&image_size_words, true);

	retval = testing_run_prod_test_item(TEST_PT11);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (image_size_words != testing_hcd->resp.data_length / 2) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Image size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows = ARRAY_SIZE(pt11_hi_limits);
	limits_cols = ARRAY_SIZE(pt11_hi_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows = ARRAY_SIZE(pt11_lo_limits);
	limits_cols = ARRAY_SIZE(pt11_lo_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	idx = 0;
	buf = testing_hcd->resp.buf;
	testing_hcd->result = true;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			data = (short)le2_to_uint(&buf[idx * 2]);
			if (data > pt11_hi_limits[row][col] ||
					data < pt11_lo_limits[row][col]) {
				testing_hcd->result = false;
				break;
			}
			idx++;
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_standard_frame_output(true);

	pt11_data = (short *)buf;

	retval = test_save_data_to_csv(pt11_data, cols, rows, PT11_TEST_CSV_FILE, count);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
			"save raw test data to CSV file failed\n");
		/*return -EAGAIN;*/
	}

	count++;


	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	return retval;
}

int testing_pt12(void)
{
	int retval;
	short data;
	unsigned char *buf;
	unsigned int idx;
	unsigned int row;
	unsigned int col;
	unsigned int rows;
	unsigned int cols;
	unsigned int limits_rows;
	unsigned int limits_cols;
	unsigned int image_size_words;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	testing_get_frame_size_words(&image_size_words, true);

	retval = testing_run_prod_test_item(TEST_PT12);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (image_size_words != testing_hcd->resp.data_length / 2) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Image size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows = ARRAY_SIZE(pt12_limits);
	limits_cols = ARRAY_SIZE(pt12_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	idx = 0;
	buf = testing_hcd->resp.buf;
	testing_hcd->result = true;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			data = (short)le2_to_uint(&buf[idx * 2]);
			if (data < pt12_limits[row][col]) {
				testing_hcd->result = false;
				break;
			}
			idx++;
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_standard_frame_output(true);

	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	return retval;
}

int testing_pt13(void)
{
	int retval;
	short data;
	unsigned char *buf;
	unsigned int idx;
	unsigned int row;
	unsigned int col;
	unsigned int rows;
	unsigned int cols;
	unsigned int limits_rows;
	unsigned int limits_cols;
	unsigned int image_size_words;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	app_info = &tcm_hcd->app_info;

	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);

	testing_get_frame_size_words(&image_size_words, true);

	retval = testing_run_prod_test_item(TEST_PT13);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	if (image_size_words != testing_hcd->resp.data_length / 2) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Image size mismatch\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	limits_rows = ARRAY_SIZE(pt13_limits);
	limits_cols = ARRAY_SIZE(pt13_limits[0]);

	if (rows > limits_rows || cols > limits_cols) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		retval = -EINVAL;
		goto exit;
	}

	idx = 0;
	buf = testing_hcd->resp.buf;
	testing_hcd->result = true;

	for (row = 0; row < rows; row++) {
		for (col = 0; col < cols; col++) {
			data = (short)le2_to_uint(&buf[idx * 2]);
			if (data < pt13_limits[row][col]) {
				testing_hcd->result = false;
				break;
			}
			idx++;
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_standard_frame_output(true);

	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	return retval;
}

int testing_reset_open(void)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;
	const struct syna_tcm_board_data *bdata = tcm_hcd->hw_if->bdata;

	if (bdata->reset_gpio < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Hardware reset unavailable\n");
		return -EINVAL;
	}

	mutex_lock(&tcm_hcd->reset_mutex);

	tcm_hcd->update_watchdog(tcm_hcd, false);

	gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
	msleep(bdata->reset_active_ms);
	gpio_set_value(bdata->reset_gpio, !bdata->reset_on_state);
	msleep(bdata->reset_delay_ms);

	tcm_hcd->update_watchdog(tcm_hcd, true);

	mutex_unlock(&tcm_hcd->reset_mutex);

	if (tcm_hcd->id_info.mode == MODE_APPLICATION) {
		retval = tcm_hcd->switch_mode(tcm_hcd, FW_MODE_BOOTLOADER);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to enter bootloader mode\n");
			return retval;
		}
	} else {
		retval = tcm_hcd->identify(tcm_hcd, false);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do identification\n");
			goto run_app_firmware;
		}
	}

	if (tcm_hcd->boot_info.last_reset_reason == reset_open_limit)
		testing_hcd->result = true;
	else
		testing_hcd->result = false;

	retval = 0;

run_app_firmware:
	if (tcm_hcd->switch_mode(tcm_hcd, FW_MODE_APPLICATION) < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run application firmware\n");
	}

	return retval;
}

void testing_lockdown_output(void)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	LOCK_BUFFER(testing_hcd->output);
	LOCK_BUFFER(testing_hcd->resp);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&testing_hcd->output,
			testing_hcd->resp.data_length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd->output.buf\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	retval = secure_memcpy(testing_hcd->output.buf,
			testing_hcd->output.buf_size,
			testing_hcd->resp.buf,
			testing_hcd->resp.buf_size,
			testing_hcd->resp.data_length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy test data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	testing_hcd->output.data_length = testing_hcd->resp.data_length;

	UNLOCK_BUFFER(testing_hcd->resp);
	UNLOCK_BUFFER(testing_hcd->output);
}

int testing_lockdown(void)
{
	int retval;
	unsigned int idx;
	unsigned int lockdown_size;
	unsigned int limits_size;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	if (tcm_hcd->read_flash_data == NULL) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Unable to read from flash\n");
		return -EINVAL;
	}

	LOCK_BUFFER(testing_hcd->resp);

	retval = tcm_hcd->read_flash_data(CUSTOM_OTP, true, &testing_hcd->resp);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to read lockdown data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		return retval;
	}

	lockdown_size = testing_hcd->resp.data_length;

	limits_size = ARRAY_SIZE(lockdown_limits);
	if (lockdown_size != limits_size) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Mismatching limits data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		return -EINVAL;
	}

	testing_hcd->result = true;

	for (idx = 0; idx < lockdown_size; idx++) {
		if (testing_hcd->resp.buf[idx] != lockdown_limits[idx]) {
			testing_hcd->result = false;
			break;
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_lockdown_output();

	return 0;
}

void testing_trx_output(void)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	LOCK_BUFFER(testing_hcd->output);
	LOCK_BUFFER(testing_hcd->resp);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&testing_hcd->output,
			testing_hcd->resp.data_length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd->output.buf\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	retval = secure_memcpy(testing_hcd->output.buf,
			testing_hcd->output.buf_size,
			testing_hcd->resp.buf,
			testing_hcd->resp.buf_size,
			testing_hcd->resp.data_length);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy test data\n");
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->output);
		return;
	}

	testing_hcd->output.data_length = testing_hcd->resp.data_length;

	UNLOCK_BUFFER(testing_hcd->resp);
	UNLOCK_BUFFER(testing_hcd->output);
}

int testing_trx(enum test_code test_code)
{
	int retval;
	unsigned char pass_vector;
	unsigned int idx;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	switch (test_code) {
	case TEST_TRX_TRX_SHORTS:
	case TEST_TRX_GROUND_SHORTS:
		pass_vector = 0xff;
		break;
	case TEST_TRX_SENSOR_OPENS:
		pass_vector = 0x00;
		break;
	default:
		return -EINVAL;
	}

	retval = testing_run_prod_test_item(test_code);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(testing_hcd->resp);

	testing_hcd->result = true;

	for (idx = 0; idx < testing_hcd->resp.data_length; idx++) {
		if (testing_hcd->resp.buf[idx] != pass_vector) {
			testing_hcd->result = false;
			break;
		}
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	testing_trx_output();

	retval = 0;

exit:
	if (tcm_hcd->features.dual_firmware) {
		if (tcm_hcd->reset(tcm_hcd, false, true) < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to do reset\n");
		}
	}

	return retval;
}

static int testing_get_static_config(unsigned char *buf, unsigned int buf_len)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	if (!buf) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"invalid parameter\n");
		return -EINVAL;
	}

	LOCK_BUFFER(testing_hcd->out);
	LOCK_BUFFER(testing_hcd->resp);

	retval = tcm_hcd->write_message(tcm_hcd,
					CMD_GET_STATIC_CONFIG,
					NULL,
					0,
					&testing_hcd->resp.buf,
					&testing_hcd->resp.buf_size,
					&testing_hcd->resp.data_length,
					NULL,
					0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_GET_STATIC_CONFIG));
		goto exit;
	}

	if (testing_hcd->resp.data_length != buf_len) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Cfg size mismatch\n");
		retval = -EINVAL;
		goto exit;
	}

	retval = secure_memcpy(buf,
				buf_len,
				testing_hcd->resp.buf,
				testing_hcd->resp.buf_size,
				buf_len);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to copy cfg data\n");
		goto exit;
	}

exit:
	UNLOCK_BUFFER(testing_hcd->resp);
	UNLOCK_BUFFER(testing_hcd->out);

	return retval;
}

static void testing_report(void)
{
	int retval;
	unsigned int offset;
	unsigned int report_size;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;

	report_size = tcm_hcd->report.buffer.data_length;

	LOCK_BUFFER(testing_hcd->report);

	if (testing_hcd->report_index == 0) {
		retval = syna_tcm_alloc_mem(tcm_hcd,
				&testing_hcd->report,
				report_size * testing_hcd->num_of_reports);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to allocate memory for testing_hcd->report.buf\n");
			UNLOCK_BUFFER(testing_hcd->report);
			return;
		}
	}

	if (testing_hcd->report_index < testing_hcd->num_of_reports) {
		offset = report_size * testing_hcd->report_index;

		retval = secure_memcpy(testing_hcd->report.buf + offset,
				testing_hcd->report.buf_size - offset,
				tcm_hcd->report.buffer.buf,
				tcm_hcd->report.buffer.buf_size,
				tcm_hcd->report.buffer.data_length);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to copy report data\n");
			UNLOCK_BUFFER(testing_hcd->report);
			return;
		}

		testing_hcd->report_index++;
		testing_hcd->report.data_length += report_size;
	}

	UNLOCK_BUFFER(testing_hcd->report);

	if (testing_hcd->report_index == testing_hcd->num_of_reports)
		complete(&report_complete);
}


int testing_bsc_calibration(void)
{
	int retval;
	unsigned char *buf;
	unsigned int idx;
	unsigned int rows;
	unsigned int cols;
	unsigned int frame_size;
	struct syna_tcm_app_info *app_info;
	struct syna_tcm_hcd *tcm_hcd = testing_hcd->tcm_hcd;
	unsigned short cstat;
	unsigned short limit;
	unsigned int satic_cfg_length;
	unsigned char *satic_cfg_buf = NULL;

	int offset_saturation_level = CFG_SATURATION_LEVEL_OFFSET/8;
	int offset_tag_touch_threshold_pct = CFG_TAG_TOUCH_THRESHOLD_PCT_OFFSET/8;

	app_info = &tcm_hcd->app_info;
	rows = le2_to_uint(app_info->num_of_image_rows);
	cols = le2_to_uint(app_info->num_of_image_cols);
	frame_size = rows * cols * 2;
	LOGD(tcm_hcd->pdev->dev.parent,
			"frame_size = %d, rows = %d, cols = %d\n", frame_size, rows, cols);

	LOCK_BUFFER(testing_hcd->out);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&testing_hcd->out,
			1);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd->out.buf\n");
		UNLOCK_BUFFER(testing_hcd->out);
		goto exit;
	}

	testing_hcd->out.buf[0] = REPORT_TOUCH;
	LOCK_BUFFER(testing_hcd->resp);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_DISABLE_REPORT,
			testing_hcd->out.buf,
			1,
			&testing_hcd->resp.buf,
			&testing_hcd->resp.buf_size,
			&testing_hcd->resp.data_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_ENABLE_REPORT));
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->out);
		goto exit;
	}

	UNLOCK_BUFFER(testing_hcd->resp);

	LOGD(tcm_hcd->pdev->dev.parent,
			"Sending BSC erase command...\n");

	testing_hcd->out.buf[0] = 3;
	LOCK_BUFFER(testing_hcd->resp);
	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_CALIBRATE,
			testing_hcd->out.buf,
			1,
			&testing_hcd->resp.buf,
			&testing_hcd->resp.buf_size,
			&testing_hcd->resp.data_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write 1st command %s\n",
				STR(CMD_CALIBRATE));
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->out);
		goto exit;
	}
	UNLOCK_BUFFER(testing_hcd->resp);

	LOGD(tcm_hcd->pdev->dev.parent,
			"Sending BSC calibration command...\n");

	testing_hcd->out.buf[0] = 2;
	LOCK_BUFFER(testing_hcd->resp);
	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_CALIBRATE,
			testing_hcd->out.buf,
			1,
			&testing_hcd->resp.buf,
			&testing_hcd->resp.buf_size,
			&testing_hcd->resp.data_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write 2nd command %s\n",
				STR(CMD_CALIBRATE));
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->out);
		goto exit;
	}
	UNLOCK_BUFFER(testing_hcd->resp);
	UNLOCK_BUFFER(testing_hcd->out);

	if (!satic_cfg_buf) {
		satic_cfg_length = le2_to_uint(app_info->static_config_size);
		LOGE(tcm_hcd->pdev->dev.parent,
			"satic_cfg_length = %d\n", satic_cfg_length);

		satic_cfg_buf = kzalloc(satic_cfg_length, GFP_KERNEL);

		if (!satic_cfg_buf) {
			LOGE(tcm_hcd->pdev->dev.parent,
				"Failed on memory allocation for satic_cfg_buf\n");
			goto exit;
		}

		retval = testing_get_static_config(satic_cfg_buf, satic_cfg_length);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to get static config\n");
			goto exit;
		}
	}

	cstat = (unsigned short)(satic_cfg_buf[offset_saturation_level] |
				(unsigned short)satic_cfg_buf[offset_saturation_level+1] << 8);

	limit = (unsigned short)(satic_cfg_buf[offset_tag_touch_threshold_pct]);

	LOGD(tcm_hcd->pdev->dev.parent,
		"cstat = %d and limit factor = %d\n", cstat, limit);

	limit = ((cstat * limit) / 512);

	LOGD(tcm_hcd->pdev->dev.parent,
		"The checking limit is %d\n", limit);

	pr_notice("syna: The checking limit is %d\n", limit);
	testing_hcd->collect_reports(REPORT_192, 1);
	if (frame_size == testing_hcd->report.data_length) {
		buf = testing_hcd->report.buf;
		testing_hcd->result = true;
		for (idx = 0; idx < testing_hcd->report.data_length; idx++) {
			if (buf[idx] > limit) {
				LOGE(tcm_hcd->pdev->dev.parent,
					"Error, hit the limit : %d, Index[%d] = %d\n", limit, idx, buf[idx]);
				testing_hcd->result = false;
			}
		}
	}

	LOCK_BUFFER(testing_hcd->out);
	testing_hcd->out.buf[0] = REPORT_TOUCH;
	LOCK_BUFFER(testing_hcd->resp);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_ENABLE_REPORT,
			testing_hcd->out.buf,
			1,
			&testing_hcd->resp.buf,
			&testing_hcd->resp.buf_size,
			&testing_hcd->resp.data_length,
			NULL,
			0);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_ENABLE_REPORT));
		UNLOCK_BUFFER(testing_hcd->resp);
		UNLOCK_BUFFER(testing_hcd->out);
		goto exit;
	}

	UNLOCK_BUFFER(testing_hcd->resp);
	UNLOCK_BUFFER(testing_hcd->out);
	retval = 0;

exit:
	kfree(satic_cfg_buf);
	return retval;
}


static int testing_init(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;
	int idx;

	testing_hcd = kzalloc(sizeof(*testing_hcd), GFP_KERNEL);
	if (!testing_hcd) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for testing_hcd\n");
		return -ENOMEM;
	}

	testing_hcd->tcm_hcd = tcm_hcd;

	testing_hcd->collect_reports = testing_collect_reports;

	INIT_BUFFER(testing_hcd->out, false);
	INIT_BUFFER(testing_hcd->resp, false);
	INIT_BUFFER(testing_hcd->report, false);
	INIT_BUFFER(testing_hcd->process, false);
	INIT_BUFFER(testing_hcd->output, false);

	testing_hcd->sysfs_dir = kobject_create_and_add(SYSFS_DIR_NAME,
			tcm_hcd->sysfs_dir);
	if (!testing_hcd->sysfs_dir) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create sysfs directory\n");
		retval = -EINVAL;
		goto err_sysfs_create_dir;
	}

	for (idx = 0; idx < ARRAY_SIZE(attrs); idx++) {
		retval = sysfs_create_file(testing_hcd->sysfs_dir,
				&(*attrs[idx]).attr);
		if (retval < 0) {
			LOGE(tcm_hcd->pdev->dev.parent,
					"Failed to create sysfs file\n");
			goto err_sysfs_create_file;
		}
	}

	retval = sysfs_create_bin_file(testing_hcd->sysfs_dir, &bin_attr);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"Failed to create sysfs bin file\n");
		goto err_sysfs_create_bin_file;
	}

	return 0;

err_sysfs_create_bin_file:
err_sysfs_create_file:
	for (idx--; idx >= 0; idx--)
		sysfs_remove_file(testing_hcd->sysfs_dir, &(*attrs[idx]).attr);

	kobject_put(testing_hcd->sysfs_dir);

err_sysfs_create_dir:
	RELEASE_BUFFER(testing_hcd->output);
	RELEASE_BUFFER(testing_hcd->process);
	RELEASE_BUFFER(testing_hcd->report);
	RELEASE_BUFFER(testing_hcd->resp);
	RELEASE_BUFFER(testing_hcd->out);

	kfree(testing_hcd);
	testing_hcd = NULL;

	return retval;
}

static int testing_remove(struct syna_tcm_hcd *tcm_hcd)
{
	int idx;

	if (!testing_hcd)
		goto exit;

	sysfs_remove_bin_file(testing_hcd->sysfs_dir, &bin_attr);

	for (idx = 0; idx < ARRAY_SIZE(attrs); idx++)
		sysfs_remove_file(testing_hcd->sysfs_dir, &(*attrs[idx]).attr);

	kobject_put(testing_hcd->sysfs_dir);

	RELEASE_BUFFER(testing_hcd->output);
	RELEASE_BUFFER(testing_hcd->process);
	RELEASE_BUFFER(testing_hcd->report);
	RELEASE_BUFFER(testing_hcd->resp);
	RELEASE_BUFFER(testing_hcd->out);

	kfree(testing_hcd);
	testing_hcd = NULL;

exit:
	complete(&testing_remove_complete);

	return 0;
}

static int testing_reset(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;

	if (!testing_hcd) {
		retval = testing_init(tcm_hcd);
		return retval;
	}

	return 0;
}

static int testing_syncbox(struct syna_tcm_hcd *tcm_hcd)
{
	if (!testing_hcd)
		return 0;

	if (tcm_hcd->report.id == testing_hcd->report_type)
		testing_report();

	return 0;
}

static struct syna_tcm_module_cb testing_module = {
	.type = TCM_TESTING,
	.init = testing_init,
	.remove = testing_remove,
	.syncbox = testing_syncbox,
	.asyncbox = NULL,
	.reset = testing_reset,
	.suspend = NULL,
	.resume = NULL,
	.early_suspend = NULL,
};

static int __init testing_module_init(void)
{
	return syna_tcm_add_module(&testing_module, true);
}

static void __exit testing_module_exit(void)
{
	syna_tcm_add_module(&testing_module, false);

	wait_for_completion(&testing_remove_complete);
}

module_init(testing_module_init);
module_exit(testing_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics TCM Testing Module");
MODULE_LICENSE("GPL v2");
