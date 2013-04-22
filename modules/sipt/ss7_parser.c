/*
 *
 * Copyright (C) 2013 Voxbone SA
 * 
 * Parsing code derrived from libss7 Copyright (C) Digium
 *
 *
 * This file is part of SIP-Router, a free SIP server.
 *
 * SIP-Router is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * SIP-Router is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * 
 */

#include "ss7.h"
#include <string.h>
#include <stddef.h>

static char char2digit(char localchar)
{
	switch (localchar) {
		case '0':
			return 0;
		case '1':
			return 1;
		case '2':
			return 2;
		case '3':
			return 3;
		case '4':
			return 4;
		case '5':
			return 5;
		case '6':
			return 6;
		case '7':
			return 7;
		case '8':
			return 8;
		case '9':
			return 9;
		case 'A':
			return 0xa;
		case 'B':
			return 0xb;
		case 'C':
			return 0xc;
		case 'D':
			return 0xd;
		case '*':
			return 0xe;
		case '#':
		case 'F':
			return 0xf;
		default:
			return 0;
	}
}

static void isup_put_number(unsigned char *dest, char *src, int *len, int *oddeven)
{
	int i = 0;
	int numlen = strlen(src);

	if (numlen % 2) {
		*oddeven = 1;
		*len = numlen/2 + 1;
	} else {
		*oddeven = 0;
		*len = numlen/2;
	}

	while (i < numlen) {
		if (!(i % 2))
			dest[i/2] |= char2digit(src[i]) & 0xf;
		else
		{
			dest[i/2] |= (char2digit(src[i]) << 4) & 0xf0;
		}
		i++;
	}
}

static int encode_called_party(char * number, unsigned char * flags, int nai, unsigned char * buf, int len)
{
	int numlen, oddeven;
	buf[0] = flags[0]&0x7F;
	buf[1] = flags[1];

	isup_put_number(&buf[2], number, &numlen, &oddeven);

	if(oddeven)
	{
		buf[0] |= 0x80;
	}
	
	if(nai)
	{
		buf[0] &= 0x80;
		buf[0] = (unsigned char)(nai&0x7F);
	}

	return numlen + 2;
}

static int encode_calling_party(char * number, int nai, int presentation, int screening, unsigned char * buf, int len) 
{
        int oddeven, datalen;

        if (!number[0] && presentation != SS7_PRESENTATION_ADDR_NOT_AVAILABLE)
                return 0;

        if (number[0] && presentation != SS7_PRESENTATION_ADDR_NOT_AVAILABLE)
	{
                isup_put_number(&buf[2], number, &datalen, &oddeven);
	}
        else 
	{
                datalen = 0;
                oddeven = 0;
                nai = 0;
        }

        buf[0] = (oddeven << 7) | nai;      /* Nature of Address Indicator */
         /* Assume E.164 ISDN numbering plan, calling number complete */
        buf[1] = ((presentation == SS7_PRESENTATION_ADDR_NOT_AVAILABLE) ? 0 : (1 << 4)) |
                ((presentation & 0x3) << 2) |
                (screening & 0x3);

        return datalen + 2;
}

// returns start of specified optional header of IAM, otherwise return -1
static int get_optional_header(unsigned char header, unsigned char *buf, int len)
{
	struct isup_iam_fixed * message = (struct isup_iam_fixed*)buf;
	int offset = 0;
	int res;

	// not an iam? do nothing
	if(message->type != ISUP_IAM)
	{
		return -1;
	}

	len -= offsetof(struct isup_iam_fixed, optional_pointer);
	offset += offsetof(struct isup_iam_fixed, optional_pointer);

	if (len < 1)
		return -1;

	offset += message->optional_pointer;
	len -= message->optional_pointer;

	if (len < 1 )
		return -1;

	/* Optional paramter parsing code */
	if (message->optional_pointer) {
		while ((len > 0) && (buf[offset] != 0)) {
			struct isup_parm_opt *optparm = (struct isup_parm_opt *)(buf + offset);

			res = optparm->len+2;
			if(optparm->type == header)
			{
				return offset;
			}

			len -= res;
			offset += res;
		}
	}
	return -1;
}

int isup_get_hop_counter(unsigned char *buf, int len)
{
	int  offset = get_optional_header(ISUP_PARM_HOP_COUNTER, buf, len);

	if(offset != -1 && len-offset-2 > 0)
	{
		return buf[offset+2] & 0x1F;
	}
	return -1;
}

int isup_get_cpc(unsigned char *buf, int len)
{
	struct isup_iam_fixed * message = (struct isup_iam_fixed*)buf;

	// not an iam? do nothing
	if(message->type != ISUP_IAM)
	{
		return -1;
	}

	/* Message Type = 1 */
	len -= offsetof(struct isup_iam_fixed, calling_party_category);

	if (len < 1)
		return -1;

	return (int)message->calling_party_category;
}


int isup_get_calling_party_nai(unsigned char *buf, int len)
{
	int  offset = get_optional_header(ISUP_PARM_CALLING_PARTY_NUM, buf, len);

	if(offset != -1 && len-offset-2 > 0)
	{
		return buf[offset+2] & 0x7F;
	}
	return -1;
}

int isup_get_called_party_nai(unsigned char *buf, int len)
{
	struct isup_iam_fixed * message = (struct isup_iam_fixed*)buf;

	// not an iam? do nothing
	if(message->type != ISUP_IAM)
	{
		return -1;
	}

	/* Message Type = 1 */
	len -= offsetof(struct isup_iam_fixed, called_party_number);

	if (len < 1)
		return -1;
	return message->called_party_number[1]&0x7F;
}

int isup_update_destination(char * dest, int hops, int nai, unsigned char *buf, int len, unsigned char * obuf, int olen)
{
	int offset = 0, offset2 = 0;
	int res;
	char optparams;
	unsigned char *param_pointer = NULL;


	// not an iam? do nothing
	if(buf[0] != ISUP_IAM)
	{
		int l = len > olen ? olen : len;
		memcpy((void*)obuf, (void *)(buf), l);
		return l;
	}

	memset(obuf, 0, olen);

	// bounds checking
	if(hops > 31)
	{
		hops = 31;
	}



	/* Copy the fixed parms */
	memcpy((void*)obuf, (void *)(buf), 6);
	len -= 6;
	olen -= 6;
	offset += 6;
	offset2 += 6;

	if (len < 1)
		return -1;

	/* IAM has one Fixed variable param, Called party number, we need to modify this */

	// pointer to fixed part (2)
	obuf[offset2++]=buf[offset++];


	//pointer to optional part (to update later)
	param_pointer = (obuf+offset2);
	optparams = buf[offset];
	offset2++;
	offset++;
	len--;
	olen--;


	// add the new mandatory fixed header
	res = encode_called_party(dest, buf+offset+1, nai, obuf+offset2+1, olen-1);
	obuf[offset2] = (char)res;

	offset2 += res+1;
	olen -= res+1;
	
	res = buf[offset];
	offset += res+1;
	len -= res+1;


	
	if (len < 1 )
		return -1;


	
	// set the pointer to the optional part
	param_pointer[0] = (char)((obuf+offset2) - param_pointer);

	/* Optional paramter parsing code */
	if (optparams) {

		bool has_hops = 0;
		
		while ((len > 0) && (buf[offset] != 0)) {
			struct isup_parm_opt *optparm = (struct isup_parm_opt *)(buf + offset);


			res = optparm->len+2;
			switch(optparm->type)
			{
				case ISUP_PARM_HOP_COUNTER:
					obuf[offset2++] = ISUP_PARM_HOP_COUNTER;
					obuf[offset2++] = 1;
					obuf[offset2++] = ((optparm->data[0]&0x1F)-1)&0x1F;
					has_hops = 1;
					break;
				default:
					memcpy((void*)(obuf+offset2), (void *)(buf + offset), res);
					offset2 += res;
			}

			len -= res;
			offset += res;
		}

		// add missing headers
		if(!has_hops)
		{
			obuf[offset2++] = ISUP_PARM_HOP_COUNTER;
			obuf[offset2++] = 1;
			obuf[offset2++] = hops & 0x1F;
			has_hops = 1;
		}

		// nicely null terminate it
		obuf[offset2++] = 0;
	}

	return offset2;
}

int isup_update_calling(char * origin, int nai, int presentation, int screening, unsigned char * buf, int len, unsigned char * obuf, int olen)
{
	int offset = 0, offset2 = 0;
	int res;
	struct isup_iam_fixed * message = (struct isup_iam_fixed*)obuf;
	struct isup_iam_fixed * orig_message = (struct isup_iam_fixed*)buf;

	// not an iam? do nothing
	if(orig_message->type != ISUP_IAM)
	{
		int l = len > olen ? olen : len;
		memcpy((void*)obuf, (void *)(buf), l);
		return l;
	}

	/* Message Type = 1 */
	memset(obuf, 0, olen);

	/* Copy the fixed parms */
	memcpy((void*)obuf, (void *)(buf), offsetof(struct isup_iam_fixed, called_party_number));
	len -= offsetof(struct isup_iam_fixed, called_party_number);
	olen -= offsetof(struct isup_iam_fixed, called_party_number);
	offset += offsetof(struct isup_iam_fixed, called_party_number);
	offset2 += offsetof(struct isup_iam_fixed, called_party_number);

	if (len < 1)
		return -1;


	/* IAM has one Fixed variable param, Called party number, we need to modify this */


	// add the new mandatory fixed header
	res = buf[offset];
	memcpy((void*)(obuf+offset2), (void *)(buf+offset), res+1);
	offset += res+1;
	offset2 += res+1;
	len -= res+1;
	olen -= res+1;


	
	if (len < 1 )
		return -1;


	
	// set the pointer to the optional part (there will be at least one optional header when we are done)
	message->optional_pointer = (char)(offset2 - offsetof(struct isup_iam_fixed, optional_pointer));

	/* Optional paramter parsing code */
	if (orig_message->optional_pointer) {

		bool has_calling = 0;
		
		while ((len > 0) && (buf[offset] != 0)) {
			int res2 = 0;
			struct isup_parm_opt *optparm = (struct isup_parm_opt *)(buf + offset);


			res = optparm->len+2;
			switch(optparm->type)
			{
				case ISUP_PARM_CALLING_PARTY_NUM:
					obuf[offset2++] = ISUP_PARM_CALLING_PARTY_NUM;
					res2 = encode_calling_party(origin, nai, presentation, screening, obuf+offset2+1, olen-1);
					obuf[offset2] = (char)res2;

					offset2 += res2+1;
					olen -= res2+1;
					has_calling = 1;
					break;
				default:
					memcpy((void*)(obuf+offset2), (void *)(buf + offset), res);
					offset2 += res;
			}

			len -= res;
			offset += res;
		}

		// add missing headers
		if(!has_calling)
		{
			obuf[offset2++] = ISUP_PARM_CALLING_PARTY_NUM;
			res = encode_calling_party(origin, nai, presentation, screening, obuf+offset2+1, olen-1);
			obuf[offset2] = (char)res;

			offset2 += res+1;
			olen -= res+1;
			has_calling = 1;
		}

		// nicely null terminate it
		obuf[offset2++] = 0;
	}

	return offset2;
}
