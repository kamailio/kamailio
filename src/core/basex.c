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
#error Neither __IS_LITTE_ENDIAN nor __IS_BIG_ENDIAN  defined
#endif
	/* decode */
	for (r=0; r<256; r++)
		_bx_ub64[r]=b64_dec_char((unsigned char)r);
#endif
#endif
	return 0;
}
