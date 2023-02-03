/*
 * convert/decode to/from ascii using various bases
 *
 * Copyright (C) 2008 iptelorg GmbH
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Defines:
 *  BASE64_LOOKUP_TABLE - use small lookup tables for conversions (faster
 *                         in general)
 */
/*!
 * \file
 * \brief Kamailio core :: convert/decode to/from ascii using various bases
 * \ingroup core
 * Module: \ref core
 */

#include <string.h>

#include "dprint.h"
#include "basex.h"

#ifdef BASE16_LOOKUP_TABLE
#ifdef BASE16_LOOKUP_LARGE

unsigned char _bx_hexdig_hi[256]={
	'0', '0', '0', '0', '0', '0', '0', '0',
	'0', '0', '0', '0', '0', '0', '0', '0',
	'1', '1', '1', '1', '1', '1', '1', '1',
	'1', '1', '1', '1', '1', '1', '1', '1',
	'2', '2', '2', '2', '2', '2', '2', '2',
	'2', '2', '2', '2', '2', '2', '2', '2',
	'3', '3', '3', '3', '3', '3', '3', '3',
	'3', '3', '3', '3', '3', '3', '3', '3',
	'4', '4', '4', '4', '4', '4', '4', '4',
	'4', '4', '4', '4', '4', '4', '4', '4',
	'5', '5', '5', '5', '5', '5', '5', '5',
	'5', '5', '5', '5', '5', '5', '5', '5',
	'6', '6', '6', '6', '6', '6', '6', '6',
	'6', '6', '6', '6', '6', '6', '6', '6',
	'7', '7', '7', '7', '7', '7', '7', '7',
	'7', '7', '7', '7', '7', '7', '7', '7',
	'8', '8', '8', '8', '8', '8', '8', '8',
	'8', '8', '8', '8', '8', '8', '8', '8',
	'9', '9', '9', '9', '9', '9', '9', '9',
	'9', '9', '9', '9', '9', '9', '9', '9',
	'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A',
	'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A',
	'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B',
	'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B',
	'C', 'C', 'C', 'C', 'C', 'C', 'C', 'C',
	'C', 'C', 'C', 'C', 'C', 'C', 'C', 'C',
	'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D',
	'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D',
	'E', 'E', 'E', 'E', 'E', 'E', 'E', 'E',
	'E', 'E', 'E', 'E', 'E', 'E', 'E', 'E',
	'F', 'F', 'F', 'F', 'F', 'F', 'F', 'F',
	'F', 'F', 'F', 'F', 'F', 'F', 'F', 'F'
};

unsigned char _bx_hexdig_low[256]={
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

unsigned char _bx_unhexdig256[256]={
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01, 
0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 
0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0x0b, 0x0c, 
0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

#else /* BASE16_LOOKUP_LARGE */

unsigned char _bx_hexdig[16+1]="0123456789ABCDEF";

unsigned char _bx_unhexdig32[32]={
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0a, 0x0b, 0x0c,
	0x0d, 0x0e, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff };

#endif /*  BASE16_LOOKUP_LARGE */
#endif /* BASE16_LOOKUP_TABLE */

#ifdef BASE64_LOOKUP_TABLE

#ifdef BASE64_LOOKUP_LARGE
/* large lookup tables, 2.5 k */

unsigned char _bx_b64_first[256];
unsigned char _bx_b64_second[4][256];
unsigned char _bx_b64_third[4][256];
unsigned char _bx_b64_fourth[256];

unsigned char _bx_ub64[256];

#elif defined BASE64_LOOKUP_8K
unsigned short _bx_b64_12[4096];
unsigned char _bx_ub64[256];

#else /*  BASE64_LOOKUP_LARGE */
/* very small lookup, 65 bytes */

unsigned char _bx_b64[64+1]=
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


unsigned char _bx_ub64[0x54+1]={
		                              0x3e, 0xff, 0xff, 0xff, 0x3f,
		0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x01, 0x02,
		0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
		0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
		0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x1a,
		0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23, 0x24,
		0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e,
		0x2f, 0x30, 0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff };

#endif /*  BASE64_LOOKUP_LARGE */

#endif /* BASE64_LOOKUP_TABLE */

#define b64_enc_char(c) base64_enc_char(c)
#define b64_dec_char(c) base64_dec_char(c)

int init_basex()
{
#ifdef BASE64_LOOKUP_TABLE
#if defined BASE64_LOOKUP_LARGE || defined BASE64_LOOKUP_8K
	int r;
#endif
#ifdef BASE64_LOOKUP_LARGE
	int i;
	
	/* encode tables */
	for (r=0; r<256; r++)
		_bx_b64_first[r]=b64_enc_char(((unsigned char)r)>>2);
	for(i=0; i<4; i++){
		for (r=0; r<256; r++)
			_bx_b64_second[i][r]=
					b64_enc_char((unsigned char)((i<<4)|(r>>4)));
	}
	for(i=0; i<4; i++){
		for (r=0; r<256; r++)
			_bx_b64_third[i][r]=
				b64_enc_char((unsigned char)(((r<<2)&0x3f)|i));
	}
	for (r=0; r<256; r++)
		_bx_b64_fourth[r]=b64_enc_char(((unsigned char)r&0x3f));
	
	/* decode */
	for (r=0; r<256; r++)
		_bx_ub64[r]=b64_dec_char((unsigned char)r);
#elif defined BASE64_LOOKUP_8K
	for (r=0; r< 4096; r++)
#if defined __IS_LITTLE_ENDIAN
		_bx_b64_12[r]=b64_enc_char(r>>6)|(b64_enc_char(r&0x3f)<<8);
#elif defined __IS_BIG_ENDIAN /* __IS_LITTLE_ENDIAN */
		_bx_b64_12[r]=(b64_enc_char(r>>6)<<8)|b64_enc_char(r&0x3f);
#else /* __IS_LITTLE_ENDIAN */
#error Neither __IS_LITTLE_ENDIAN nor __IS_BIG_ENDIAN  defined
#endif
	/* decode */
	for (r=0; r<256; r++)
		_bx_ub64[r]=b64_dec_char((unsigned char)r);
#endif
#endif
	return 0;
}

/* base58 implementation */
/* adapted from https://github.com/luke-jr/libbase58
 * (Copyright 2014 Luke Dashjr - MIT License) */
static const int8_t _sr_b58map[] = {
	-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1,-1,-1,-1,-1,
	-1, 0, 1, 2, 3, 4, 5, 6,  7, 8,-1,-1,-1,-1,-1,-1,
	-1, 9,10,11,12,13,14,15, 16,-1,17,18,19,20,21,-1,
	22,23,24,25,26,27,28,29, 30,31,32,-1,-1,-1,-1,-1,
	-1,33,34,35,36,37,38,39, 40,41,42,43,-1,44,45,46,
	47,48,49,50,51,52,53,54, 55,56,57,-1,-1,-1,-1,-1,
};

static const char _sr_b58digits[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

/**
 * decode base58 string stored in b58 (of size b58sz) and store result in outb
 *   - *outbszp provides the size of outb buffer
 *   - *outbszp is updated to the length of decoded data
 *   - returns NULL in case of failure, or the pointer inside outb from where
 *   the decoded data starts (it is 0-terminated)
 */
char* b58_decode(char *outb, int *outbszp, char *b58, int b58sz)
{
	size_t outbsz = *outbszp - 1 /* save space for ending 0 */;
	const unsigned char *b58u = (void*)b58;
	unsigned char *outu = (void*)outb;
	size_t outisz = (outbsz + 3) / 4;
	uint32_t outi[outisz];
	uint64_t t;
	uint32_t c;
	size_t i, j;
	uint8_t bytesleft = outbsz % 4;
	uint32_t zeromask = bytesleft ? (0xffffffff << (bytesleft * 8)) : 0;
	unsigned zerocount = 0;

	if (!b58sz)
		b58sz = strlen(b58);

	outb[outbsz-1] = '\0';
	memset(outi, 0, outisz * sizeof(*outi));

	/* leading zeros, just count */
	for (i = 0; i < b58sz && b58u[i] == '1'; ++i)
		++zerocount;

	for ( ; i < b58sz; ++i)
	{
		if (b58u[i] & 0x80) {
			LM_ERR("high-bit set on invalid digit\n");
			return NULL;
		}
		if (_sr_b58map[b58u[i]] == -1) {
			LM_ERR("invalid base58 digit\n");
			return NULL;
		}
		c = (unsigned)_sr_b58map[b58u[i]];
		for (j = outisz; j--; )
		{
			t = ((uint64_t)outi[j]) * 58 + c;
			c = (t & 0x3f00000000) >> 32;
			outi[j] = t & 0xffffffff;
		}
		if (c) {
			LM_ERR("output number too big (carry to the next int32)\n");
			return NULL;
		}
		if (outi[0] & zeromask) {
			LM_ERR("output number too big (last int32 filled too far)\n");
			return NULL;
		}
	}

	j = 0;
	switch (bytesleft) {
		case 3:
			*(outu++) = (outi[0] &   0xff0000) >> 16;
		case 2:
			*(outu++) = (outi[0] &     0xff00) >>  8;
		case 1:
			*(outu++) = (outi[0] &       0xff);
			++j;
		default:
			break;
	}

	for (; j < outisz; ++j)
	{
		*(outu++) = (outi[j] >> 0x18) & 0xff;
		*(outu++) = (outi[j] >> 0x10) & 0xff;
		*(outu++) = (outi[j] >>    8) & 0xff;
		*(outu++) = (outi[j] >>    0) & 0xff;
	}

	/* count canonical base58 byte count */
	outu = (void*)outb;
	for (i = 0; i < outbsz; ++i)
	{
		if (outu[i])
			break;
		--*outbszp;
	}
	*outbszp += zerocount;

	return outb + i;
}

/**
 * encode raw data (of size binsz) into base58 format stored in b58
 *   - *b58sz gives the size of b58 buffer
 *   - *b58sz is updated to the length of result
 *   - b58 is 0-terminated
 *   - return NULL on failure or b58
 */
char* b58_encode(char *b58, int *b58sz, char *data, int binsz)
{
	const uint8_t *bin = (void*)data;
	int carry;
	ssize_t i, j, high, zcount = 0;
	size_t size;

	while (zcount < binsz && !bin[zcount])
		++zcount;

	size = (binsz - zcount) * 138 / 100 + 1;
	uint8_t buf[size];
	memset(buf, 0, size);

	for (i = zcount, high = size - 1; i < binsz; ++i, high = j)
	{
		for (carry = bin[i], j = size - 1; (j > high) || carry; --j)
		{
			carry += 256 * buf[j];
			buf[j] = carry % 58;
			carry /= 58;
		}
	}

	for (j = 0; j < size && !buf[j]; ++j);

	if (*b58sz <= zcount + size - j)
	{
		*b58sz = zcount + size - j + 1;
		return NULL;
	}

	if (zcount)
		memset(b58, '1', zcount);
	for (i = zcount; j < size; ++i, ++j)
		b58[i] = _sr_b58digits[buf[j]];
	b58[i] = '\0';
	*b58sz = i;

	return b58;
}

/* base64-url encoding table (RFC 4648 document) */
static const char _ksr_b64url_encmap[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

/* base64-url decoding table */
static const int _ksr_b64url_decmap[] = {
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, 0x3E, -1, -1,
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B,
	0x3C, 0x3D, -1, -1, -1, -1, -1, -1,
	-1, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
	0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
	0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
	0x17, 0x18, 0x19, -1, -1, -1, -1, 0x3F,
	-1, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
	0x31, 0x32, 0x33, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1,
	-1, -1, -1, -1, -1, -1, -1, -1
};

int base64url_enc(char *in, int ilen, char *out, int osize)
{
	int  left;
	int  idx;
	int  i;
	int  r;
	char *p;
	unsigned int  block;
	int  olen;

	olen = (((ilen+2)/3)<<2);
	if (olen >= osize) {
		LM_ERR("not enough output buffer size %d - need %d\n", osize, olen+1);
		return -1;
	}
	memset(out, 0, (olen+1)*sizeof(char));

	p = out;
	for(idx=0; idx<ilen; idx+=3) {
		left = ilen - idx - 1 ;
		left = (left>1)?2:left;

		block = 0;
		for(i=0, r=16; i<=left; i++, r-=8)
			block += ((unsigned char)in[idx+i]) << r;

		*(p++) = _ksr_b64url_encmap[(block >> 18) & 0x3f];
		*(p++) = _ksr_b64url_encmap[(block >> 12) & 0x3f];
		*(p++) = (left>0)?_ksr_b64url_encmap[(block >> 6) & 0x3f]:'=';
		*(p++) = (left>1)?_ksr_b64url_encmap[block & 0x3f]:'=';
	}

	return olen;
}

int base64url_dec(char *in, int ilen, char *out, int osize)
{
	int n;
	unsigned int block;
	int idx;
	int i;
	int j;
	int end;
	char c;
	int olen;

	for(n=0,i=ilen-1; in[i]=='='; i--)
		n++;

	olen = ((ilen * 6) >> 3) - n;

	if (olen<=0) {
		LM_ERR("invalid olen parameter calculated, can't continue %d\n", olen);
		return -1;
	}

	if(olen >= osize) {
		LM_ERR("not enough output buffer size %d - need %d\n", osize, olen+1);
		return -1;
	}
	memset(out, 0, (olen+1)*sizeof(char));

	end = ilen - n;
	for(i=0, idx=0; i<end; idx+=3) {
		block = 0;
		for(j=0; j<4 && i<end ; j++) {
			c = _ksr_b64url_decmap[(unsigned char)in[i++]];
			if(c<0) {
				LM_ERR("invalid input string\"%.*s\"\n", ilen, in);
				return -1;
			}
			block += c << (18 - 6*j);
		}

		for(j=0, n=16; j<3 && idx+j<olen; j++, n-=8)
			out[idx+j] = (char)((block >> n) & 0xff);
	}

	return olen;
}

