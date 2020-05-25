/*
 * DPN(Dynamic Power State Notify) used for notify modem AP power state.
 */

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/io.h>
#include <linux/ktime.h>
#include <linux/version.h>
#include <linux/smp.h>
#include <linux/tick.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/msm-bus.h>
#include <linux/uaccess.h>
#include <linux/dma-mapping.h>
#include <soc/qcom/spm.h>
#include <soc/qcom/pm.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/scm-boot.h>
#include <asm/suspend.h>
#include <asm/cacheflush.h>
#include <asm/cputype.h>
#include <asm/system_misc.h>
#include <soc/qcom/socinfo.h>
#include <linux/seq_file.h>
#include <linux/fb.h>
#include <linux/version.h>
#include <linux/syscore_ops.h>
#include <linux/soc/qcom/qmi.h>
#include <linux/net.h>


#define QMI_VS_SEND_POWER_STATE_REQ_V02                   0x0075
#define QMI_VS_SEND_POWER_STATE_REQ_V02_MAX_MSG_LEN       20

#define POWER_STATE_REQ_MAX_MSG_LEN_V02	                  256
#define QMI_VS_SERVICE_ID                                 0xE4
#define QMI_VS_VER                                        2
#define QMI_POWER_RESP_TIMEOUT                            5000

struct vs_send_power_state_req_msg_v02 {

	/* Mandatory */
	u8 state;
};  /* Message */


struct qmi_vs_response_type_v02 {
	u16 result;
	u16 error;
	u32 vs_error;
};


struct vs_send_power_state_resp_msg_v02 {
  struct qmi_vs_response_type_v02 resp;
};

struct qmi_vs {
	struct qmi_handle handle;
	struct sockaddr_qrtr qmi_vs_qrtr;
	bool connected;
	struct mutex			mutex;
};



struct qmi_elem_info power_state_req_msg_v02_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(uint8_t),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x01,
		.offset         = offsetof(struct vs_send_power_state_req_msg_v02,
							state),
		.ei_array	= NULL,
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.is_array       = QMI_COMMON_TLV_TYPE,
	},
};


struct qmi_elem_info qmi_vs_response_type_v02_ei[] = {
{
	.data_type	= QMI_SIGNED_2_BYTE_ENUM,
	.elem_len	= 1,
	.elem_size	= sizeof(u16),
	.is_array	= NO_ARRAY,
	.tlv_type	= QMI_COMMON_TLV_TYPE,
	.offset		= offsetof(struct qmi_vs_response_type_v02,
					result),
	.ei_array	= NULL,
},
{
	.data_type		= QMI_SIGNED_2_BYTE_ENUM,
	.elem_len		= 1,
	.elem_size		= sizeof(u16),
	.is_array		= NO_ARRAY,
	.tlv_type		= QMI_COMMON_TLV_TYPE,
	.offset			= offsetof(struct qmi_vs_response_type_v02,
						error),
	.ei_array		= NULL,
},
{
	.data_type      = QMI_SIGNED_4_BYTE_ENUM,
	.elem_len       = 1,
	.elem_size      = sizeof(u32),
	.is_array       = NO_ARRAY,
	.tlv_type       = QMI_COMMON_TLV_TYPE,
	.offset         = offsetof(struct qmi_vs_response_type_v02,
						vs_error),
	.ei_array       = NULL,
},
{
	.data_type	= QMI_EOTI,
	.elem_len	= 0,
	.elem_size	= 0,
	.is_array	= NO_ARRAY,
	.tlv_type	= QMI_COMMON_TLV_TYPE,
	.offset		= 0,
	.ei_array	= NULL,
},
};



struct qmi_elem_info power_state_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_vs_response_type_v02),
		.is_array       = NO_ARRAY,
		.tlv_type       = 0x02,
		.offset         = offsetof(struct vs_send_power_state_resp_msg_v02,
				   resp),
		.ei_array      = qmi_vs_response_type_v02_ei,
	},
	{
		.data_type      = QMI_EOTI,
		.is_array       = NO_ARRAY,
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};


static struct qmi_vs qmi_vs_info;


static int vs_new_server(struct qmi_handle *qmi, struct qmi_service *svc)
{
	struct qmi_vs *data = container_of(qmi,
					struct qmi_vs, handle);

	pr_info("Connection established between QMI handle and QMI VS service\n");

	data->qmi_vs_qrtr.sq_family = AF_QIPCRTR;
	data->qmi_vs_qrtr.sq_node = svc->node;
	data->qmi_vs_qrtr.sq_port = svc->port;

	kernel_connect(qmi->sock,
					(struct sockaddr *)&data->qmi_vs_qrtr,
					sizeof(data->qmi_vs_qrtr),
					0);

	data->connected = true;
	return 0;
}

static void vs_del_server(struct qmi_handle *qmi, struct qmi_service *svc)
{
	struct qmi_vs *data = container_of(qmi,
					struct qmi_vs, handle);
	pr_info("Connection lost between QMI handle and QMI VS service\n");
	data->connected = false;
}


struct qmi_ops vs_ops = {
	.new_server = vs_new_server,
	.del_server = vs_del_server,
};


static int qmi_vs_init(void) {

	int ret;

  mutex_init(&qmi_vs_info.mutex);

	ret = qmi_handle_init(&qmi_vs_info.handle,
				POWER_STATE_REQ_MAX_MSG_LEN_V02,
				&vs_ops,
				NULL);
	if (ret < 0)
		return ret;

	ret = qmi_add_lookup(&qmi_vs_info.handle, QMI_VS_SERVICE_ID,
			QMI_VS_VER, 0);
	if (ret < 0)
		return ret;

  return ret;
}

static int dem_qmi_send_power_state(int state)
{
	struct vs_send_power_state_req_msg_v02 req;
	struct qmi_txn txn;
	int ret;

	req.state = state;
	ret = qmi_txn_init(&qmi_vs_info.handle, &txn,
		NULL, NULL);
	if (ret < 0) {
		pr_err("qmi send power state:%d failed txn init ret:%d\n",
			state, ret);
		goto qmi_send_exit;
	}

	ret = qmi_send_request(&qmi_vs_info.handle, &qmi_vs_info.qmi_vs_qrtr,
							&txn,
							QMI_VS_SEND_POWER_STATE_REQ_V02,
							QMI_VS_SEND_POWER_STATE_REQ_V02_MAX_MSG_LEN,
							power_state_req_msg_v02_ei, &req);
	if (ret < 0) {
		pr_err("qmi send power state:%d txn send failed ret:%d\n",
				state, ret);
		qmi_txn_cancel(&txn);
		goto qmi_send_exit;
	}

	qmi_txn_cancel(&txn);
	pr_info("Requested qmi send power state:%d\n", state);

qmi_send_exit:
	return ret;
}


static int dpn_suspend(struct device *dev)
{
	dem_qmi_send_power_state(0);
	return 0;
}

static int dpn_resume(struct device *dev)
{
	dem_qmi_send_power_state(1);
	return 0;
}

static const struct dev_pm_ops dev_dpn_ops = {
	.suspend	= dpn_suspend,
	.resume		= dpn_resume,
};


static const struct of_device_id dpn_table[] = {
	{.compatible = "dynamic_power_state_notify"},
	{},
};



static int dpn_probe(struct platform_device *pdev)
{
	int ret = 0;
	pr_info("%s enter\n", __func__);
	ret = qmi_vs_init();
	return ret;
}

static int  dpn_remove(struct platform_device *pdev)
{
	pr_info("%s enter\n", __func__);
	return 0;
}



static struct platform_driver dpn_driver = {
	.probe = dpn_probe,
	.remove	= dpn_remove,
	.driver = {
		.name = "dynamic_power_state_notify",
		.owner = THIS_MODULE,
		.pm	= &dev_dpn_ops,
		.of_match_table = dpn_table,
	},
};



int __init dpn_init(void)
{
	int ret;
	pr_info("%s enter\n", __func__);
	ret = platform_driver_register(&dpn_driver);
	if (ret) {
		pr_err("%s platform_driver_register failed %d\n", __func__, ret);
		return ret;
	}
	return 0;
}


late_initcall(dpn_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Dynamic power manager for modem");
