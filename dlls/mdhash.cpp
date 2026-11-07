/***
*
*	Rocket Crowbar 1.9: mdhash.cpp, RIPEMD-160 (mdhash.h).
*
*	The original is the reference code's macro-unrolled compress (6306 bytes); this one walks
*	the same rounds from tables. Same digest; checked against the RIPEMD-160 test vectors.
*
****/

#include <string.h>

#include "mdhash.h"

#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

// orig 0x1c2080
void MDinit(uint32_t* MDbuf)
{
	MDbuf[0] = 0x67452301UL;
	MDbuf[1] = 0xefcdab89UL;
	MDbuf[2] = 0x98badcfeUL;
	MDbuf[3] = 0x10325476UL;
	MDbuf[4] = 0xc3d2e1f0UL;
}

static uint32_t RMD_F(int j, uint32_t x, uint32_t y, uint32_t z)
{
	switch (j / 16)
	{
	case 0:
		return x ^ y ^ z;
	case 1:
		return (x & y) | (~x & z);
	case 2:
		return (x | ~y) ^ z;
	case 3:
		return (x & z) | (y & ~z);
	default:
		return x ^ (y | ~z);
	}
}

static const uint32_t s_K[5] = {0x00000000UL, 0x5a827999UL, 0x6ed9eba1UL, 0x8f1bbcdcUL, 0xa953fd4eUL};
static const uint32_t s_KP[5] = {0x50a28be6UL, 0x5c4dd124UL, 0x6d703ef3UL, 0x7a6d76e9UL, 0x00000000UL};

static const int s_R[80] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	7, 4, 13, 1, 10, 6, 15, 3, 12, 0, 9, 5, 2, 14, 11, 8,
	3, 10, 14, 4, 9, 15, 8, 1, 2, 7, 0, 6, 13, 11, 5, 12,
	1, 9, 11, 10, 0, 8, 12, 4, 13, 3, 7, 15, 14, 5, 6, 2,
	4, 0, 5, 9, 7, 12, 2, 10, 14, 1, 3, 8, 11, 6, 15, 13};

static const int s_RP[80] = {
	5, 14, 7, 0, 9, 2, 11, 4, 13, 6, 15, 8, 1, 10, 3, 12,
	6, 11, 3, 7, 0, 13, 5, 10, 14, 15, 8, 12, 4, 9, 1, 2,
	15, 5, 1, 3, 7, 14, 6, 9, 11, 8, 12, 2, 10, 0, 4, 13,
	8, 6, 4, 1, 3, 11, 15, 0, 5, 12, 2, 13, 9, 7, 10, 14,
	12, 15, 10, 4, 1, 5, 8, 7, 6, 2, 13, 14, 0, 3, 9, 11};

static const int s_S[80] = {
	11, 14, 15, 12, 5, 8, 7, 9, 11, 13, 14, 15, 6, 7, 9, 8,
	7, 6, 8, 13, 11, 9, 7, 15, 7, 12, 15, 9, 11, 7, 13, 12,
	11, 13, 6, 7, 14, 9, 13, 15, 14, 8, 13, 6, 5, 12, 7, 5,
	11, 12, 14, 15, 14, 15, 9, 8, 9, 14, 5, 6, 8, 6, 5, 12,
	9, 15, 5, 11, 6, 8, 13, 12, 5, 12, 13, 14, 11, 8, 5, 6};

static const int s_SP[80] = {
	8, 9, 9, 11, 13, 15, 15, 5, 7, 7, 8, 11, 14, 14, 12, 6,
	9, 13, 15, 7, 12, 8, 9, 11, 7, 7, 12, 7, 6, 15, 13, 11,
	9, 7, 15, 11, 8, 6, 6, 14, 12, 13, 5, 14, 13, 13, 7, 5,
	15, 5, 8, 11, 14, 14, 6, 14, 6, 9, 12, 9, 12, 5, 15, 8,
	8, 5, 12, 9, 12, 5, 14, 6, 8, 13, 6, 5, 15, 13, 11, 11};

// orig 0x1c20b0. One 16-word block into MDbuf.
void compress(uint32_t* MDbuf, uint32_t* X)
{
	uint32_t aa = MDbuf[0], bb = MDbuf[1], cc = MDbuf[2], dd = MDbuf[3], ee = MDbuf[4];
	uint32_t aaa = aa, bbb = bb, ccc = cc, ddd = dd, eee = ee;

	for (int j = 0; j < 80; j++)
	{
		uint32_t t = aa + RMD_F(j, bb, cc, dd) + X[s_R[j]] + s_K[j / 16];
		t = ROL(t, s_S[j]) + ee;
		aa = ee;
		ee = dd;
		dd = ROL(cc, 10);
		cc = bb;
		bb = t;

		t = aaa + RMD_F(79 - j, bbb, ccc, ddd) + X[s_RP[j]] + s_KP[j / 16];
		t = ROL(t, s_SP[j]) + eee;
		aaa = eee;
		eee = ddd;
		ddd = ROL(ccc, 10);
		ccc = bbb;
		bbb = t;
	}

	ddd += cc + MDbuf[1];
	MDbuf[1] = MDbuf[2] + dd + eee;
	MDbuf[2] = MDbuf[3] + ee + aaa;
	MDbuf[3] = MDbuf[4] + aa + bbb;
	MDbuf[4] = MDbuf[0] + bb + ccc;
	MDbuf[0] = ddd;
}

// orig 0x1c3960. Pads the last (lswlen & 63) bytes at strptr and appends the bit length.
void MDfinish(uint32_t* MDbuf, unsigned char* strptr, uint32_t lswlen, uint32_t mswlen)
{
	uint32_t X[16];
	memset(X, 0, sizeof(X));

	for (uint32_t i = 0; i < (lswlen & 63); i++)
		X[i >> 2] ^= (uint32_t)*strptr++ << (8 * (i & 3));

	X[(lswlen >> 2) & 15] ^= (uint32_t)1 << (8 * (lswlen & 3) + 7);

	if ((lswlen & 63) > 55)
	{
		compress(MDbuf, X);
		memset(X, 0, sizeof(X));
	}

	X[14] = lswlen << 3;
	X[15] = (lswlen >> 29) | (mswlen << 3);
	compress(MDbuf, X);
}

// orig 0x1c3a40. The digest of a NUL-terminated string.
unsigned char* RMD(unsigned char* message)
{
	static unsigned char hashcode[20];
	uint32_t MDbuf[5];
	uint32_t X[16];

	MDinit(MDbuf);
	uint32_t length = (uint32_t)strlen((char*)message);
	for (uint32_t nbytes = length; nbytes > 63; nbytes -= 64)
	{
		for (int i = 0; i < 16; i++)
		{
			X[i] = ((uint32_t)message[3] << 24) | ((uint32_t)message[2] << 16) | ((uint32_t)message[1] << 8) | (uint32_t)message[0];
			message += 4;
		}
		compress(MDbuf, X);
	}
	MDfinish(MDbuf, message, length, 0);

	for (int i = 0; i < 20; i += 4)
	{
		hashcode[i] = (unsigned char)MDbuf[i >> 2];
		hashcode[i + 1] = (unsigned char)(MDbuf[i >> 2] >> 8);
		hashcode[i + 2] = (unsigned char)(MDbuf[i >> 2] >> 16);
		hashcode[i + 3] = (unsigned char)(MDbuf[i >> 2] >> 24);
	}
	return hashcode;
}
