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
 *
 * Defines:
 *  BRANCH_INCLUDE_FROMTO_BODY - if defined the old (pre 3.1) mode of
 *   including the full from & to bodies will be used (instead of only the
 *   tags).
 * BRANCH_IGNORE_3261_VIA - if defined, no check and special/simpler handling
 *   of messages with 3261 cookies in the via branch will be made (same
 *   behaviour as in pre 3.1 versions).
*/

#ifndef __char_msg_val_h
#define __char_msg_val_h

#include "comp_defs.h"
#include "compiler_opt.h"
#include "str.h"
#include "parser/msg_parser.h"
#include "parser/parse_to.h"
#include "parser/parse_from.h"
#include "md5utils.h"

/*! \brief calculate characteristic value of a message -- this value
   is used to identify a transaction during the process of
   reply matching
 */
inline static int char_msg_val( struct sip_msg *msg, char *cv )
{
	str src[8];

	if (unlikely(!check_transaction_quadruple(msg))) {
		LM_ERR("can't calculate char_value due to a parsing error\n");
		memset( cv, '0', MD5_LEN );
		return 0;
	}
#ifndef BRANCH_IGNORE_3261_VIA
	if (likely(msg->via1->branch && msg->via1->branch->value.len>MCOOKIE_LEN &&
				memcmp(msg->via1->branch->value.s, MCOOKIE, MCOOKIE_LEN)==0)){
		/* 3261 branch cookie present => hash only the received branch ID */
		src[0]=msg->via1->branch->value;
		MD5StringArray ( cv, src, 1 );
		return 1; /* success */
	}
#endif /* BRANCH_IGNORE_3261_VIA */
#ifdef BRANCH_INCLUDE_FROMTO_BODY
	/* use the from & to full bodies */
	src[0]= msg->from->body;
	src[1]= msg->to->body;
#else
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
	src[1]=get_to(msg)->tag_value;
#endif /* BRANCH_INCLUDE_FROMTO_BODY */
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
