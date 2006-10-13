/*
 * $Id$
 *
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * History:
 * --------
 *  2006-08-15  initial version (anca)
 */

#include "utils_func.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../mem/mem.h"
#include "../../dprint.h"
#include <ctype.h>

static const char base64digits[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#define BAD     -1
static const char base64val[] = {
BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD,
BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD,
BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD, 62, BAD,BAD,BAD, 63,
52, 53, 54, 55,  56, 57, 58, 59,  60, 61,BAD,BAD, BAD,BAD,BAD,BAD,
BAD,  0,  1,  2,   3,  4,  5,  6,   7,  8,  9, 10,  11, 12, 13, 14,
15, 16, 17, 18,  19, 20, 21, 22,  23, 24, 25,BAD, BAD,BAD,BAD,BAD,
BAD, 26, 27, 28,  29, 30, 31, 32,  33, 34, 35, 36,  37, 38, 39, 40,
41, 42, 43, 44,  45, 46, 47, 48,  49, 50, 51,BAD, BAD,BAD,BAD,BAD
};
#define DECODE64(c)  (isascii(c) ? base64val[c] : BAD)

void to64frombits(unsigned char *out, const unsigned char *in, int inlen)
{
	for (; inlen >= 3; inlen -= 3)
	{
		*out++ = base64digits[in[0] >> 2];
		*out++ = base64digits[((in[0] << 4) & 0x30) | (in[1] >> 4)];
		*out++ = base64digits[((in[1] << 2) & 0x3c) | (in[2] >> 6)];
		*out++ = base64digits[in[2] & 0x3f];
		in += 3;
	}

	if (inlen > 0)
	{
		unsigned char fragment;

		*out++ = base64digits[in[0] >> 2];
		fragment = (in[0] << 4) & 0x30;

		if (inlen > 1)
			fragment |= in[1] >> 4;

		*out++ = base64digits[fragment];
		*out++ = (inlen < 2) ? '=' : base64digits[(in[1] << 2) & 0x3c];
		*out++ = '=';
	}
		*out = '\0';
		
}


int uandd_to_uri(str user,  str domain, str *out)
{
	int size;

	if(out==0)
		return -1;

	size = user.len + domain.len+7;

	out->s = (char*)pkg_malloc(size*sizeof(char));
	if(out->s == NULL)
	{
		LOG(L_ERR, "PRESENCE: uandd_to_uri: Error while allocating memory\n");
		return -1;
	}
	out->len = 0;
	strcpy(out->s,"sip:");
	out->len = 4;
	strncpy(out->s+out->len, user.s, user.len);
	out->len += user.len;
	out->s[out->len] = '@';
	out->len+= 1;
	strncpy(out->s + out->len, domain.s, domain.len);
	out->len += domain.len;

	out->s[out->len] = 0;
	DBG("presence:uandd_to_uri: uri=%.*s\n", out->len, out->s);
	
	return 0;
}

/*
str* int_to_str(long int n)
{
	str* n_str;
	int m = n, i=0;
	if(m==0)
		i=1;

	while( m>0 )
	{
		m= m/10;
		i++;
	}

	n_str = (str*)pkg_malloc(sizeof(str));
	if(n_str == NULL)
	{
		LOG(L_ERR,"PRESENCE:int_to_str: ERROR while allocating memory\n");
		return NULL;
	}	
	n_str->s = (char*)pkg_malloc( (i+1) * sizeof(char));
	if(n_str->s == NULL)
	{
		LOG(L_ERR,"PRESENCE:int_to_str: ERROR while allocating memory\n");
		return NULL;
	}
	
	n_str->len = i;
	n_str->s[i] = 0;	
	while( i>0 )
	{
		i--;
		n_str->s[i]=n%10 + '0';
		n = n/10;
	}
	return n_str;
}
*/
int a_to_i (char *s,int len)
{
	int n = 0, i= 0;
	
	while( i<len  )		
		n=n*10+( s[i++] -'0');
	
	return n;
}

