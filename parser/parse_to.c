/*
 * Copyright (C) 2001-2003 Fhg Fokus
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

/** Parser :: Parse To: header.
 * @file
 * @ingroup parser
 */

#include "parse_to.h"
#include "parse_addr_spec.h"
#include <stdlib.h>
#include <string.h>
#include "../dprint.h"
#include "msg_parser.h"
#include "parse_uri.h"
#include "../ut.h"
#include "../mem/mem.h"


char* parse_to(char* const buffer, const char* const end, struct to_body* const to_b)
{
	return parse_addr_spec(buffer, end, to_b, 0);
}


int parse_to_header(struct sip_msg* const msg)
{
	if ( !msg->to && ( parse_headers(msg,HDR_TO_F,0)==-1 || !msg->to)) {
		ERR("bad msg or missing TO header\n");
		return -1;
	}

	// HDR_TO_T is automatically parsed (get_hdr_field in parser/msg_parser.c)
	// so check only ptr validity
	if (msg->to->parsed)
		return 0;
	else
		return -1;
}

sip_uri_t *parse_to_uri(sip_msg_t* const msg)
{
	to_body_t *tb = NULL;
	
	if(msg==NULL)
		return NULL;

	if(parse_to_header(msg)<0)
	{
		LM_ERR("cannot parse TO header\n");
		return NULL;
	}

	if(msg->to==NULL || get_to(msg)==NULL)
		return NULL;

	tb = get_to(msg);
	
	if(tb->parsed_uri.user.s!=NULL || tb->parsed_uri.host.s!=NULL)
		return &tb->parsed_uri;

	if (parse_uri(tb->uri.s, tb->uri.len , &tb->parsed_uri)<0)
	{
		LM_ERR("failed to parse To uri\n");
		memset(&tb->parsed_uri, 0, sizeof(struct sip_uri));
		return NULL;
	}

	return &tb->parsed_uri;
}
