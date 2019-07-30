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

int check_sw(unsigned char *sw)
{
	int ret = VEB_OK;

	VEB_TRACE_IN();
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_INF("check_sw err:0x%x%x\n", sw[0], sw[1]);

		if (sw[1] == 0x6C) {
			VEB_ERR("len %d is the expected value\n", sw[0]);
			ret = VEB_ERR_LEN;
		} else if (sw[1] == 0x63 && sw[0] == 0xCF) {
			VEB_ERR("set pin need cpuid verify pass, otherwise fail!\n");
			ret = VEB_ERR_CPUID_VERIFY;
		} else if (sw[1] == 0x67 && sw[0] == 0x00) {
			VEB_ERR("len error.\n");
			ret = VEB_ERR_LEN;
		} else if ((sw[1] == 0x6B) && (sw[0] == 0)) {
			VEB_ERR("params error.\n");
			ret = VEB_ERR_PARAM;
		} else if ((sw[1] == 0x6C) && (sw[0] == 0x10)) {
			VEB_ERR("ins data len is error.\n");
			ret = VEB_ERR_LEN;
		} else if ((sw[1] == 0x6C) && (sw[0] == 0x20)) {
			VEB_ERR("key len error.\n");
			ret = VEB_ERR_EXPECT_LEN;
		} else if ((sw[1] == 0x69) && (sw[0] == 0x88)) {
			VEB_ERR("crc data check error.\n");
			ret = VEB_ERR_SHA;
		} else if ((sw[1] == 0x6A) && (sw[0] == 0x82)) {
			VEB_ERR("The Corresponding Keyid key doesn't exist, or key id max or keyid cannot export.\n");
			ret = VEB_ERR_KEY_EXIST;
		} else if ((sw[1] == 0x6A) && (sw[0] == 0x89)) {
			VEB_ERR("cpuid pin can set once, reset fail.\n");
			ret = VEB_ERR_CPUID_PIN_SET;
		} else if ((sw[1] == 0x6D) && (sw[0] == 0)) {
			VEB_ERR("cmd ins error.\n");
			ret = VEB_ERR_INS;
		} else if ((sw[1] == 0x00) && (sw[0] == 0x60)) {
			VEB_ERR("key data empty.\n");
			ret = VEB_ERR_USERKEY_EMPTY;
		} else if ((sw[1] == 0x90) && (sw[0] == 0x86)) {
			VEB_ERR("sm2 sign check fail.\n");
			ret = VEB_ERR_SM2_SIGNCHECK;
		} else if ((sw[1] == 0x6F) && (sw[0] == 0x00)) {
			VEB_ERR("crc data error.\n");
			ret = VEB_ERR_CRC;
		} else if ((sw[1] == 0x6F) && (sw[0] == 0x09)) {
			VEB_ERR("rsa key gen fail or compute fail.\n");
			ret = VEB_ERR_RSA_COMPUTE;
		} else if ((sw[1] == 0x6F) && (sw[0] == 0x11)) {
			VEB_ERR("SM2 decrypt fail.\n");
			ret = VEB_ERR_SM2_DECRYTO;
		} else if ((sw[1] == 0x6F) && (sw[0] == 0x12)) {
			VEB_ERR("KeyID priv key can't export.\n");
			ret = VEB_ERR_KEYID_CANNOT_EXPORT;
		} else if ((sw[1] == 0x6F) && (sw[0] == 0x13)) {
			VEB_ERR("priv or public key doesn't exist.\n");
			ret = VEB_ERR_ASYMMETRIC_KEY_EXIST;
		} else if ((sw[1] == 0x6F) && (sw[0] == 0x45)) {
			VEB_ERR("flash program or erase fail.\n");
			ret = VEB_ERR_FLASH_PROGRAM_ERASE;
		} else if ((sw[1] == 0x6F) && (sw[0] == 0x82)) {
			VEB_ERR("sm2 export key pare, p2 parm error.\n");
			ret = VEB_ERR_SM2_PARAM;
		} else if (sw[1] == 0x6F) {
			char check_type[20];

			/* 0x01 AES, 0x02 SM4, 0x04 SM3, 0x08 SM2, 0x10 RandNum*/
			if (sw[0] == 0x01) {
				strlcpy(check_type, "AES", sizeof(check_type));
			} else if (sw[0] == 0x02) {
				strlcpy(check_type, "SM4", sizeof(check_type));
			} else if (sw[0] == 0x04) {
				strlcpy(check_type, "SM3", sizeof(check_type));
			} else if (sw[0] == 0x08) {
				strlcpy(check_type, "SM2", sizeof(check_type));
			} else if (sw[0] == 0x10) {
				strlcpy(check_type, "RandNum", sizeof(check_type));
			}
			VEB_ERR("self checking %s error.\n", check_type);
			ret = VEB_ERR_SELF_CHECKING;
		} else {
		VEB_ERR("unknown err, reset a5 !\n");
		veb_spi_reset();
		ret = VEB_ERR_CHECK_SW_ERR;
		}
	}

	VEB_TRACE_OUT();
	return ret;
}

int veb_spi_recv(struct spi_device *spi, char *rx, int length)
{
	struct spi_transfer t = {
		.cs_change = 0,
		.bits_per_word = VEB_SPI_BITS_PER_WORD,
		.delay_usecs = 3,
	};
	struct spi_message m;

	int status = 0;
	unsigned char  *dma_buf_tx;
	unsigned char  *dma_buf_rx;

	VEB_TRACE_IN();

	dma_buf_tx = kzalloc(length, GFP_KERNEL | GFP_DMA);
	dma_buf_rx = kzalloc(length, GFP_KERNEL | GFP_DMA);

	/**
	 * Because of a bug in mtk platform, we need to set tx_buf.
	 */
	t.rx_buf = dma_buf_rx;
	t.tx_buf = dma_buf_tx;
	t.len = length;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	status = spi_sync(spi, &m);
	if (status != 0) {
		VEB_ERR("spi recv failed(%d)!\n", status);
		kfree(dma_buf_tx);
		kfree(dma_buf_rx);
		return status;
	}

	memcpy(rx, dma_buf_rx, length);
	kfree(dma_buf_tx);
	kfree(dma_buf_rx);
	VEB_TRACE_OUT();
	return status;
}

int veb_spi_send(struct spi_device *spi, char *tx, int length)
{
	struct spi_transfer t = {
		.cs_change = 0,
		.bits_per_word = VEB_SPI_BITS_PER_WORD,
		.delay_usecs = 3,
	};
	struct spi_message m;

	int status = 0;
	unsigned char  *dma_buf_tx;
	unsigned char  *dma_buf_rx;

	VEB_TRACE_IN();

	dma_buf_tx = kzalloc(length, GFP_KERNEL | GFP_DMA);
	dma_buf_rx = kzalloc(length, GFP_KERNEL | GFP_DMA);
	memcpy(dma_buf_tx, tx, length);
	t.tx_buf = dma_buf_tx;
	t.rx_buf = dma_buf_rx;
	t.len = length;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	status = spi_sync(spi, &m);

	if (status != 0) {
		kfree(dma_buf_tx);
		kfree(dma_buf_rx);
		VEB_ERR("spi send failed(%d)!\n", status);
		return status;
	}

	kfree(dma_buf_tx);
	kfree(dma_buf_rx);
	VEB_TRACE_OUT();
	return status;
}

int veb_spi_duplex(struct spi_device *spi, char *tx, char *rx, int length)
{
	struct spi_transfer t = {
		.cs_change = 0,
		.bits_per_word = VEB_SPI_BITS_PER_WORD,
		.delay_usecs = 3,
	};
	struct spi_message m;

	int status = 0;
	unsigned char  *dma_buf_tx;
	unsigned char  *dma_buf_rx;

	VEB_TRACE_IN();

	dma_buf_tx = kzalloc(length, GFP_KERNEL | GFP_DMA);
	dma_buf_rx = kzalloc(length, GFP_KERNEL | GFP_DMA);
	memcpy(dma_buf_tx, tx, length);
	t.rx_buf = dma_buf_rx;
	t.tx_buf = dma_buf_tx;
	t.len = length;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	status = spi_sync(spi, &m);
	if (status != 0) {
		VEB_ERR("spi duplex failed(%d)!\n", status);
		kfree(dma_buf_tx);
		kfree(dma_buf_rx);
		return status;
	}

	memcpy(rx, dma_buf_rx, length);
	kfree(dma_buf_tx);
	kfree(dma_buf_rx);
	VEB_TRACE_OUT();
	return status;
}

#define CMD_RSA_KEY_GENERATE			0xD1
int veb_cmd_config(struct spi_device *spi, cmd_t *cmd)
{
	unsigned char sw[VEB_IC_STATUS_LENGTH+4] __VEB_ALIGNED__ = {0};
	int ret = 0;

	VEB_TRACE_IN();
	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		VEB_ERR("Try to wake chipset up failed(%d)!\n", ret);
		return VEB_ERR_WAKE_TIMEOUT;
	}

	veb_spi_send(spi, (unsigned char *)cmd, CMD_T_SIZE);

	VEB_DDUMP_WITH_PREFIX("command :", ((unsigned char *)cmd), CMD_T_SIZE);

#if 0
	if (cmd->cmd[1] == CMD_RSA_KEY_GENERATE) {
		ret = veb_rsa_wait_ready(200);
	} else {
		ret = veb_wait_ready();
	}
#else
	ret = veb_wait_ready();
#endif
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}
	veb_spi_recv(spi, sw, VEB_IC_STATUS_LENGTH);
	ret = check_sw(sw);
	if (ret != 0) {
		VEB_ERR("status is not correct(0x%02X%02X)!\n", sw[1], sw[0]);
		return ret;
	}

	if (cmd->cmd[1] == CMD_RSA_KEY_GENERATE) {
	   ret = veb_rsa_wait_ready(200);
	} else {
		ret = veb_wait_ready();
	}

	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}

	sw[0] = sw[1] = 0;
	if (cmd->cmd[0] == VEB_IC_CLA_CRC) {
		veb_spi_recv(spi, sw, VEB_IC_STATUS_LENGTH + 4);
	} else {
		veb_spi_recv(spi, sw, VEB_IC_STATUS_LENGTH);
	}
	ret = check_sw(sw);
	if (ret != 0) {
		VEB_ERR("status is not correct(0x%02X%02X)!\n", sw[1], sw[0]);
		return ret;
	}

	VEB_DBG("status 0x%02X%02X\n", sw[1], sw[0]);
	return VEB_OK;
}

int veb_cmd_read(struct spi_device *spi, cmd_t *cmd, char *buf, int len)
{
	unsigned char sw[VEB_IC_STATUS_LENGTH] __VEB_ALIGNED__ = {0};

	unsigned int crc32_got __VEB_ALIGNED__ = 0;
	unsigned int crc32_cal = 0;
	unsigned char *buf_tmp = NULL;
	int ret = 0;
	int tmp_len = 0, len1 = 0;
	int count = 0;
	int i = 0;

	VEB_TRACE_IN();
	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		VEB_ERR("Try to wake chipset up failed(%d)!\n", ret);
		return VEB_ERR_WAKE_TIMEOUT;
	}

	veb_spi_send(spi, (unsigned char *)cmd, CMD_T_SIZE);

	VEB_DDUMP_WITH_PREFIX("command :", ((unsigned char *)cmd), CMD_T_SIZE);
	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}
	veb_spi_recv(spi, sw, VEB_IC_STATUS_LENGTH);
	ret = check_sw(sw);
	if (ret != 0) {
		VEB_ERR("status is not correct(0x%02X%02X)!\n", sw[1], sw[0]);
		return ret;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}

	buf_tmp = kmalloc(len + 16, GFP_KERNEL);
	if (buf_tmp == NULL) {
		VEB_ERR("buf malloc fail.\n");
		return VEB_ERR_MALLOC;
	}

	len1 = (cmd->cmd[0] == VEB_IC_CLA_CRC) ? (len + 6) : (len + VEB_IC_STATUS_LENGTH);
	count = (len1 + XFER_BLOCK_SIZE - 1) / XFER_BLOCK_SIZE;
	tmp_len = XFER_BLOCK_SIZE;
	for (i = 0; i < count; i++) {
		if (((i + 1) == count) && ((len1 % XFER_BLOCK_SIZE) != 0)) {
			tmp_len = len1 % XFER_BLOCK_SIZE;
		}

		veb_spi_recv(spi, buf_tmp + i * XFER_BLOCK_SIZE, tmp_len);
		ret = veb_wait_ready();
		if (ret != 0) {
			kfree(buf_tmp);
			VEB_ERR("wait for ready time out(%d)!\n", ret);
			return VEB_ERR_WAIT_BUSY_TIMEOUT;
		}
	}

	/**
	 * check crc32 of data Received
	 */
	if (cmd->cmd[0] == VEB_IC_CLA_CRC) {
		crc32_cal = get_crc32_mpeg(buf_tmp, len + VEB_IC_STATUS_LENGTH);
		memcpy((unsigned char *)&crc32_got, buf_tmp + len + VEB_IC_STATUS_LENGTH, 4);
		if (crc32_cal != crc32_got) {
			kfree(buf_tmp);
			VEB_ERR("crc32(0x%08x:0x%08x) is not correct!\n", crc32_got, crc32_cal);
			return VEB_ERR_CRC;
		}
	}

	memcpy(buf, buf_tmp, len);
	sw[0] = sw[1] = 0;
	memcpy((unsigned char *)sw, buf_tmp + len, VEB_IC_STATUS_LENGTH);
	kfree(buf_tmp);
	ret = check_sw(sw);
	if (ret != 0) {
		VEB_ERR("status is not correct(0x%02X%02X)!\n", sw[1], sw[0]);
		return ret;
	}

	VEB_DBG("status 0x%02X%02X\n", sw[1], sw[0]);
	return VEB_OK;
}

int veb_cmd_write(struct spi_device *spi, cmd_t *cmd, unsigned char *buf, int len)
{
	unsigned char sw[VEB_IC_STATUS_LENGTH + 4] __VEB_ALIGNED__ = {0};
	unsigned char *buf_tmp;

	unsigned int crc32_cal __VEB_ALIGNED__ = 0;
	int ret = 0;
	int tmp_len = 0;
	int count = 0;
	int i = 0;

	VEB_TRACE_IN();
	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		VEB_ERR("Try to wake chipset up failed(%d)!\n", ret);
		return VEB_ERR_WAKE_TIMEOUT;
	}

	veb_spi_send(spi, (unsigned char *)cmd, CMD_T_SIZE);

	VEB_DDUMP_WITH_PREFIX("command :", ((unsigned char *)cmd), CMD_T_SIZE);

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}
	veb_spi_recv(spi, sw, VEB_IC_STATUS_LENGTH);
	ret = check_sw(sw);
	if (ret != 0) {
		VEB_ERR("status is not correct(0x%02X%02X)!\n", sw[1], sw[0]);
		return ret;
	}
	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}
	buf_tmp = kmalloc(len + 6, GFP_KERNEL);
	if (buf_tmp == NULL) {
		VEB_ERR("buf malloc fail.\n");
		return VEB_ERR_MALLOC;
	}

	memcpy(buf_tmp, buf, len);
	if (cmd->cmd[0] == VEB_IC_CLA_CRC) {
		crc32_cal = get_crc32_mpeg(buf, len);
		memcpy(buf_tmp + len, &crc32_cal, 4);
		len += 4;
	}
	tmp_len = XFER_BLOCK_SIZE;
	count = (len + XFER_BLOCK_SIZE - 1) / XFER_BLOCK_SIZE;
	for (i = 0; i < count; i++) {
		if (((i + 1) == count) && ((len % XFER_BLOCK_SIZE) != 0)) {
			tmp_len = len % XFER_BLOCK_SIZE;
		}
		veb_spi_send(spi, buf_tmp + i * XFER_BLOCK_SIZE, tmp_len);
		ret = veb_wait_ready();
		if (ret != 0) {
			VEB_ERR("wait for ready time out(%d)!\n", ret);
			kfree(buf_tmp);
			return VEB_ERR_WAIT_BUSY_TIMEOUT;
		}
	}
	kfree(buf_tmp);
	sw[0] = sw[1] = 0;
	if (cmd->cmd[0] == VEB_IC_CLA_CRC) {
		veb_spi_recv(spi, sw, VEB_IC_STATUS_LENGTH + 4);
	} else {
		veb_spi_recv(spi, sw, VEB_IC_STATUS_LENGTH);
	}
	ret = check_sw(sw);
	if (ret != 0) {
		VEB_ERR("status is not correct(0x%02X%02X)!\n", sw[1], sw[0]);
	}

	VEB_DBG("status 0x%02X%02X\n", sw[1], sw[0]);
	return ret;
}

int veb_cmd_exchange(struct spi_device *spi, cmd_t *cmd, unsigned char *source,
		unsigned int srlength, unsigned char *sink, unsigned int sklength)
{
	unsigned char sw[VEB_IC_STATUS_LENGTH] __VEB_ALIGNED__ = {0};
	unsigned int crc32_got __VEB_ALIGNED__ = 0;
	unsigned int crc32_cal __VEB_ALIGNED__ = 0;
	unsigned char *buf_tmp;
	int tmp_len = 0, ret = 0, left = 0;

	VEB_TRACE_IN();
	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		VEB_ERR("Try to wake chipset up failed(%d)!\n", ret);
		return VEB_ERR_WAKE_TIMEOUT;
	}

	veb_spi_send(spi, (unsigned char *)cmd, CMD_T_SIZE);

	VEB_DDUMP_WITH_PREFIX("command :", ((unsigned char *)cmd), CMD_T_SIZE);

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}
	veb_spi_recv(spi, sw, VEB_IC_STATUS_LENGTH);
	ret = check_sw(sw);
	if (ret != 0) {
		VEB_ERR("status is not correct(0x%02X%02X)!\n", sw[1], sw[0]);
		return ret;
	}
	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}

	buf_tmp = kmalloc((srlength > sklength ? srlength : sklength) + 4 + 2, GFP_KERNEL);
	if (buf_tmp == NULL) {
		VEB_ERR("buf malloc fail.\n");
		return VEB_ERR_MALLOC;
	}
	tmp_len = srlength;
	VEB_DDUMP_WITH_PREFIX("source :", ((unsigned char *)source), srlength);
	memcpy(buf_tmp, source, tmp_len);
	if (cmd->cmd[0] == VEB_IC_CLA_CRC) {
		crc32_cal = get_crc32_mpeg(source, srlength);
		memcpy(buf_tmp + tmp_len, (unsigned char *)&crc32_cal, sizeof(crc32_cal));
		tmp_len += 4;
		VEB_DBG("fill crc32 data 0x%x len %d\n", crc32_cal, tmp_len);
	}
	/*VEB_DDUMP_WITH_PREFIX("send buf: ", buf_tmp, tmp_len);*/
	if (tmp_len > XFER_BLOCK_SIZE) {
		/* sym max data len is 1024*/
		left = tmp_len % XFER_BLOCK_SIZE;
		VEB_DBG("len %d XFER_BLOCK_SIZE left %d\n", tmp_len / XFER_BLOCK_SIZE, left);
		tmp_len = XFER_BLOCK_SIZE;
	}
	/*VEB_DDUMP_WITH_PREFIX("buf_tmp :", ((unsigned char *)buf_tmp), tmp_len);*/
	veb_spi_send(spi, buf_tmp, tmp_len);
	if (left) {
		VEB_DBG("send left data xfer %d\n", left);
		veb_spi_send(spi, buf_tmp + tmp_len, left);
	}
	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		kfree(buf_tmp);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}
	if (cmd->cmd[0] == VEB_IC_CLA_CRC) {
		tmp_len = sklength + VEB_IC_STATUS_LENGTH + 4;
	} else {
		tmp_len = sklength + VEB_IC_STATUS_LENGTH;
	}
	left = 0;
	memset(buf_tmp, 0, tmp_len);
	if (tmp_len > XFER_BLOCK_SIZE) {
		/* sym max data len is 1024*/
		left = tmp_len % XFER_BLOCK_SIZE;
		VEB_DBG("len %d XFER_BLOCK_SIZE left %d\n", tmp_len / XFER_BLOCK_SIZE, left);
		tmp_len = XFER_BLOCK_SIZE;
	}
	veb_spi_recv(spi, buf_tmp, tmp_len);

	if (left) {
		VEB_DBG("recv left data xfer %d\n", left);
		veb_spi_recv(spi, buf_tmp + tmp_len, left);
	}
	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait for ready time out(%d)!\n", ret);
		kfree(buf_tmp);
		return VEB_ERR_WAIT_BUSY_TIMEOUT;
	}
	/*VEB_DDUMP_WITH_PREFIX("recv buf: ", buf_tmp, tmp_len + left);*/
	if (cmd->cmd[0] == VEB_IC_CLA_CRC) {
		crc32_cal = get_crc32_mpeg(buf_tmp, sklength + VEB_IC_STATUS_LENGTH);
		VEB_DBG("crc32_cal 22 0x%x\n", crc32_cal);
		memcpy((unsigned char *)&crc32_got, buf_tmp + sklength + VEB_IC_STATUS_LENGTH, sizeof(crc32_got));
		if (crc32_cal != crc32_got) {
			VEB_ERR("crc32(0x%08x:0x%08x) is not correct!\n", crc32_got, crc32_cal);
			kfree(buf_tmp);
			return VEB_ERR_CRC;
		}
	}
	memcpy(sink, buf_tmp, sklength);
	sw[0] = sw[1] = 0;
	memcpy(sw, buf_tmp + sklength, VEB_IC_STATUS_LENGTH);
	kfree(buf_tmp);
	ret = check_sw(sw);
	if (ret != 0) {
		VEB_ERR("status is not correct(0x%02X%02X)!\n", sw[1], sw[0]);
	}

	VEB_TRACE_OUT();
	return ret;
}

int veb_sym_crypto(struct spi_device *spi, cmd_t *cmd, unsigned char *buf, unsigned char *IV, int len)
{
	int ret, branch, left = 0, tmp_len = 0;
	unsigned char sw[VEB_IC_STATUS_LENGTH] __VEB_ALIGNED__;
	unsigned int crc32_got __VEB_ALIGNED__ = 0;
	unsigned int crc32_cal = 0;
	unsigned char *buf_tmp = NULL;
	struct spi_transfer xfer, xfer_left;
	struct spi_message p;

	VEB_TRACE_IN();
	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		branch = 1;
		goto func_error;
	}
	veb_spi_send(spi, (unsigned char *)cmd, CMD_T_SIZE);
	VEB_DDUMP_WITH_PREFIX("command :", ((unsigned char *)cmd), CMD_T_SIZE);

	ret = veb_wait_ready();
	if (ret != 0) {
		branch = 2;
		goto func_error;
	}

	veb_spi_recv(spi, sw, VEB_IC_STATUS_LENGTH);
	ret = check_sw(sw);
	if (ret != 0) {
		branch = 3;
		goto func_error;
	}
	ret = veb_wait_ready();
	if (ret != 0) {
		branch = 4;
		goto func_error;
	}
	buf_tmp = kmalloc(len + 16 + 4 + 2, GFP_KERNEL|GFP_DMA);
	if (buf_tmp == NULL) {
		VEB_ERR("buf malloc fail.\n");
		return VEB_ERR_MALLOC;
	}

	/*VEB_DDUMP_WITH_PREFIX("data buf :", (buf), len);*/
	tmp_len = 0;    /* begin fill  buf_tmp = counter IV, + Buf + crc32*/
	if (IV != NULL) {
		memcpy(buf_tmp, IV, 16);
		tmp_len += 16;
	}
	memcpy(buf_tmp + tmp_len, buf, len);
	tmp_len += len;
	if (cmd->cmd[0] == VEB_IC_CLA_CRC) {
		crc32_cal = get_crc32_mpeg(buf_tmp, tmp_len);
		memcpy(buf_tmp + tmp_len, (unsigned char *)&crc32_cal, sizeof(crc32_cal));
		tmp_len += 4;
	}

	if (tmp_len > XFER_BLOCK_SIZE) {
		/* sym max data len is 1024*/
		left = tmp_len % XFER_BLOCK_SIZE;
		tmp_len = XFER_BLOCK_SIZE;
	}

	spi_message_init(&p);  /* send data*/
	INIT_XFER_STRUCT(xfer, buf_tmp, NULL, tmp_len);
	spi_message_add_tail(&xfer, &p);

	if (left) {
		VEB_DBG("send left data xfer %d.\n", left);
		INIT_XFER_STRUCT(xfer_left, buf_tmp + tmp_len, NULL, left);
		spi_message_add_tail(&xfer_left, &p);
	}
	spi_sync(spi, &p);
	ret = veb_wait_ready();
	if (ret != 0) {
		branch = 5;
		goto func_error;
	}

	tmp_len = len + VEB_IC_STATUS_LENGTH;
	left = 0;
	if (cmd->cmd[0] == VEB_IC_CLA_CRC)
		tmp_len += 4;
	if (tmp_len > XFER_BLOCK_SIZE) {
		/* sym max data len is 1024*/
		left = tmp_len % XFER_BLOCK_SIZE;
		tmp_len = XFER_BLOCK_SIZE;
	}
	VEB_DBG("tmp_len is %d.\n", tmp_len);
	spi_message_init(&p);  /*recv data, need cal the len again*/
	INIT_XFER_STRUCT(xfer, buf_tmp, buf_tmp, tmp_len); /*need set tx buf*/
	spi_message_add_tail(&xfer, &p);
	if (left) {
		VEB_DBG("recv left data xfer %d.\n", left);
		INIT_XFER_STRUCT(xfer_left, buf_tmp + tmp_len, buf_tmp + tmp_len, left);
		spi_message_add_tail(&xfer_left, &p);
	}
	spi_sync(spi, &p);
	ret = veb_wait_ready();
	if (ret != 0) {
		branch = 6;
		goto func_error;
	}

	if (cmd->cmd[0] == VEB_IC_CLA_CRC) {
		memcpy((unsigned char *)&crc32_got, buf_tmp + len + 2, sizeof(crc32_got));
		crc32_cal = get_crc32_mpeg(buf_tmp, len + 2);
		if (crc32_cal != crc32_got) {
			VEB_ERR("crc32(0x%08x:0x%08x) is not correct!\n", crc32_got, crc32_cal);
			kfree(buf_tmp);
			return VEB_ERR_CRC;
		}
	}
	/*VEB_DDUMP_WITH_PREFIX("recv buf_tmp :", (buf_tmp), tmp_len);*/

	sw[0] = sw[1] = 0;
	memcpy(sw, buf_tmp + len, 2);
	memcpy(buf, buf_tmp, len); /* copy recv data*/
	kfree(buf_tmp);
	return check_sw(sw);

func_error:
	kfree(buf_tmp);
	VEB_ERR("len: %d, branch %d.\n", len, branch);
	return ret;
}

int veb_sym_duplex_crypto(struct spi_device *spi, cmd_t *cmd, unsigned char *buf, unsigned char *IV, int len)
{
	int ret = 0, i = 0, count = 0, len1 = 0, tmp_len = 0, branch = 0;
	unsigned char sw[2] __aligned(4);
	unsigned char *tmp;
	unsigned char *tmp_xfer = NULL;
	unsigned int crc32_got __VEB_ALIGNED__ = 0;
	unsigned int crc32_cal = 0;
	struct spi_transfer xfer, xfer_left;
	struct spi_message p;

	VEB_TRACE_IN();
	count = (len + XFER_BLOCK_SIZE - 1) / XFER_BLOCK_SIZE + 3;
	tmp_xfer = kmalloc((XFER_BLOCK_SIZE + 4) * count, GFP_KERNEL|GFP_DMA);
	if (tmp_xfer == NULL) {
		VEB_ERR("kmalloc %d failed!\n", XFER_BLOCK_SIZE);
		return -ENOMEM;
	}

	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		branch = 1;
		goto func_error;
	}

	veb_spi_send(spi, (unsigned char *)cmd, CMD_T_SIZE);
	ret = veb_wait_ready();
	if (ret != 0) {
		branch = 2;
		goto func_error;
	}

	veb_spi_recv(spi, sw, VEB_IC_STATUS_LENGTH);
	ret = check_sw(sw);
	if (ret != 0) {
		branch = 3;
		goto func_error;
	}
	ret = veb_wait_ready();
	if (ret != 0) {
		branch = 4;
		goto func_error;
	}

	if (IV != NULL) {
		memcpy(tmp_xfer, IV, 16);
		tmp_len = 16;
		if (cmd->cmd[0] == VEB_IC_CLA_CRC) {
			crc32_cal = get_crc32_mpeg(IV, 16);
			memcpy(tmp_xfer + tmp_len, (unsigned char *)&crc32_cal, sizeof(crc32_cal));
			tmp_len += 4;
		}
		veb_spi_send(spi, tmp_xfer, tmp_len);
		ret = veb_wait_ready();
		if (ret != 0) {
			branch = 5;
			goto func_error;
		}
	}

	if (cmd->cmd[0] == VEB_IC_CLA_CRC) {
		count = (len) / XFER_BLOCK_SIZE;
		tmp = tmp_xfer;
		for (i = 0; i < count; i++) {
			memcpy(tmp, buf+i * XFER_BLOCK_SIZE, XFER_BLOCK_SIZE);
			crc32_cal = get_crc32_mpeg(tmp, XFER_BLOCK_SIZE);
			tmp += XFER_BLOCK_SIZE;
			memcpy(tmp, (unsigned char *)&crc32_cal, 4);
			tmp += 4;
		}
		tmp_len = len % XFER_BLOCK_SIZE;
		if (tmp_len != 0) {
			/*VEB_DBG("crc data is left %d i %d\n", tmp_len, i);*/
			memcpy(tmp, buf + i * XFER_BLOCK_SIZE, tmp_len);
			memset(tmp + tmp_len, '5', XFER_BLOCK_SIZE - tmp_len);
			crc32_cal = get_crc32_mpeg(tmp, XFER_BLOCK_SIZE);
			tmp += XFER_BLOCK_SIZE;
			memcpy(tmp, (unsigned char *)&crc32_cal, 4);
			tmp += 4;
		}
		for (i = 0; i < 2; i++) {
			memset(tmp, '5', XFER_BLOCK_SIZE);
			if (tmp_len != 0 && i == 1) {
				crc32_cal = get_crc32_mpeg(tmp, tmp_len);
				tmp += tmp_len;
			} else {
				crc32_cal = get_crc32_mpeg(tmp, XFER_BLOCK_SIZE);
				tmp += XFER_BLOCK_SIZE;
			}
			memcpy(tmp, (unsigned char *)&crc32_cal, 4);
			tmp += 4;
		}
		len1 = tmp - tmp_xfer;
	}

	spi_message_init(&p);  /* send data*/
	if (cmd->cmd[0] == VEB_IC_CLA_CRC) {
		tmp_len = len1 - len1 % XFER_BLOCK_SIZE;
		INIT_XFER_STRUCT(xfer, tmp_xfer, buf, tmp_len);
		spi_message_add_tail(&xfer, &p);
		/*veb_spi_send_duplex(spidev, tmp_xfer, buf, tmp_len);*/
		if (len1 - tmp_len) {
		   /*veb_spi_send_duplex(spidev, tmp_xfer + tmp_len, buf + tmp_len, len1 % XFER_BLOCK_SIZE);*/
		   INIT_XFER_STRUCT(xfer_left, tmp_xfer + tmp_len, buf + tmp_len, len1 - tmp_len);
		   spi_message_add_tail(&xfer_left, &p);
		}
	} else {
		memset(buf + len, '5', XFER_BLOCK_SIZE * 2);
		tmp_len = (len + XFER_BLOCK_SIZE * 2) - len % XFER_BLOCK_SIZE;
		INIT_XFER_STRUCT(xfer, buf, tmp_xfer, tmp_len);
		spi_message_add_tail(&xfer, &p);
		if (len % XFER_BLOCK_SIZE != 0) {
			INIT_XFER_STRUCT(xfer_left, buf + tmp_len, tmp_xfer + tmp_len, len % XFER_BLOCK_SIZE);
			spi_message_add_tail(&xfer_left, &p);
			/*veb_spi_send_duplex(spidev, buf + tmp_len, tmp_xfer + tmp_len, len % XFER_BLOCK_SIZE);*/
		}
	}
	spi_sync(spi, &p);
	ret = veb_wait_ready();
	if (ret != 0) {
		branch = 5;
		goto func_error;
	}

	if (cmd->cmd[0] == VEB_IC_CLA_CRC) {
		tmp = buf;
		for (i = 0; i < 2; i++) {
			crc32_cal = get_crc32_mpeg(tmp, XFER_BLOCK_SIZE);
			tmp += XFER_BLOCK_SIZE;
			memcpy((unsigned char *)&crc32_got, tmp, 4);
			tmp += 4;
			/*VEB_DBG("dummp crc32_cal 0x%x crc32_got 0x%x\n", crc32_cal, crc32_got);*/
		}
		count = len / XFER_BLOCK_SIZE;
		for (i = 0; i < count; i++) {
			crc32_cal = get_crc32_mpeg(tmp, XFER_BLOCK_SIZE);
			memcpy(tmp_xfer + i * XFER_BLOCK_SIZE, tmp, XFER_BLOCK_SIZE);
			tmp += XFER_BLOCK_SIZE;
			memcpy((unsigned char *)&crc32_got, tmp, 4);
			tmp += 4;
			if (crc32_cal != crc32_got) {
				VEB_ERR("data crc32_cal 0x%x crc32_got 0x%x\n", crc32_cal, crc32_got);
				branch = 8;
				goto func_error;
			}
		}
		tmp_len = len % XFER_BLOCK_SIZE;
		if (tmp_len != 0) {
			crc32_cal = get_crc32_mpeg(tmp, tmp_len);
			memcpy(tmp_xfer + i * XFER_BLOCK_SIZE, tmp, tmp_len);
			tmp += tmp_len;
			memcpy((unsigned char *)&crc32_got, tmp, 4);
			tmp += 4;
			if (crc32_cal != crc32_got) {
				VEB_ERR("data crc32_cal 0x%x crc32_got 0x%x\n", crc32_cal, crc32_got);
				branch = 9;
				goto func_error;
			}
		}
		memcpy(buf, tmp_xfer, len);
	} else {
		tmp_len = len - len % XFER_BLOCK_SIZE;
		/*VEB_DBG("recv tmp_len %d, len %d\n", tmp_len, len);*/
		memcpy(buf, tmp_xfer + 2 * XFER_BLOCK_SIZE, tmp_len);
		if (len % XFER_BLOCK_SIZE != 0) {
			VEB_DBG("recv left %d\n", len % XFER_BLOCK_SIZE);
			memcpy(buf + tmp_len, tmp_xfer + 2 * XFER_BLOCK_SIZE + tmp_len, len % XFER_BLOCK_SIZE);
		}
	}

	sw[0] = sw[1] = 0;
	veb_spi_recv(spi, sw, VEB_IC_STATUS_LENGTH);
	ret = check_sw(sw);
	kfree(tmp_xfer);
	return ret;

func_error:
	kfree(tmp_xfer);
	VEB_ERR("len: %d, branch %d\n", len, branch);
	return ret;
}

int veb_sym_withkey_cmd_write(struct spi_device *spi, cmd_t *cmd, unsigned char *buf,
								veb_key_info *veb_key, unsigned char *IV, int len)
{
	int ret, branch;
	unsigned char sw[VEB_IC_STATUS_LENGTH] __VEB_ALIGNED__;
	unsigned int crc32_got __VEB_ALIGNED__ = 0;
	unsigned int crc32_cal = 0;
	unsigned char *buf_tmp = NULL;
	int tmp_len = 0, left = 0;
	struct spi_transfer xfer, xfer_left;
	struct spi_message m;

	VEB_TRACE_IN();
	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		branch = 1;
		goto func_error;
	}
	veb_spi_send(spi, (unsigned char *)cmd, CMD_T_SIZE);
	/*VEB_DDUMP_WITH_PREFIX("cmd", (unsigned char *)cmd, CMD_T_SIZE);*/
	ret = veb_wait_ready();
	if (ret != 0) {
		branch = 2;
		goto func_error;
	}
	veb_spi_recv(spi, sw, VEB_IC_STATUS_LENGTH);
	ret = check_sw(sw);
	if (ret != 0) {
		branch = 3;
		goto func_error;
	}
	ret = veb_wait_ready();
	if (ret != 0) {
		branch = 4;
		goto func_error;
	}

	buf_tmp = kmalloc(len + AES_256_KEY_LEN + 16 + 4 + 2, GFP_KERNEL|GFP_DMA);
	if (buf_tmp == NULL) {
		VEB_ERR("buf malloc fail.\n");
		return VEB_ERR_MALLOC;
	}

	/* fill send data = key, IV, data + crc32*/
	memcpy(buf_tmp, veb_key->buf, veb_key->len);
	tmp_len = veb_key->len;
	if (IV != NULL) {
		memcpy(buf_tmp + veb_key->len, IV, 16);
		tmp_len += 16;
	}
	memcpy(buf_tmp + tmp_len, buf, len);
	tmp_len += len;
	if (cmd->cmd[0] == VEB_IC_CLA_CRC) {
		crc32_cal = get_crc32_mpeg(buf_tmp, tmp_len);
		memcpy(buf_tmp + tmp_len, (unsigned char *)&crc32_cal, sizeof(crc32_cal));
		tmp_len += 4;
	}

	if (tmp_len > XFER_BLOCK_SIZE) {
		left = tmp_len % XFER_BLOCK_SIZE;
		tmp_len = XFER_BLOCK_SIZE;
	}
	/*VEB_DBG("data len %d tmp_len %d\n", len, tmp_len);*/
	spi_message_init(&m);  /* send data key, IV, data + crc32*/
	INIT_XFER_STRUCT(xfer, buf_tmp, NULL, tmp_len);
	spi_message_add_tail(&xfer, &m);
	if (left) {
		VEB_DBG("send left data xfer %d.\n", left);
		INIT_XFER_STRUCT(xfer_left, buf_tmp + tmp_len, NULL, left);
		spi_message_add_tail(&xfer_left, &m);
	}
	spi_sync(spi, &m);
	ret = veb_wait_ready();
	if (ret != 0) {
		branch = 5;
		goto func_error;
	}

	/*VEB_DBG("before recv data len %d tmp_len %d\n", len, tmp_len);*/
	tmp_len = len + VEB_IC_STATUS_LENGTH;
	left = 0;
	if (cmd->cmd[0] == VEB_IC_CLA_CRC)
		tmp_len += 4;
	if (tmp_len > XFER_BLOCK_SIZE) {
		left = tmp_len % XFER_BLOCK_SIZE;
		tmp_len = XFER_BLOCK_SIZE;
	}
	VEB_DBG("recv data sw + crc32 tmp_len %d\n", tmp_len);

	spi_message_init(&m);  /* recv data + sw + crc32*/
	INIT_XFER_STRUCT(xfer, buf_tmp, buf_tmp, tmp_len);
	spi_message_add_tail(&xfer, &m);
	if (left) {
		VEB_DBG("recv left data xfer %d\n", left);
		INIT_XFER_STRUCT(xfer_left, buf_tmp + tmp_len, buf_tmp + tmp_len, left);
		spi_message_add_tail(&xfer_left, &m);
	}
	spi_sync(spi, &m);
	ret = veb_wait_ready();
	if (ret != 0) {
		branch = 6;
		goto func_error;
	}

	memcpy(buf, buf_tmp, len);
	if (cmd->cmd[0] == VEB_IC_CLA_CRC) {
		memcpy((unsigned char *)&crc32_got, buf_tmp + len + 2, sizeof(crc32_got));
		crc32_cal = get_crc32_mpeg(buf_tmp, len + VEB_IC_STATUS_LENGTH);
		if (crc32_cal != crc32_got) {
			kfree(buf_tmp);
			VEB_ERR("crc32(0x%08x:0x%08x) is not correct!\n", crc32_got, crc32_cal);
			return VEB_ERR_CRC;
		}
	}
	sw[0] = sw[1] = 0;
	memcpy(sw, buf_tmp + len, VEB_IC_STATUS_LENGTH);
	kfree(buf_tmp);
	return check_sw(sw);

func_error:
	kfree(buf_tmp);
	VEB_ERR("len: %d, branch %d.\n", len, branch);
	return ret;
}
