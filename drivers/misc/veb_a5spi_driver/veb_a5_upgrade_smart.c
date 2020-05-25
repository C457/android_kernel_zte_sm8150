#include <linux/delay.h>
#include <linux/string.h>

#include "veb_a5.h"
#include "veb_base.h"
#include "veb_platform.h"
#include "veb_errno.h"
#include "veb_common.h"
#include "veb_crc.h"
#include "veb_sha.h"

extern int veb_smartcard_cmd_read(struct spi_device *spi, smart_cmd_t *cmd, char *buf, unsigned int *outlen);
static int veb_a5_version(struct veb_private_data *priv, char *version);


#define VEB_A5_CMD_VERSION			0x82

#define VEB_A5_CMD_SET_BASE			0x96
#define VEB_A5_CMD_ERASE			0xe1
#define VEB_A5_CMD_PROGRAM			0xe3				/*BLOCK*/
#define VEB_A5_CMD_DONE				0xe4
#define VEB_A5_CMD_CMODE			0x70
#define VEB_A5_CMD_BMODE			0x7F

#define VEB_A5_CMD_COS_SHA1_GET		0x85
#define VEB_A5_CMD_CONFIG			0x70

#define VEB_A5_BASE_ADDR			0x00400000

#define VEB_A5_CLA_CRC				0x81
#define VEB_A5_CLA_NOCRC			0x80

#define VEB_VERSION_LENGTH			64

#define CMD_HEAD_APDU				0x34A60253

#define VEB_TOBOOT_MODE			 0
#define VEB_TOROM_MODE			 1

#define VEB_SAFE_COS			 0
#define VEB_FINANCE_COS			 1
#define VEB_FINANCE_LIB			 2

#define CONFIG_MPU				 1
#define VEB_MPU_ON					0x9e
#define VEB_MPU_OFF					0x8d


#define CMD_T_INIT_APDU(CLA, INS_CMD, P1, P2, DATA_LEN) \
do {\
	memset(&cmd_v, 0, sizeof(cmd_v)); \
	cmd_v.head = CMD_HEAD_APDU; \
	cmd_v.len = DATA_LEN; \
	cmd_v.cmd[0] = CLA; \
	cmd_v.cmd[1] = INS_CMD; \
	cmd_v.cmd[2] = P1; \
	cmd_v.cmd[3] = P2; \
	cmd_v.crc8[0] = 0x55; \
	cmd_v.crc8[1] = 0x55; \
	cmd_v.crc8[2] = 0x55; \
	cmd_v.crc8[3] = get_crc8_0xff((unsigned char *)&cmd_v, CHIP_CMD_CRC_LEN); \
} while (0)


#define CMD_T_INIT(CLA, INS_CMD, P1, P2, DATA_LEN) \
do {\
	memset(&cmd_v, 0, sizeof(cmd_v)); \
	cmd_v.head = CMD_HEAD; \
	cmd_v.len = DATA_LEN; \
	cmd_v.cmd[0] = CLA; \
	cmd_v.cmd[1] = INS_CMD; \
	cmd_v.cmd[2] = P1; \
	cmd_v.cmd[3] = P2; \
	cmd_v.crc8[0] = 0x55; \
	cmd_v.crc8[1] = 0x55; \
	cmd_v.crc8[2] = 0x55; \
	cmd_v.crc8[3] = get_crc8_0xff((unsigned char *)&cmd_v, CHIP_CMD_CRC_LEN); \
} while (0)

#if CONFIG_MPU
static int veb_a5_set_mpu_protect(struct veb_private_data *priv, int cfg)
{
	char p1 = 0;
	char p2 = 0;
	char sw[2] = {0};
	char version[VEB_VERSION_LENGTH] = {0};
	struct cmd_t cmd_v __VEB_ALIGNED__;
	int ret = VEB_OK;

	VEB_TRACE_IN();
	ret = veb_a5_version(priv, version);
	if (ret != 0) {
		VEB_ERR("get version failed(%d)!\n", ret);
		return ret;
	}
	VEB_DBG("version : %s\n", version);
	if (strncmp("A5 SPI_BOOT V0.8", version, 16) > 0) {
		VEB_DBG("not have MPU!\n");
		return VEB_OK;
	}
	/* p1 <===> addr[24 ~ 31]
	 p2 <===> addr[16 ~ 23]*/
	p1 = (char)0x04;
	if (!cfg) {
		p2 = VEB_MPU_OFF;
	} else {
		p2 = VEB_MPU_ON;
	}

	CMD_T_INIT(VEB_A5_CLA_NOCRC, VEB_A5_CMD_BMODE, p1, p2, 0);

	VEB_DDUMP_WITH_PREFIX("cmd :", (char *)&cmd_v, CMD_T_SIZE);
	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		VEB_ERR("Try to wake chipset up failed(%d)!\n", ret);
		return VEB_ERR_WAKE_TIMEOUT;
	}
	ret = veb_spi_send(priv->spi, ((char *)&cmd_v), CMD_T_SIZE);
	if (ret != 0) {
		VEB_ERR("spi send failed(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}

	VEB_TRACE_OUT();
	return VEB_OK;
}
#endif

static int veb_a5_set_base_addr(struct veb_private_data *priv, int base)
{
	char p1 = 0;
	char p2 = 0;

	char sw[2] = {0};

	struct cmd_t cmd_v __VEB_ALIGNED__;

	int ret = VEB_OK;

	VEB_TRACE_IN();

	/*p1 <===> addr[24 ~ 31]
	 p2 <===> addr[16 ~ 23]*/
	p1 = (char)((base >> 24) & 0xff);
	p2 = (char)((base >> 16) & 0xff);

	CMD_T_INIT(VEB_A5_CLA_NOCRC, VEB_A5_CMD_SET_BASE, p1, p2, 0);

	VEB_DDUMP_WITH_PREFIX("cmd :", (char *)&cmd_v, CMD_T_SIZE);
	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		VEB_ERR("Try to wake chipset up failed(%d)!\n", ret);
		return VEB_ERR_WAKE_TIMEOUT;
	}
	ret = veb_spi_send(priv->spi, ((char *)&cmd_v), CMD_T_SIZE);
	if (ret != 0) {
		VEB_ERR("spi send failed(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}

	VEB_TRACE_OUT();
	return VEB_OK;
}

#define VEB_A5_COS_MAX          0x320000             /*128K*/
static int veb_a5_erase(struct veb_private_data *priv, int addr, int length)
{
	char p1 = 0;
	char p2 = 0;
	char sw[2] = {0};

	struct cmd_t cmd_v __VEB_ALIGNED__;

	int ret = VEB_OK;

	VEB_TRACE_IN();

	ret = veb_a5_set_base_addr(priv, (addr & 0xffff0000));
	if (ret != 0) {
		VEB_ERR("set base addr(0x%08x) failed(%d)!", (addr & 0xffff0000), ret);
		return ret;
	}

	/*p1 <===> addr[8 ~ 15]
	 p2 <===> addr[0 ~ 7]*/
	p1 = (char)((addr >> 8) & 0xff);
	p2 = (char)((addr >> 0) & 0xff);
	CMD_T_INIT(VEB_A5_CLA_NOCRC, VEB_A5_CMD_ERASE, p1, p2, length);

	VEB_DDUMP_WITH_PREFIX("cmd :", (char *)&cmd_v, CMD_T_SIZE);
	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		VEB_ERR("Try to wake chipset up failed(%d)!\n", ret);
		return VEB_ERR_WAKE_TIMEOUT;
	}
	ret = veb_spi_send(priv->spi, ((char *)&cmd_v), CMD_T_SIZE);
	if (ret != 0) {
		VEB_ERR("spi send failed(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("cmd status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(%d)!\n", ret);
		return ret;
	}
	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("operate status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}

	VEB_TRACE_OUT();
	return VEB_OK;
}

#define VEB_A5_BLOCK_SIZE			2048
static int veb_a5_program(struct veb_private_data *priv, int addr, char *buf, int length)
{
	char p1 = 0;
	char p2 = 0;
	char sw[2] = {0};

	struct cmd_t cmd_v __VEB_ALIGNED__;

	int ret = VEB_OK;

	VEB_TRACE_IN();

	ret = veb_a5_set_base_addr(priv, (addr & 0xffff0000));
	if (ret != 0) {
		VEB_ERR("set base addr(0x%08x) failed(%d)!", (addr & 0xffff0000), ret);
		return ret;
	}

	/*p1 <===> addr[8 ~ 15]
	 p2 <===> addr[0 ~ 7]*/
	p1 = (char)((addr >> 8) & 0xff);
	p2 = (char)((addr >> 0) & 0xff);

	CMD_T_INIT(VEB_A5_CLA_NOCRC, VEB_A5_CMD_PROGRAM, p1, p2, length);

	VEB_DDUMP_WITH_PREFIX("cmd :", (char *)&cmd_v, CMD_T_SIZE);
	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		VEB_ERR("Try to wake chipset up failed(%d)!\n", ret);
		return VEB_ERR_WAKE_TIMEOUT;
	}
	ret = veb_spi_send(priv->spi, ((char *)&cmd_v), CMD_T_SIZE);
	if (ret != 0) {
		VEB_ERR("spi send failed(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_send(priv->spi, buf, length);
	if (ret != 0) {
		VEB_ERR("wait ready failed!(%d)!\n", ret);
		return ret;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}

	VEB_TRACE_OUT();
	return VEB_OK;
}

static int veb_a5_cos_sha1_get(struct veb_private_data *priv, int addr, int cLength, char *sha1)
{
	char sw[2] = {0};
	char p1 = 0;
	char p2 = 0;

#ifndef CONFIG_ARCH_MT6572
	char s[22] = {0};
#endif

	struct cmd_t cmd_v __VEB_ALIGNED__;
	int ret = VEB_OK;

	VEB_TRACE_IN();

	ret = veb_a5_set_base_addr(priv, (addr & 0xffff0000));
	if (ret != 0) {
		VEB_ERR("set base addr(0x%08x) failed(%d)!", (addr & 0xffff0000), ret);
		return ret;
	}

	p1 = (addr & 0x0000ff00) >> 8;
	p2 = addr & 0x000000ff;
	CMD_T_INIT(VEB_A5_CLA_NOCRC, VEB_A5_CMD_COS_SHA1_GET, p1, p2, cLength);

	VEB_DDUMP_WITH_PREFIX("cmd :", (char *)&cmd_v, CMD_T_SIZE);
	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		VEB_ERR("Try to wake chipset up failed(%d)!\n", ret);
		return VEB_ERR_WAKE_TIMEOUT;
	}
	ret = veb_spi_send(priv->spi, ((char *)&cmd_v), CMD_T_SIZE);
	if (ret != 0) {
		VEB_ERR("spi send failed(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(%d)!\n", ret);
		return ret;
	}

#ifndef CONFIG_ARCH_MT6572
	ret = veb_spi_recv(priv->spi, s, 22);
	if (ret != 0) {
		VEB_ERR("wait ready failed!(%d)!\n", ret);
		return ret;
	}

	VEB_DDUMP_WITH_PREFIX("sha1 :", s, 22);

	if ((s[21] != 0x90) || (s[20] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", s[21], s[20]);
		ret =  VEB_ERR_STATUS;
	} else {
		memcpy(sha1, s, 20);
	}
#else
	ret = veb_spi_recv(priv->spi, sha1, 20);
	if (ret != 0) {
		VEB_ERR("wait ready failed!(%d)!\n", ret);
		return ret;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}

	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}
#endif

	VEB_TRACE_OUT();
	return ret;
}

static int veb_a5_done(struct veb_private_data *priv, int addr, int cLength)
{
	char p1 = 0;
	char p2 = 0;
	char sw[2] = {0};

	struct cmd_t cmd_v __VEB_ALIGNED__;

	int ret = VEB_OK;

	VEB_TRACE_IN();

	ret = veb_a5_set_base_addr(priv, (addr & 0xffff0000));
	if (ret != 0) {
		VEB_ERR("set base addr(0x%08x) failed(%d)!", (addr & 0xffff0000), ret);
		return ret;
	}

	/*p1 <===> addr[8 ~ 15]
	 p2 <===> addr[0 ~ 7]*/
	p1 = (char)((addr >> 8) & 0xff);
	p2 = (char)((addr >> 0) & 0xff);
	CMD_T_INIT(VEB_A5_CLA_NOCRC, VEB_A5_CMD_DONE, p1, p2, cLength);

	VEB_DDUMP_WITH_PREFIX("cmd :", (char *)&cmd_v, CMD_T_SIZE);
	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		VEB_ERR("Try to wake chipset up failed(%d)!\n", ret);
		return VEB_ERR_WAKE_TIMEOUT;
	}
	ret = veb_spi_send(priv->spi, ((char *)&cmd_v), CMD_T_SIZE);
	if (ret != 0) {
		VEB_ERR("spi send failed(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}

	VEB_TRACE_OUT();
	return VEB_OK;
}

int veb_a5_disboot(struct veb_private_data *priv)
{
	char sw[2] = {0};

	struct cmd_t cmd_v __VEB_ALIGNED__;

	int ret = VEB_OK;

	VEB_TRACE_IN();

	CMD_T_INIT(VEB_A5_CLA_NOCRC, VEB_A5_CMD_BMODE, 0x01, 0x5a, (0x7265746f));

	VEB_DDUMP_WITH_PREFIX("cmd :", (char *)&cmd_v, CMD_T_SIZE);
	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		VEB_ERR("Try to wake chipset up failed(%d)!\n", ret);
		return VEB_ERR_WAKE_TIMEOUT;
	}
	ret = veb_spi_send(priv->spi, ((char *)&cmd_v), CMD_T_SIZE);
	if (ret != 0) {
		VEB_ERR("spi send failed(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}

	veb_spi_reset();
	VEB_TRACE_OUT();
	return VEB_OK;
}

#if 0
static int veb_a5_restore_secboot(struct veb_private_data *priv)
{
	char sw[2] = {0};

	struct cmd_t cmd_v __VEB_ALIGNED__;

	int ret = VEB_OK;

	VEB_TRACE_IN();

	CMD_T_INIT(VEB_A5_CLA_NOCRC, VEB_A5_CMD_CMODE, 0x01, 0x5a, (0x5245544f));

	VEB_DDUMP_WITH_PREFIX("cmd :", (char *)&cmd_v, CMD_T_SIZE);

	ret = veb_spi_send(priv->spi, ((char *)&cmd_v), CMD_T_SIZE);
	if (ret != 0) {
		VEB_ERR("spi send failed(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}

	veb_spi_reset();
	VEB_TRACE_OUT();
	return VEB_OK;
}
#endif
#if 0
static int veb_a5_restore_secboot(struct veb_private_data *priv)
{
	char sw[4] = {0};

	smart_cmd_t cmd_v __VEB_ALIGNED__;

	int ret = VEB_OK;
	unsigned int outlen = 0;

	VEB_TRACE_IN();

	CMD_T_INIT_APDU(0xf0, 0xd0, 0x00, 0x00, 0x00);

	VEB_DDUMP_WITH_PREFIX("cmd :", (char *)&cmd_v, CMD_T_SIZE);
	ret = veb_smartcard_cmd_read(priv->spi, &cmd_v, sw, &outlen);
	if (ret != 0) {
		VEB_ERR("veb_a5_restore_secboot failed(ret:%d)!\n", ret);
		return ret;
	}
	veb_spi_reset();
	VEB_TRACE_OUT();
	return VEB_OK;
}
#endif

int veb_a5_restore_mode(struct veb_private_data *priv, int mode)
{
	char outdata[512] = {0};
	char sw[4] = {0};
	struct cmd_t cmd_v __VEB_ALIGNED__;

	int ret = VEB_OK;
	unsigned int outlen = 0;

	VEB_TRACE_IN();
	if (mode == VEB_TOBOOT_MODE) {
		CMD_T_INIT_APDU(0xf0, 0x18, 0x00, 0x00, 0x0c);	/*get version for smartcard*/
		VEB_DDUMP_WITH_PREFIX("cmd :", (char *)&cmd_v, CMD_T_SIZE);
		ret = veb_smartcard_cmd_read(priv->spi, &cmd_v, outdata, &outlen);
		memcpy((unsigned char *)sw, outdata + outlen - 2, 2);
		if (ret != 0 || sw[0] != 0x90 || sw[1] != 0x00) {
			/*veb_spi_reset();
			msleep(350);*/
			VEB_ERR("native cos mode (ret:%d) sw:0x%02x %02x!\n", ret, sw[0], sw[1]);
			CMD_T_INIT(VEB_A5_CLA_NOCRC, VEB_A5_CMD_CMODE, 0x01, 0x5a, (0x5245544f));
		} else {
			CMD_T_INIT_APDU(0xf0, 0xd0, 0x00, 0x00, 0x00);
			VEB_DDUMP_WITH_PREFIX("cmd :", (char *)&cmd_v, CMD_T_SIZE);
			memset(sw, 0, 4);
			ret = veb_smartcard_cmd_read(priv->spi, &cmd_v, sw, &outlen);
			if (ret != 0 || sw[0] != 0x90 || sw[1] != 0x00) {
				VEB_ERR("veb_a5_restore_secboot failed(ret:%d)!\n", ret);
				return ret;
			}
			goto fun_out;
		}
	} else if (mode == VEB_TOROM_MODE) {
#if CONFIG_MPU
		ret = veb_a5_set_mpu_protect(priv, 0);
		if (ret != VEB_OK) {
			VEB_ERR("set mpu off failed(%d)!\n", ret);
			return ret;
		}
#endif
		CMD_T_INIT(VEB_A5_CLA_NOCRC, VEB_A5_CMD_BMODE, 0x01, 0x00, (0x72656274));
	} else {
		return VEB_ERR_PARAM;
	}
	VEB_DDUMP_WITH_PREFIX("cmd :", (char *)&cmd_v, CMD_T_SIZE);
	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		VEB_ERR("Try to wake chipset up failed(%d)!\n", ret);
		return VEB_ERR_WAKE_TIMEOUT;
	}
	ret = veb_spi_send(priv->spi, ((char *)&cmd_v), CMD_T_SIZE);
	if (ret != 0) {
		VEB_ERR("spi send failed(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}

fun_out:
	veb_spi_reset();
	VEB_TRACE_OUT();
	return VEB_OK;
}

static int veb_a5_version(struct veb_private_data *priv, char *version)
{
	char sw[2] = {0};

#ifndef CONFIG_ARCH_MT6572
	char v[VEB_VERSION_LENGTH + 2] = {0};
#endif

	cmd_t cmd_v __VEB_ALIGNED__;

	int ret = VEB_OK;

	VEB_TRACE_IN();

	if (version == NULL) {
		VEB_ERR("parameter version should not be NULL!\n");
		return VEB_ERR_PARAM;
	}

	ret = veb_spi_wake(WAKE_TIME);
	if (ret != 0) {
		VEB_ERR("Try to wake chipset up failed(%d)!\n", ret);
		return VEB_ERR_WAKE_TIMEOUT;
	}
	CMD_T_INIT(VEB_A5_CLA_NOCRC, VEB_A5_CMD_VERSION, 0, 0, VEB_VERSION_LENGTH);
	VEB_DDUMP_WITH_PREFIX("cmd :", (char *)&cmd_v, CMD_T_SIZE);

	ret = veb_spi_send(priv->spi, ((char *)&cmd_v), CMD_T_SIZE);
	if (ret != 0) {
		VEB_ERR("spi send failed(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(ret:%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}
	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(%d)!\n", ret);
		return ret;
	}

#ifndef CONFIG_ARCH_MT6572
	ret = veb_spi_recv(priv->spi, v, VEB_VERSION_LENGTH + 2);
	if (ret != 0) {
		VEB_ERR("wait ready failed!(%d)!\n", ret);
		return ret;
	}

	VEB_DDUMP_WITH_PREFIX("version :", v, (VEB_VERSION_LENGTH + 2));

	if ((v[VEB_VERSION_LENGTH + 1] != 0x90) || (v[VEB_VERSION_LENGTH] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", v[VEB_VERSION_LENGTH + 1], v[VEB_VERSION_LENGTH]);
		return VEB_ERR_STATUS;
	}
	memcpy(version, v, VEB_VERSION_LENGTH);

#else
	ret = veb_spi_recv(priv->spi, version, VEB_VERSION_LENGTH);
	if (ret != 0) {
		VEB_ERR("wait ready failed!(%d)!\n", ret);
		return ret;
	}

	ret = veb_wait_ready();
	if (ret != 0) {
		VEB_ERR("wait ready failed!(%d)!\n", ret);
		return ret;
	}

	ret = veb_spi_recv(priv->spi, sw, 2);
	if (ret != 0) {
		VEB_ERR("spi recv failed(%d)!\n", ret);
		return ret;
	}

	if ((sw[1] != 0x90) || (sw[0] != 0)) {
		VEB_ERR("status error(status:0x%02x%02x)!\n", sw[1], sw[0]);
		return VEB_ERR_STATUS;
	}
#endif

	VEB_TRACE_OUT();
	return ret;
}

int veb_a5_upgrade(struct veb_private_data *priv, int type, char *cos, int cLength)
{
	char version[VEB_VERSION_LENGTH] = {0};

	unsigned char sha1_got[20] = {0};
	unsigned char sha1_cal[20] = {0};

	int ret = VEB_OK;
	int len = 0;
	int addr = 0;
	char *buf = NULL;
	int first_addr = 0;

	if (cos == NULL) {
		VEB_ERR("cos should not be NULL!\n");
		return VEB_ERR_PARAM;
	}

	if (((type == VEB_SAFE_COS) || (type == VEB_FINANCE_LIB)) && (cLength > VEB_A5_COS_MAX)) {
		VEB_ERR("out of area(length = %d)!\n", cLength);
		return VEB_ERR_PARAM;
	}

	if (cLength % 4) {
		VEB_ERR("length(%d) should be multiples of 4!\n", cLength);
		return VEB_ERR_LEN;
	}
	if ((type == VEB_SAFE_COS) || (type == VEB_FINANCE_LIB)) {
		first_addr = 0x0040c000;
	} else if (type == VEB_FINANCE_COS) {
		first_addr = 0x0043f000;
	} else {
		VEB_ERR("param err type = %d !!!\n", type);
		return VEB_ERR_PARAM;
	}
	ret = veb_a5_version(priv, version);
	if (ret != 0) {
		VEB_ERR("get version failed(%d)!\n", ret);
		return ret;
	}
	VEB_DBG("version : %s\n", version);

	if (!strncmp("A5 SPI V", version, 8)) {
		ret = veb_a5_restore_mode(priv, VEB_TOBOOT_MODE);
		if (ret != 0) {
			VEB_ERR("restore second boot failed(%d)!\n", ret);
			return ret;
		}

		ret = veb_a5_version(priv, version);
		if (ret != 0) {
			VEB_ERR("get version failed(%d)!\n", ret);
			return ret;
		}
		VEB_DBG("version : %s\n", version);
	}
#if CONFIG_MPU
	ret = veb_a5_set_mpu_protect(priv, 0);
	if (ret != VEB_OK) {
		VEB_ERR("set mpu off failed(%d)!\n", ret);
		return ret;
	}
#endif
	ret = veb_a5_erase(priv, first_addr, cLength);
	if (ret != VEB_OK) {
		VEB_ERR("erase(length:%d) failed(%d)!\n", cLength, ret);
		return ret;
	}

	len = cLength;
	buf = cos;
	addr = first_addr;
	while (len > 0x800) {
		ret = veb_a5_program(priv, addr, buf, 0x800);
		if (ret != VEB_OK) {
			VEB_ERR("program failed(%d)!\n", ret);
			return ret;
		}

		len -= 0x800;
		buf += 0x800;
		addr += 0x800;
	}

	while (len > 0x400) {
		ret = veb_a5_program(priv, addr, buf, 0x400);
		if (ret != 0) {
			VEB_ERR("program failed(%d)!\n", ret);
			return ret;
		}

		len -= 0x400;
		buf += 0x400;
		addr += 0x400;
	}

	if (len) {
		ret = veb_a5_program(priv, addr, buf, len);
		if (ret != 0) {
			VEB_ERR("program failed(%d)!\n", ret);
			return ret;
		}
	}

	ret = veb_a5_cos_sha1_get(priv, first_addr, cLength, (char *)sha1_got);
	if (ret != VEB_OK) {
		VEB_ERR("sha1 get failed(%d)!\n", ret);
		return ret;
	}

	sha_buf((unsigned char *)cos, cLength, sha1_cal);
	if (memcmp(sha1_got, sha1_cal, 20) != 0) {
		ret = VEB_ERR_SHA;
		VEB_ERR("sha1 memcmp failed(%d)!\n", ret);
		VEB_EDUMP_WITH_PREFIX("sha1_got :", sha1_got, 20);
		VEB_EDUMP_WITH_PREFIX("sha1_cal :", sha1_cal, 20);
		return ret;
	}
	VEB_DDUMP_WITH_PREFIX("sha1_got :", sha1_got, 20);
	VEB_DDUMP_WITH_PREFIX("sha1_cal :", sha1_cal, 20);
if ((type == VEB_SAFE_COS) || (type == VEB_FINANCE_COS)) {
		ret = veb_a5_done(priv, first_addr, cLength);
		if (ret != VEB_OK) {
			VEB_ERR("done failed(%d)!\n", ret);
			return ret;
		}

		ret = veb_a5_disboot(priv);
		if (ret != VEB_OK) {
			VEB_ERR("disable boot failed(%d)!\n", ret);
			return ret;
		}

		msleep(1500);
		ret = veb_a5_version(priv, version);
		if (ret != 0) {
			VEB_ERR("get version failed(%d)!\n", ret);
			return ret;
		}
		VEB_DBG("version : %s\n", version);
	}
	VEB_TRACE_OUT();
	return ret;
}
