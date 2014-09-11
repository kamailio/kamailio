/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * History:
 * ----------
 * 2003-02-28 scratchpad compatibility abandoned (jiri)
 * 2002-01-28 scratchpad removed (jiri)
 * 2004-08-15 max value of max-fwd header is configurable (bogdan)
 * 2005-11-03 MF value saved in msg->maxforwards->parsed (bogdan)
 */


#include <stdlib.h>
#include <string.h>

#include "mf_funcs.h"
#include "../../mem/mem.h"
#include "../../ut.h"
#include "../../data_lump.h"


#define MF_HDR "Max-Forwards: "
#define MF_HDR_LEN (sizeof(MF_HDR) - 1)

/* do a tricky thing and keep the parsed value of MAXFWD hdr incremented 
 * by one in order to make difference between 0 (not set)
 * and 0 (zero value) - bogdan */
#define IS_MAXWD_STORED(_msg_) \
	((_msg_)->maxforwards->parsed)
#define STORE_MAXWD_VAL(_msg_,_val_) \
	(_msg_)->maxforwards->parsed = ((void*)(long)((_val_)+1))
#define FETCH_MAXWD_VAL(_msg_) \
	(((int)(long)(_msg_)->maxforwards->parsed)-1)

/* looks for the MAX FORWARDS header
   returns its value, -1 if is not present or -2 for error */
int is_maxfwd_present( struct sip_msg* msg , str *foo)
{
	int x, err;

	/* lookup into the message for MAX FORWARDS header*/
	if ( !msg->maxforwards ) {
		if  ( parse_headers( msg , HDR_MAXFORWARDS_F, 0 )==-1 ){
			LM_ERR("parsing MAX_FORWARD header failed!\n");
			return -2;
		}
		if (!msg->maxforwards) {
			LM_DBG("max_forwards header not found!\n");
			return -1;
		}
	} else if (IS_MAXWD_STORED(msg)) {
		trim_len( foo->len , foo->s , msg->maxforwards->body );
		return FETCH_MAXWD_VAL(msg);
	}

	/* if header is present, trim to get only the string containing numbers */
	trim_len( foo->len , foo->s , msg->maxforwards->body );

	/* convert from string to number */
	x = str2s( foo->s,foo->len,&err);
	if (err){
		LM_ERR("unable to parse the max forwards number\n");
		return -2;
	}
	/* store the parsed values */
	STORE_MAXWD_VAL(msg, x);
	LM_DBG("value = %d \n",x);
	return x;
}



int decrement_maxfwd( struct sip_msg* msg , int x, str *s)
{
	int i;

	/* decrement the value */
	x--;

	/* update the stored value */
	STORE_MAXWD_VAL(msg, x);

	/*rewriting the max-fwd value in the message (buf and orig)*/
	for(i = s->len - 1; i >= 0; i--) {
		s->s[i] = (x % 10) + '0';
		x /= 10;
		if (x==0) {
			i = i - 1;
			break;
		}
	}
	while(i >= 0) s->s[i--] = ' ';

	return 0;
}

int add_maxfwd_header( struct sip_msg* msg , unsigned int val )
{
	unsigned int  len;
	char          *buf;
	struct lump*  anchor;

	/* constructing the header */
	len = MF_HDR_LEN /*"MAX-FORWARDS: "*/+ CRLF_LEN + 3/*val max on 3 digits*/;

	buf = (char*)pkg_malloc( len );
	if (!buf) {
		LM_ERR("add_maxfwd_header: no more pkg memory\n");
		goto error;
	}
	memcpy( buf , MF_HDR, MF_HDR_LEN );
	len = MF_HDR_LEN ;
	len += btostr( buf+len , val );
	memcpy( buf+len , CRLF , CRLF_LEN );
	len +=CRLF_LEN;

	/*inserts the header at the beginning of the message*/
	anchor = anchor_lump(msg, msg->headers->name.s - msg->buf, 0 , 0);
	if (anchor == 0) {
		LM_ERR("add_maxfwd_header: failed to get anchor\n");
		goto error1;
	}

	if (insert_new_lump_before(anchor, buf, len, 0) == 0) {
		LM_ERR("add_maxfwd_header: failed to insert MAX-FORWARDS lump\n");
		goto error1;
	}

	return 0;
error1:
	pkg_free( buf );
error:
	return -1;
}
