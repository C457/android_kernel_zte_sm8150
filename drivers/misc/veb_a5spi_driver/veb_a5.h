#ifndef __VEB_A5_H__
#define __VEB_A5_H__

#include <linux/compiler.h>
#include <linux/spi/spi.h>
#include <linux/miscdevice.h>

#define VEB_IOCTL_MAGIC					 0xa5
#define VEB_A5_NAME						 "a5SpiInHal"
#define VEB_A5_DESC						 "veb_a5 chip driver"
#define VEB_A5_VERSION					 "1.1"

#define A5_CHIP_CON_NEW				213
#define VEB_CMD_CONFIG				221
#define VEB_CMD_READ				222
#define VEB_CMD_WRITE				223
#define VEB_CMD_EXCHANGE			224
#define VEB_CMD_SYNC_CRYPTO			226
#define VEB_CMD_SYNC_DUPLEX_CRYPTO	228
#define VEB_CMD_WITHKEY_SYNC_CRYPTO 230
#define VEB_CMD_SMARTCARD_READ		231
#define VEB_CMD_SMARTCARD_EXCHANGE	232
#define VEB_CMD_SMARTCARD			233

#define VEB_CMD_RESET				253
#define VEB_CMD_RESTORE_MODE		254
#define VEB_CMD_UPGRADE				255

#define VEB_IC_STATUS_LENGTH		2
#define VEB_IC_CLA_NOCRC			0x80
#define VEB_IC_CLA_CRC				0x81

#define VEB_DATA_MMAP		0
#define VEB_DATA_COPY		1

#define VEB_SPI_BITS_PER_WORD				8
#define VEB_SPI_MODE						SPI_MODE_3
#define __VEB_ALIGNED__						__aligned(4)

#define XFER_BLOCK_SIZE			1024

#define VEB_IC_STATUS_LENGTH	2
#define VEB_IC_CLA_NOCRC		0x80
#define VEB_IC_CLA_CRC			0x81

#define VEB_DATA_MMAP		0
#define VEB_DATA_COPY		1
#define IV_LEN				16

/****************************************************************************************/
#define INIT_XFER_STRUCT(XFER, TX_BUF, RX_BUF, LEN) \
	do {												\
	   memset(&(XFER), 0, sizeof(XFER));				\
	   XFER.tx_buf	   = TX_BUF;						\
	   XFER.rx_buf	   = RX_BUF;						\
	   XFER.len		   = LEN;							\
	   XFER.cs_change  = 0;								\
	   XFER.bits_per_word = VEB_SPI_BITS_PER_WORD;		\
	   XFER.delay_usecs = 0;							\
} while (0)

/****************************************************************************************
		 STRUCT
****************************************************************************************/
typedef struct veb_private_data {
	struct miscdevice *miscdev;
	struct spi_device *spi;
	struct mutex dev_lock;

#define VEB_MMAP_SIZE		0x200000
	struct mutex mmap_lock;
	void *mmap_addr;
} veb_private_data_t;

#define CMD_T_SIZE				16
typedef struct cmd_t {
#define CMD_HEAD   0x34A50253
	unsigned int head;
	unsigned int len;

#define CMD_LEN	   4
	unsigned char cmd[CMD_LEN];

#define CHIP_CMD_CRC_LEN		15
	unsigned char crc8[4];
} cmd_t, smart_cmd_t;

/**
 * struct veb_sdata is designed to transfer data in read only or write only
 * @length		length of s
 * @type		VEB_DATA_MMAP or VEB_DATA_COPY
 * @s			buffer for read or write
 * Note:
 *	   's' means single.
 */
typedef struct veb_sdata {
	cmd_t cmd;
	int length;
	int type;
	char *s;
} veb_sdata_t;

#define VEB_FRAME_MAX			(128 * 1024)
typedef struct veb_mmap_data {
	char source[VEB_FRAME_MAX];
	char sink[VEB_FRAME_MAX];
} veb_mmap_data_t;

typedef struct veb_ddata {
	cmd_t cmd;
	int type;
	char *source;
	int srLength;
	char *sink;
	int skLength;
} veb_ddata_t;

typedef struct {
	cmd_t cmd;
	int type;
	unsigned int inlen;
	unsigned char *inbuf;
	unsigned int outlen;
	unsigned char *outbuf;
	unsigned char *iv;
} veb_crypto_data_t;

typedef struct {
	unsigned int len;
	unsigned char buf[32];
#define	   AES_256_KEY_LEN					32
} veb_key_info;

typedef struct {
	cmd_t cmd;
	int type;
	unsigned int inlen;
	unsigned char *inbuf;
	unsigned int outlen;
	unsigned char *outbuf;
	unsigned char *iv;
	veb_key_info veb_key;
} veb_withkey_data_t;

typedef struct veb_cos {
	int cos_type;
	int length;
	int type;
	char *s;
} veb_cos_t;
#endif
