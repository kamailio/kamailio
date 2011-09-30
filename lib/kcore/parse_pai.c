/*
 * Copyright (C) 2006 Juha Heinanen
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*!
 * \file
 * \brief P-Asserted-Identity header parser
 * \ingroup parser
 */

#include "../../parser/parse_from.h"
#include "../../parser/parse_to.h"
#include <stdlib.h>
#include <string.h>
#include "../../dprint.h"
#include "../../parser/msg_parser.h"
#include "../../ut.h"
#include "../../mem/mem.h"

/*!
 * This method is used to parse P-Asserted-Identity header (RFC 3325).
 *
 * Currently only one name-addr / addr-spec is supported in the header
 * and it must contain a sip or sips URI.
 * \param msg sip msg
 * \return 0 on success, -1 on failure.
 */
int parse_pai_header( struct sip_msg *msg )
{
	struct to_body* pai_b;

	if ( !msg->pai && (parse_headers(msg, HDR_PAI_F,0)==-1 || !msg->pai)) {
		goto error;
	}

	/* maybe the header is already parsed! */
	if (msg->pai->parsed)
		return 0;
 
	/* bad luck! :-( - we have to parse it */
	/* first, get some memory */
	pai_b = pkg_malloc(sizeof(struct to_body));
	if (pai_b == 0) {
		LM_ERR("out of pkg_memory\n");
		goto error;
	}
 
	/* now parse it!! */
	memset(pai_b, 0, sizeof(struct to_body));
	parse_to(msg->pai->body.s, msg->pai->body.s + msg->pai->body.len+1, pai_b);
	if (pai_b->error == PARSE_ERROR) {
		LM_ERR("bad P-Asserted-Identity header\n");
		free_to(pai_b);
		goto error;
	}
 	msg->pai->parsed = pai_b;
 
	return 0;
error:
	return -1;
}
