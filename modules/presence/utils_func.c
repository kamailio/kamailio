/*
 * presence module - presence server implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*! \file
 * \brief Kamailio presence module :: Utilities
 * \ingroup presence 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../data_lump_rpl.h"
#include "utils_func.h"
#include "event_list.h"
#include "presence.h"


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

int a_to_i (char *s,int len)
{
	int n = 0, i= 0;
	
	while( i<len  )		
		n=n*10+( s[i++] -'0');
	
	return n;
}

int send_error_reply(struct sip_msg* msg, int reply_code, str reply_str)
{
    str hdr_append;
    char buffer[256];
    int i;
    pres_ev_t* ev= EvList->events;

    if(reply_code== BAD_EVENT_CODE)
	{
		hdr_append.s = buffer;
		hdr_append.s[0]='\0';
		hdr_append.len = sprintf(hdr_append.s, "Allow-Events: ");
		if(hdr_append.len < 0)
		{
			LM_ERR("unsuccessful sprintf\n");
			return -1;
		}

		for(i= 0; i< EvList->ev_count; i++)
		{
			if(i> 0)
			{
				memcpy(hdr_append.s+ hdr_append.len, ", ", 2);
				hdr_append.len+= 2;
			}	
			memcpy(hdr_append.s+ hdr_append.len, ev->name.s, ev->name.len );
			hdr_append.len+= ev->name.len ;
			ev= ev->next;
		}
		memcpy(hdr_append.s+ hdr_append.len, CRLF, CRLF_LEN);
		hdr_append.len+=  CRLF_LEN;
		hdr_append.s[hdr_append.len]= '\0';
		
		if (add_lump_rpl( msg, hdr_append.s, hdr_append.len, LUMP_RPL_HDR)==0 )
		{
			LM_ERR("unable to add lump_rl\n");
			return -1;
		}
    } else if(reply_code== INTERVAL_TOO_BRIEF) {
        
        hdr_append.s = buffer;
        hdr_append.s[0]='\0';
        hdr_append.len = sprintf(hdr_append.s, "Min-Expires: %d", min_expires);
        if(hdr_append.len < 0)
        {
            LM_ERR("unsuccessful sprintf\n");
            return -1;
        }
        memcpy(hdr_append.s+ hdr_append.len, CRLF, CRLF_LEN);
        hdr_append.len+=  CRLF_LEN;
        hdr_append.s[hdr_append.len]= '\0';
        
        if (add_lump_rpl( msg, hdr_append.s, hdr_append.len, LUMP_RPL_HDR)==0 )
        {
            LM_ERR("unable to add lump_rl\n");
            return -1;
        }
    }

	if (slb.freply(msg, reply_code, &reply_str) < 0)
	{
		LM_ERR("sending %d %.*s reply\n", reply_code, reply_str.len,
				reply_str.s);
		return -1;
	}
	return 0;

}

