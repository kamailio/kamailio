/*
 * Copyright (C) 2001-2003 Juha Heinanen
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
 
#include "parse_from.h"
#include "parse_to.h"
#include <stdlib.h>
#include <string.h>
#include "../dprint.h"
#include "msg_parser.h"
#include "../ut.h"
#include "../mem/mem.h"

 
/*
 * This method is used to parse RPID header.
 *
 * params: msg : sip msg
 * returns 0 on success,
 *        -1 on failure.
 */
int parse_rpid_header( struct sip_msg *msg )
{
	struct to_body* rpid_b;
	
 	if ( !msg->rpid && (parse_headers(msg,HDR_RPID,0)==-1 || !msg->rpid)) {
 		goto error;
 	}
 
 	/* maybe the header is already parsed! */
 	if (msg->rpid->parsed)
 		return 0;
 
 	/* bad luck! :-( - we have to parse it */
 	/* first, get some memory */
 	rpid_b = pkg_malloc(sizeof(struct to_body));
 	if (rpid_b == 0) {
 		LOG(L_ERR, "ERROR:parse_rpid_header: out of pkg_memory\n");
 		goto error;
 	}
 
 	/* now parse it!! */
 	memset(rpid_b, 0, sizeof(struct to_body));
 	parse_to(msg->rpid->body.s,msg->rpid->body.s+msg->rpid->body.len+1,rpid_b);
 	if (rpid_b->error == PARSE_ERROR) {
 		LOG(L_ERR, "ERROR:parse_rpid_header: bad rpid header\n");
 		pkg_free(rpid_b);
 		goto error;
 	}
 	msg->rpid->parsed = rpid_b;
 
 	return 0;
 error:
 	return -1;
}
