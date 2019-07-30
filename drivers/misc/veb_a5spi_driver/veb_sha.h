#ifndef __VEB_SHA_H__
#define __VEB_SHA_H__

#include <linux/types.h>
/*typedef unsigned int uint32_t;*/

#define	   HASH_LEN							20
#define	   FIX
#define	   H0								0x67452301L
#define	   H1								0xefcdab89L
#define	   H2								0x98badcfeL
#define	   H3								0x10325476L
#define	   H4								0xc3d2e1f0L

#define	   K0								0x5a827999L
#define	   K1								0x6ed9eba1L
#define	   K2								0x8f1bbcdcL
#define	   K3								0xca62c1d6L

#define	   PAD								0x80
#define	   ZERO								0

typedef struct {
	uint32_t length[2];
	uint32_t h[8];
	uint32_t w[80];
} sha256;


/* functions */
#define	   S(n, x)							(((x) << n) | ((x) >> (32 - n)))
#define	   F0(x, y, z)						(z ^ (x & (y ^ z)))
#define	   F1(x, y, z)						(x ^ y ^ z)
#define	   F2(x, y, z)						((x & y) | (z & (x | y)))
#define	   F3(x, y, z)						(x ^ y ^ z)



extern void sha_buf(unsigned char *buf, int len, unsigned char hash[20]);
#endif
