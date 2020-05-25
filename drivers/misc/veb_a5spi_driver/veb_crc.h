#ifndef __VEB_CRC_H__
#define __VEB_CRC_H__

unsigned char get_crc8_rohc(unsigned char start, const unsigned char *buff, int len);
#define get_crc8_0xff(buf, len) get_crc8_rohc(0xff, buf, len)

unsigned short crc16_ccitt_false(unsigned short start, unsigned char *buff, unsigned int len);
#define get_crc16_ccitt(buf, len)	crc16_ccitt_false(0xffff, buf, len)

unsigned int crc32_mpeg(unsigned int start, unsigned char *buff, unsigned int len);
#define get_crc32_mpeg(buf, len)	crc32_mpeg(0xffffffff, buf, len)

#endif
