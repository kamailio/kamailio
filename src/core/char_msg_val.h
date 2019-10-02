/* 
 * Copyright (C) 2010 iptelorg GmbH
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

/** compute the characteristic value of a message.
 * @file
 * @ingroup core
 * \author andrei
*/

#ifndef __char_msg_val_h
#define __char_msg_val_h

#include "comp_defs.h"
#include "compiler_opt.h"
#include "str.h"
#include "parser/msg_parser.h"
#include "parser/parse_to.h"
#include "parser/parse_from.h"
#include "crypto/md5utils.h"

/*! \brief calculate characteristic value of a message -- this value
   is used to identify a transaction during the process of
   reply matching
 */
inline static int char_msg_val( struct sip_msg *msg, char *cv )
{
	str src[8];
	str sempty = str_init("");

	if (unlikely(!check_transaction_quadruple(msg))) {
		LM_ERR("can't calculate char_value due to a parsing error\n");
		memset( cv, '0', MD5_LEN );
		return 0;
	}
	/* to body is automatically parsed (via check_transactionquadruple / 
	   parse_header), but the from body has to be parsed manually */
	if (msg->from->parsed==0){
		/* parse from body */
		if (unlikely(parse_from_header(msg) == -1)){
			LM_ERR("error while parsing From header\n");
			return 0;
		}
	}
	/* use only the from & to tags */
	src[0]=get_from(msg)->tag_value;
	if(msg->first_line.u.request.method_value
			& (METHOD_INVITE|METHOD_ACK|METHOD_CANCEL)) {
		src[1]=sempty;
	} else {
		src[1]=get_to(msg)->tag_value;
	}
	src[2]= msg->callid->body;
	src[3]= msg->first_line.u.request.uri;
	src[4]= get_cseq( msg )->number;

	/* topmost Via is part of transaction key as well ! */
	src[5]= msg->via1->host;
	src[6]= msg->via1->port_str;
	if (likely(msg->via1->branch)) {
		src[7]= msg->via1->branch->value;
		MD5StringArray ( cv, src, 8 );
	} else {
		MD5StringArray( cv, src, 7 );
	}
	return 1;
}



#endif /*__char_msg_val_h*/

/* vi: set ts=4 sw=4 tw=79:ai:cindent: */
