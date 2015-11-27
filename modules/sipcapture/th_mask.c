/**
 *
 * Copyright (C) 2009 SIP-Router.org
 *
 * This file is part of Kamailio, a free SIP server.
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
 */
/*!
 * \file
 * \brief Kamailio topoh :: 
 * \ingroup topoh
 * Module: \ref topoh
 */

#include <string.h>

#include "../../dprint.h"
#include "../../md5.h"
#include "../../crc.h"
#include "../../mem/mem.h"
#include "th_mask.h"

#define TH_EB64I \
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-"

char _th_EB64[65];
int _th_DB64[256];
char *_th_PD64 = "*";

extern str _th_key;

void th_shuffle(char *in, int size)
{
	char tmp;
	int last;
	unsigned int r;
	unsigned char md5[16];
	unsigned int *md5i;
	unsigned int crc;
	MD5_CTX ctx;

	MD5Init(&ctx);
	MD5Update(&ctx, _th_key.s, _th_key.len);
	MD5Update(&ctx, _th_key.s, _th_key.len);
	U_MD5Final(md5, &ctx);

	md5i = (unsigned int*)md5;

	crc = (unsigned int)crcitt_string(_th_key.s, _th_key.len);
	for (last = size; last > 1; last--)
	{
		r = (md5i[(crc+last+_th_key.len)%4]
				+ _th_key.s[(crc+last+_th_key.len)%_th_key.len]) % last;
		tmp = in[r];
		in[r] = in[last - 1];
		in[last - 1] = tmp;
	}
}

void th_mask_init(void)
{
	int i;

	memcpy(_th_EB64, TH_EB64I, sizeof(TH_EB64I));
	th_shuffle(_th_EB64, 64);
	LM_DBG("original table: %s\n", TH_EB64I);
	LM_DBG("updated table: %s\n", _th_EB64);
	for(i=0; i<256; i++)
		_th_DB64[i] = -1;
	for(i=0; i<64; i++)
		_th_DB64[(int)_th_EB64[i]] = i;

	return;
}

char* th_mask_encode(char *in, int ilen, str *prefix, int *olen)
{
	char *out;
	int  left;
	int  idx;
	int  i;
	int  r;
	char *p;
	int  block;

	*olen = (((ilen+2)/3)<<2) + ((prefix!=NULL&&prefix->len>0)?prefix->len:0);
	out = (char*)pkg_malloc((*olen+1)*sizeof(char));
	if(out==NULL)
	{
		LM_ERR("no more pkg\n");
		*olen = 0;
		return NULL;
	}
	memset(out, 0, (*olen+1)*sizeof(char));
	if(prefix!=NULL&&prefix->len>0)
		memcpy(out, prefix->s, prefix->len);

	p = out + (int)((prefix!=NULL&&prefix->len>0)?prefix->len:0);
	for(idx=0; idx<ilen; idx+=3)
	{
		left = ilen - idx - 1 ;
		left = (left>1)?2:left;

		block = 0;
		for(i=0, r=16; i<=left; i++, r-=8)
			block += ((unsigned char)in[idx+i]) << r;

		*(p++) = _th_EB64[(block >> 18) & 0x3f];
		*(p++) = _th_EB64[(block >> 12) & 0x3f];
		*(p++) = (left>0)?_th_EB64[(block >> 6) & 0x3f]:_th_PD64[0];
		*(p++) = (left>1)?_th_EB64[block & 0x3f]:_th_PD64[0];
	}

	return out;
}

char* th_mask_decode(char *in, int ilen, str *prefix, int extra, int *olen)
{
	char *out;
	int n;
	int block;
	int idx;
	int i;
	int j;
	int end;
	char c;

	for(n=0,i=ilen-1; in[i]==_th_PD64[0]; i--)
		n++;

	*olen = (((ilen-((prefix!=NULL&&prefix->len>0)?prefix->len:0)) * 6) >> 3)
				- n;
	out = (char*)pkg_malloc((*olen+1+extra)*sizeof(char));

	if(out==NULL)
	{
		LM_ERR("no more pkg\n");
		*olen = 0;
		return NULL;
	}
	memset(out, 0, (*olen+1+extra)*sizeof(char));

	end = ilen - n;
	i = (prefix!=NULL&&prefix->len>0)?prefix->len:0;
	for(idx=0; i<end; idx+=3)
	{
		block = 0;
		for(j=0; j<4 && i<end ; j++)
		{
			c = _th_DB64[(int)in[i++]];
			if(c<0)
			{
				LM_ERR("invalid input string\"%.*s\"\n", ilen, in);
				pkg_free(out);
				*olen = 0;
				return NULL;
			}
			block += c << (18 - 6*j);
		}

		for(j=0, n=16; j<3 && idx+j< *olen; j++, n-=8)
			out[idx+j] = (char)((block >> n) & 0xff);
	}

	return out;
}


