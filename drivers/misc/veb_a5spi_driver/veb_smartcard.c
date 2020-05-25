#include <linux/spi/spi.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>

#include "veb_common.h"
#include "veb_platform.h"
#include "veb_errno.h"
#include "veb_crc.h"
#include "veb_base.h"
#include "veb_a5.h"


/*********************************************************************************************
*********************************************************************************************/
#define SCMD_HEAD		0x34A60253
#define SMART_DATALEN	3
#define RECV_DATA_LEN_SIZE	4
#define CRC32_LEN			4

typedef struct {
	smart_cmd_t cmd;
	int type;
	unsigned int inlen;
	unsigned char *inbuf;
	unsigned int *outlen;
	unsigned char *outbuf;
} veb_smartcard_data_t;

/*********************************************************************************************
*********************************************************************************************/
int veb_smartcard_cmd(struct spi_device *spi, smart_cmd_t stCmd, unsigned char *pDataIn,
				unsigned int uInLen, unsigned char *pDataOut, unsigned int *puOutLen)
{
	smart_cmd_t cmd = stCmd;
	unsigned char sw[VEB_IC_STATUS_LENGTH + CRC32_LEN] __VEB_ALIGNED__ = {0};
	unsigned char len_buf[RECV_DATA_LEN_SIZE + CRC32_LEN] __VEB_ALIGNED__ = {0};
	unsigned int crc32_got __VEB_ALIGNED__ = 0;
	unsigned int crc32_cal = 0;
	unsigned char buf_tmp[1024] __VEB_ALIGNED__ = {0};
	unsigned int datalen = 0;
	unsigned int uRSAGen2048KeyFlag = 0;
	int ret = 0;

	VEB_TRACE_IN();

	if (NULL == pDataIn || NULL == pDataOut || NULL == puOutLen) {
		VEB_ERR("pDataIn,pDataOut,puOutLen cannot be NULL!\n");
		return VEB_ERR_PARAM;
	}
	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		VEB_ERR("Try to wake chipset up failed(%d)!\n", ret);
		return VEB_ERR_WAKE_TIMEOUT;
	}

	/* for rsa gen 2048 key */
	if (cmd.cmd[0] == 0x0F) {
		uRSAGen2048KeyFlag = 1;
		cmd.cmd[0] = 0x02;
		cmd.crc8[3] = get_crc8_0xff((unsigned char *)&cmd, CHIP_CMD_CRC_LEN);
	}

	/* send cmd first */
	veb_spi_send(spi, (unsigned char *)&cmd, CMD_T_SIZE);
	VEB_DDUMP_WITH_PREFIX("command :", (unsigned char *)&cmd, CMD_T_SIZE);
	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}

	/* recv sw */
	veb_spi_recv(spi, sw, VEB_IC_STATUS_LENGTH + CRC32_LEN);
	if (sw[0] != 0x00 || sw[1] != 0x90) {
		VEB_ERR("sw status is not correct(0x%02X%02X)!\n", sw[1], sw[0]);
		return VEB_ERR_UNKNOWN;
	}
	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}

	/* send data */
	crc32_cal = get_crc32_mpeg(pDataIn, uInLen);
	memcpy(buf_tmp, pDataIn, uInLen);
	memcpy(buf_tmp + uInLen, (unsigned char *)&crc32_cal, CRC32_LEN);
	VEB_DDUMP_WITH_PREFIX("send data crc32_cal:", (unsigned char *)&crc32_cal, CRC32_LEN);
	veb_spi_send(spi, buf_tmp, uInLen + CRC32_LEN);

	if (uRSAGen2048KeyFlag) {
		ret = veb_rsa_wait_ready(200);
	} else {
		ret = veb_wait_ready();
	}

	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		ret =  VEB_ERR_WAIT_BUSY_TIMEOUT;
		goto end;
	}

	/* recv data len */
	veb_spi_recv(spi, len_buf, RECV_DATA_LEN_SIZE + CRC32_LEN);
	memcpy((unsigned char *)&crc32_got, len_buf + RECV_DATA_LEN_SIZE, CRC32_LEN);
	crc32_cal = get_crc32_mpeg(len_buf, RECV_DATA_LEN_SIZE);
	if (crc32_cal != crc32_got) {
		VEB_ERR("len_buf:crc32(0x%08x:0x%08x) is not correct!\n", crc32_got, crc32_cal);
		ret =  VEB_ERR_CRC;
		goto end;
	}
	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		ret = VEB_ERR_WAIT_BUSY_TIMEOUT;
		goto end;
	}

	/* recv data buf + sw */
	memcpy((unsigned char *)&datalen, len_buf, RECV_DATA_LEN_SIZE);
	VEB_DDUMP_WITH_PREFIX("len_buf :", len_buf, RECV_DATA_LEN_SIZE);
	VEB_DBG("datalen is %d = 0x%8x\n", datalen, datalen);

	veb_spi_recv(spi, buf_tmp, datalen + CRC32_LEN);
	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		ret = VEB_ERR_WAIT_BUSY_TIMEOUT;
		goto end;
	}
	/* copy recv data buf + sw */
	memcpy((unsigned char *)&crc32_got, buf_tmp + datalen, CRC32_LEN);
	VEB_DDUMP_WITH_PREFIX("recv_data buf :", buf_tmp, datalen + CRC32_LEN);
	crc32_cal = get_crc32_mpeg(buf_tmp, datalen);
	if (crc32_cal != crc32_got) {
		VEB_ERR("recv data:crc32(0x%08x:0x%08x) is not correct!\n", crc32_got, crc32_cal);
		ret = VEB_ERR_CRC;
		goto end;
	}
	*puOutLen = datalen;
	memcpy(pDataOut, buf_tmp, datalen);
	ret = VEB_OK;
end:

	return ret;
}


/*************************************************************
 send:	CLA+INS+P1+P2+LE:
		LE = 0, len = 256
 recv:	INS+SW
 recv:	datalen 2byte + 1byte crc (len ^ value len1 ^ len2)
 recv:	data buf + 2 sw
*/
int veb_smartcard_cmd_read(struct spi_device *spi, smart_cmd_t *cmd, char *buf, unsigned int *outlen)
{
	unsigned char sw[VEB_IC_STATUS_LENGTH] __VEB_ALIGNED__ = {0};
	unsigned char len_buf[SMART_DATALEN] __VEB_ALIGNED__ = {0};
	unsigned char *buf_tmp = NULL;
	unsigned char crc;
	unsigned char init_apdu[5] = {0x00, 0xa4, 0x04, 0x00, 0x00};
	int datalen = 0;
	int waittime = 0;
	int ret = 0;

	VEB_TRACE_IN();
	#if 0
	if (cmd->cmd[1] == 0x14) {
		waittime = 50;/*Erease flash 15s time out*/
	} else if (cmd->cmd[1] == 0xA4) {
		waittime = 800;/*90time out*/
	}
	#endif
	if (memcmp(init_apdu, cmd->cmd, 4) == 0) {
		waittime = 90;/*90s time out*/
	} else {
		waittime = 3;/*3s time out*/
	}
	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		VEB_ERR("Try to wake chipset up failed(%d)!\n", ret);
		return VEB_ERR_WAKE_TIMEOUT;
	}

	veb_spi_send(spi, (unsigned char *)cmd, CMD_T_SIZE);
	VEB_DDUMP_WITH_PREFIX("command :", ((unsigned char *)cmd), CMD_T_SIZE);
	if (!!waittime) {
		VEB_ERR("waittime out=%d\n", waittime);
		ret = veb_smartcard_wait_ready(waittime);
	} else {
		ret = veb_wait_ready();
	}
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}
	veb_spi_recv(spi, sw, VEB_IC_STATUS_LENGTH);
	if (cmd->cmd[1] == sw[0] && sw[1] != 0) {
		VEB_ERR("status is not correct(0x%02X%02X)!\n", sw[1], sw[0]);
		return VEB_ERR_UNKNOWN;
	} else if (cmd->cmd[1] != sw[0]) {
		if (outlen != NULL) {
			*outlen = VEB_IC_STATUS_LENGTH;
		}
		memcpy(buf, sw, VEB_IC_STATUS_LENGTH);
		if (sw[0] != 0x90 || sw[1] != 0x00) {
			VEB_ERR("sw status is not correct(0x%02X%02X)!\n", sw[1], sw[0]);
			/*return VEB_ERR_STATUS;*/
		}
		return VEB_OK;
	}
	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}
	/* recv data len*/
	veb_spi_recv(spi, len_buf, SMART_DATALEN);
	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}
	VEB_DDUMP_WITH_PREFIX("len_buf :", len_buf, SMART_DATALEN);
	datalen = len_buf[0] << 8;
	datalen += len_buf[1];

	/**
	 * check crc32 of data Received
	**/
	crc = len_buf[0] ^ len_buf[1];
	if (crc != len_buf[2]) {
		VEB_ERR("data len crc error\n");
		return VEB_ERR_CRC;
	}
	VEB_DBG("datalen is %d\n", datalen);
	buf_tmp = kmalloc(datalen + VEB_IC_STATUS_LENGTH, GFP_KERNEL);
	if (buf_tmp == NULL) {
		VEB_ERR("buf malloc fail\n");
		return VEB_ERR_MALLOC;
	}

	/* recv	 data buf + sw*/
	veb_spi_recv(spi, buf_tmp, datalen);
	ret = veb_wait_ready();
	if (ret != 0) {
		kfree(buf_tmp);
		VEB_ERR("wait for ready time out(%d)\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}
	/* copy recv data buf + sw*/
	memcpy(buf, buf_tmp, datalen);
	/* check sw*/
	memcpy((unsigned char *)sw, buf_tmp + datalen - 2, VEB_IC_STATUS_LENGTH);
	kfree(buf_tmp);
	if (outlen != NULL) {
		*outlen = datalen;
	}
	if (sw[0] != 0x90 || sw[1] != 0x00) {
		VEB_ERR("status is not correct(0x%02X%02X)!\n", sw[1], sw[0]);
		/*return VEB_ERR_UNKNOWN;*/
	}

	VEB_DBG("status 0x%02X%02X\n", sw[1], sw[0]);
	return VEB_OK;
}


/**********************************************
   send cmd: CLA+INS+P1+P2+LC
   send	 DATA
   recv	  sink = data + sw
*/
int veb_smartcard_cmd_exchange(struct spi_device *spi, smart_cmd_t *cmd, unsigned char *source,
		unsigned char *sink, unsigned int *outlen)
{
	unsigned char sw[VEB_IC_STATUS_LENGTH] __VEB_ALIGNED__ = {0};
	unsigned char len_buf[SMART_DATALEN] __VEB_ALIGNED__ = {0};
	unsigned char *buf_tmp;
	unsigned char crc;
	int waittime = 3;
	int datalen = 0, ret = 0;

	VEB_TRACE_IN();
	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		VEB_ERR("Try to wake chipset up failed(%d)!\n", ret);
		return VEB_ERR_WAKE_TIMEOUT;
	}

	veb_spi_send(spi, (unsigned char *)cmd, CMD_T_SIZE);
	VEB_DDUMP_WITH_PREFIX("command :", ((unsigned char *)cmd), CMD_T_SIZE);
	VEB_DDUMP_WITH_PREFIX("source :", ((unsigned char *)source), cmd->len);
	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}
	/* recv INS sw*/
	veb_spi_recv(spi, sw, VEB_IC_STATUS_LENGTH);
	if (cmd->cmd[1] == sw[0] && sw[1] != 0) {
		VEB_ERR("status is not correct(0x%02X%02X)!\n", sw[1], sw[0]);
		return VEB_ERR_UNKNOWN;
	} else if (cmd->cmd[1] != sw[0]) {
		if (outlen != NULL) {
			*outlen = VEB_IC_STATUS_LENGTH;
		}
		memcpy(sink, sw, VEB_IC_STATUS_LENGTH);

		if (sw[0] != 0x90 || sw[1] != 0x00) {
			VEB_ERR("sw status is not correct(0x%02X%02X)!\n", sw[1], sw[0]);
			/*return VEB_ERR_STATUS;*/
		}
		return VEB_OK;
	}
	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}
	/* send data*/
	datalen = cmd->len;
	VEB_DBG("datalen %d\n", datalen);
	veb_spi_send(spi, source, datalen);
	waittime = 3;
	if (!!waittime) {
		VEB_ERR("waittime out=%d\n", waittime);
		ret = veb_smartcard_wait_ready(waittime);
	} else {
		ret = veb_wait_ready();
	}
	/*ret = veb_wait_ready();*/
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}

	/* recv data len*/
	veb_spi_recv(spi, len_buf, SMART_DATALEN);
	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}
	datalen = len_buf[0] << 8;
	datalen += len_buf[1];
	VEB_DDUMP_WITH_PREFIX("len_buf :", ((unsigned char *)len_buf), SMART_DATALEN);

	/**
	 * check crc32 of data Received
	**/
	crc = len_buf[0] ^ len_buf[1];
	VEB_DBG("datalen is %d, len_buf:0x%2x, 0x%2x, 0x%2x,\n", datalen, len_buf[0], len_buf[1], len_buf[2]);

	if (crc != len_buf[2]) {
		VEB_ERR("data len crc error\n");
		return VEB_ERR_CRC;
	}
	VEB_DBG("datalen is %d\n", datalen);

	buf_tmp = kmalloc(datalen + VEB_IC_STATUS_LENGTH, GFP_KERNEL);
	if (buf_tmp == NULL) {
		VEB_ERR("buf malloc fail\n");
		return VEB_ERR_MALLOC;
	}
	/* copy recv data + sw*/
	veb_spi_recv(spi, buf_tmp, datalen);
	ret = veb_wait_ready();
	if (ret != 0) {
		kfree(buf_tmp);
		VEB_ERR("wait for ready time out(%d)\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}
	VEB_DDUMP_WITH_PREFIX("buf_tmp :", ((unsigned char *)buf_tmp), 32);
	if (outlen != NULL) {
		VEB_ERR("data len not NULL\n");
		*outlen = datalen;
	}

	/* copy recv data buf*/
	memcpy(sink, buf_tmp, datalen);
	/* check sw*/
	memcpy((unsigned char *)sw, buf_tmp + datalen - 2, VEB_IC_STATUS_LENGTH);
	kfree(buf_tmp);
	if ((sw[0] != 0x90) || (sw[1] != 0x00)) {
		VEB_ERR("status is not correct(0x%02X%02X)\n", sw[1], sw[0]);
		/*return VEB_ERR_UNKNOWN;*/
	}
	VEB_DBG("status 0x%02X%02X\n", sw[1], sw[0]);

	return VEB_OK;
}

long veb_smartcard_ioctl(veb_private_data_t *priv, unsigned int cmd, unsigned long arg)
{
	veb_smartcard_data_t data;
	long ret = VEB_OK;
	char *buf = NULL;
	unsigned int uOutLen = 0;
	unsigned char inbuf[1024] = {0};

	VEB_TRACE_IN();
	if (copy_from_user(&data, (void __user *)arg, sizeof(veb_smartcard_data_t))) {
		VEB_ERR("copy from user failed!\n");
		return VEB_ERR_COPY_FROM_USER;
	}

	if (copy_from_user(inbuf, data.inbuf, data.inlen)) {
		VEB_ERR("copy from user failed!\n");
		return VEB_ERR_COPY_FROM_USER;
	}

	if (data.type == VEB_DATA_MMAP) {
		mutex_lock(&(priv->mmap_lock));
		buf = priv->mmap_addr;
	} else if ((data.type == VEB_DATA_COPY) && (data.outbuf != NULL)) {
		buf = kmalloc(1024, GFP_KERNEL);
		if (buf == NULL) {
			VEB_ERR("kzalloc(size:%d) failed!\n", ((int)sizeof(veb_smartcard_data_t)));
			return VEB_ERR_MALLOC;
		}
	} else {
		VEB_ERR("type(%d) or s of struct veb_sdata is illegal!\n", data.type);
		return VEB_ERR_PARAM;
	}

	mutex_lock(&(priv->dev_lock));
	ret = veb_smartcard_cmd(priv->spi, data.cmd, inbuf, data.inlen, buf, &uOutLen);
	mutex_unlock(&(priv->dev_lock));
	if (ret != VEB_OK) {
		VEB_ERR("veb_smartcard_cmd(length:%d) failed(%ld)!\n", uOutLen, ret);
		VEB_EDUMP_WITH_PREFIX("command", ((char *)&(data.cmd)), CMD_T_SIZE);
		goto error;
	}

	if (data.type == VEB_DATA_COPY) {
		if (copy_to_user((void __user *)data.outbuf, buf, uOutLen + 4)) {
			VEB_ERR("copy outbuf to user failed!\n");
			ret = VEB_ERR_COPY_TO_USER;
			goto error;
		}

		if (copy_to_user((void __user *)data.outlen, (char *)&uOutLen, sizeof(uOutLen))) {
			VEB_ERR("copy outlength to user failed!\n");
			ret = VEB_ERR_COPY_TO_USER;
			goto error;
		}

	} else if (data.type == VEB_DATA_MMAP) {
		if (copy_to_user((void __user *)data.outlen, (char *)&uOutLen, sizeof(uOutLen))) {
			VEB_ERR("mmap:copy outlength to user failed!\n");
			ret = VEB_ERR_COPY_TO_USER;
			goto error;
		}
	}

error:
	if (data.type == VEB_DATA_MMAP) {
		mutex_unlock(&(priv->mmap_lock));
	} else {
		kfree(buf);
	}
	VEB_TRACE_OUT();
	return ret;
}

long veb_smartcard_ioctl_read(veb_private_data_t *priv, unsigned int cmd, unsigned long arg)
{
	veb_smartcard_data_t data;
	long ret = VEB_OK;
	char *buf = NULL;

	if (copy_from_user(&data, (void __user *)arg, sizeof(veb_smartcard_data_t))) {
		VEB_ERR("copy from user failed!\n");
		return VEB_ERR_COPY_FROM_USER;
	}

	if (data.type == VEB_DATA_MMAP) {
		mutex_lock(&(priv->mmap_lock));
		buf = priv->mmap_addr;
	} else if ((data.type == VEB_DATA_COPY) && (data.outbuf != NULL)) {
		buf = kmalloc(1024, GFP_KERNEL);
		if (buf == NULL) {
			VEB_ERR("kzalloc(size:%d) failed!\n", ((int)sizeof(veb_smartcard_data_t)));
			return VEB_ERR_MALLOC;
		}
	} else {
		VEB_ERR("type(%d) or s of struct veb_sdata is illegal!\n", data.type);
		return VEB_ERR_PARAM;
	}

	mutex_lock(&(priv->dev_lock));
	ret = veb_smartcard_cmd_read(priv->spi, &(data.cmd), buf, data.outlen);
	mutex_unlock(&(priv->dev_lock));
	if (ret != VEB_OK) {
		VEB_ERR("veb_cmd_read(length:%d) failed(%ld)!\n", *data.outlen, ret);
		VEB_EDUMP_WITH_PREFIX("command", ((char *)&(data.cmd)), CMD_T_SIZE);
		goto error;
	}

	if (data.type == VEB_DATA_COPY) {
		if (copy_to_user((void __user *)data.outbuf, buf, *(data.outlen))) {
			VEB_ERR("copy to user failed!\n");
			ret = VEB_ERR_COPY_TO_USER;
			goto error;
		}
	}
error:
	if (data.type == VEB_DATA_MMAP) {
		mutex_unlock(&(priv->mmap_lock));
	} else {
		kfree(buf);
	}
	VEB_TRACE_OUT();
	return ret;
}


long veb_smartcard_ioctl_exchange(veb_private_data_t *priv, unsigned int cmd, unsigned long arg)
{
	veb_smartcard_data_t d;
	long ret = VEB_OK;
	unsigned char *buf = NULL;

	VEB_TRACE_IN();
	if (copy_from_user(&d, (void __user *)arg, sizeof(veb_smartcard_data_t))) {
		VEB_ERR("copy from user failed\n");
		return VEB_ERR_COPY_FROM_USER;
	}

	if (d.type == VEB_DATA_MMAP) {
		mutex_lock(&(priv->mmap_lock));
		buf = priv->mmap_addr;
	} else if ((d.type == VEB_DATA_COPY) && (d.inbuf != NULL) && (d.outbuf != NULL)) {
		buf = kmalloc(1024, GFP_KERNEL);
		if (buf == NULL) {
			VEB_ERR("kzalloc(size:%d) failed\n", ((int)sizeof(veb_smartcard_data_t)));
			return VEB_ERR_MALLOC;
		}

		if (copy_from_user(buf, d.inbuf, d.inlen)) {
			kfree(buf);
			VEB_ERR("copy from user failed\n");
			return VEB_ERR_COPY_FROM_USER;
		}
	} else {
		VEB_ERR("type(%d) or s of struct veb_sdata is illegal\n", d.type);
		return VEB_ERR_PARAM;
	}

	mutex_lock(&(priv->dev_lock));
	ret = veb_smartcard_cmd_exchange(priv->spi, &(d.cmd), buf, buf, d.outlen);
	mutex_unlock(&(priv->dev_lock));
	if (ret != VEB_OK) {
		VEB_ERR("veb_cmd_read failed(%ld)\n", ret);
		VEB_EDUMP_WITH_PREFIX("command", ((char *)&(d.cmd)), CMD_T_SIZE);
		goto error;
	}

	VEB_DBG("*(d.outlen) %d\n", *(d.outlen));
	if (d.type == VEB_DATA_COPY) {
		if (copy_to_user(d.outbuf, buf, *(d.outlen))) {
			kfree(buf);
			VEB_ERR("copy from user failed\n");
			return VEB_ERR_COPY_FROM_USER;
		}
	}

error:
	if (d.type == VEB_DATA_MMAP) {
		mutex_unlock(&(priv->mmap_lock));
	} else {
		kfree(buf);
		VEB_ERR("veb_cmd_exchange(source:%d) failed(%ld)\n", *(d.outlen), ret);
		VEB_EDUMP_WITH_PREFIX("command", ((char *)&(d.cmd)), CMD_T_SIZE);
	}

	VEB_TRACE_OUT();
	return ret;
}
