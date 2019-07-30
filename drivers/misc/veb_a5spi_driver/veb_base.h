#ifndef __VEB_BASE_H__
#define __VEB_BASE_H__

#include <linux/spi/spi.h>

#include "veb_a5.h"

extern int check_sw(unsigned char *sw);

extern int veb_spi_recv(struct spi_device *spi, char *rx, int length);
extern int veb_spi_send(struct spi_device *spi, char *tx, int length);
extern int veb_spi_duplex(struct spi_device *spi, char *tx, char *rx, int length);

extern int veb_cmd_read(struct spi_device *spi, cmd_t *cmd, char *buf, int len);
extern int veb_cmd_write(struct spi_device *spi, cmd_t *cmd, unsigned char *buf, int len);
extern int veb_cmd_config(struct spi_device *spi, cmd_t *cmd);
extern int veb_cmd_exchange(struct spi_device *spi, cmd_t *cmd, unsigned char *source, unsigned int srlength,
							unsigned char *sink, unsigned int sklength);
extern int veb_sym_crypto(struct spi_device *spi, cmd_t *cmd, unsigned char *buf, unsigned char *IV, int len);
extern int veb_sym_duplex_crypto(struct spi_device *spi, cmd_t *cmd, unsigned char *buf, unsigned char *IV, int len);
extern int veb_sym_withkey_cmd_write(struct spi_device *spi, cmd_t *cmd, unsigned char *buf, veb_key_info *veb_key,
										unsigned char *IV, int len);

#endif
