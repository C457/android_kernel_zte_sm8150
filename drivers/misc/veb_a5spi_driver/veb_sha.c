#include "veb_sha.h"


typedef sha256 sha;

static void shs_transform(sha *sh)
{
	uint32_t a, b, c, d, e, temp;
	int t;

#ifdef FIX
	for (t = 16; t < 80; t++)
		sh->w[t] = S(1, sh->w[t - 3] ^ sh->w[t - 8] ^ sh->w[t - 14] ^ sh->w[t - 16]);

#else
	for (t = 16; t < 80; t++)
		sh->w[t] = sh->w[t - 3] ^ sh->w[t - 8] ^ sh->w[t - 14] ^ sh->w[t - 16];

#endif

	a = sh->h[0]; b = sh->h[1]; c = sh->h[2]; d = sh->h[3]; e = sh->h[4];

	for (t = 0; t < 20; t++) {
		temp = K0 + F0(b, c, d) + S(5, a) + e + sh->w[t];
		e = d;
		d = c;
		c = S(30, b);
		b = a;
		a = temp;
	}

	for (t = 20; t < 40; t++) {
		temp = K1 + F1(b, c, d) + S(5, a) + e + sh->w[t];
		e = d;
		d = c;
		c = S(30, b);
		b = a;
		a = temp;
	}

	for (t = 40; t < 60; t++) {
		temp = K2 + F2(b, c, d) + S(5, a) + e + sh->w[t];
		e = d;
		d = c;
		c = S(30, b);
		b = a;
		a = temp;
	}

	for (t = 60; t < 80; t++) {
		temp = K3 + F3(b, c, d) + S(5, a) + e + sh->w[t];
		e = d;
		d = c;
		c = S(30, b);
		b = a;
		a = temp;
	}

	sh->h[0] += a; sh->h[1] += b; sh->h[2] += c;
	sh->h[3] += d; sh->h[4] += e;

}


static void shs_init(sha *sh)
{
	int i;

	for (i = 0; i < 80; i++) {
		sh->w[i] = 0L;
	}
	sh->length[0] = sh->length[1] = 0L;
	sh->h[0] = H0;
	sh->h[1] = H1;
	sh->h[2] = H2;
	sh->h[3] = H3;
	sh->h[4] = H4;
}



static void shs_process(sha *sh, int byte)
{
	int cnt;

	cnt = (int)((sh->length[0] / 32) % 16);

	sh->w[cnt] <<= 8;
	sh->w[cnt] |= (uint32_t)(byte & 0xFF);

	sh->length[0] += 8;

	if (sh->length[0] == 0L) {
		sh->length[1]++;
		sh->length[0] = 0L;
	}

	if ((sh->length[0] % 512) == 0) {
		shs_transform(sh);
	}

}

static void shs_hash(sha *sh, char hash[20])
{
	int i;
	uint32_t len0, len1;

	len0 = sh->length[0];
	len1 = sh->length[1];
	shs_process(sh, PAD);

	while ((sh->length[0] % 512) != 448)
		shs_process(sh, ZERO);
	sh->w[14] = len1;
	sh->w[15] = len0;
	shs_transform(sh);

	for (i = 0; i < 20; i++) {
		hash[i] = ((sh->h[i/4] >> (8 * (3 - i % 4))) & 0xffL);
	}

}




void sha_buf(unsigned char *buf, int len, unsigned char hash[20])
{
	int i = 0;
	sha sh;

	shs_init(&sh);
	for (i = 0; i < len; i++) {
		shs_process(&sh, buf[i]);
	}
	shs_hash(&sh, hash);

}
