/*
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
 */

/*! \file
 * \brief Parser :: SIP From header parsing
 *
 * \ingroup parser
 */

#include "parse_from.h"
#include "parse_to.h"
#include <stdlib.h>
#include <string.h>
#include "../dprint.h"
#include "msg_parser.h"
#include "parse_uri.h"
#include "../ut.h"
#include "../mem/mem.h"

/*! \brief
 * This method is used to parse the from header.
 *
 * \note It was decided not to parse
 * anything in core that is not *needed* so this method gets called by 
 * rad_acc module and any other modules that needs the FROM header.
 *
 * params: msg : sip msg
 * returns 0 on success,
 *        -1 on failure.
 */
int parse_from_header( struct sip_msg *msg)
{
	struct to_body* from_b;

	if ( !msg->from && ( parse_headers(msg,HDR_FROM_F,0)==-1 || !msg->from)) {
		LOG(L_ERR,"ERROR:parse_from_header: bad msg or missing FROM header\n");
		goto error;
	}

	/* maybe the header is already parsed! */
	if (msg->from->parsed)
		return 0;

	/* bad luck! :-( - we have to parse it */
	/* first, get some memory */
	from_b = pkg_malloc(sizeof(struct to_body));
	if (from_b == 0) {
		LOG(L_ERR, "ERROR:parse_from_header: out of pkg_memory\n");
		goto error;
	}

	/* now parse it!! */
	memset(from_b, 0, sizeof(struct to_body));
	parse_to(msg->from->body.s,msg->from->body.s+msg->from->body.len+1,from_b);
	if (from_b->error == PARSE_ERROR) {
		LOG(L_ERR, "ERROR:parse_from_header: bad from header [%.*s]\n",
				msg->from->body.len, msg->from->body.s);
		free_to(from_b);
		goto error;
	}
	msg->from->parsed = from_b;

	return 0;
error:
	return -1;
}

sip_uri_t *parse_from_uri(sip_msg_t *msg)
{
	to_body_t *tb = NULL;
        
	if(msg==NULL)
		return NULL;

	if(parse_from_header(msg)<0)
	{
		LM_ERR("cannot parse FROM header\n");
		return NULL;
	}
	
	if(msg->from==NULL || get_from(msg)==NULL)
		return NULL;

	tb = get_from(msg);
	
	if(tb->parsed_uri.user.s!=NULL || tb->parsed_uri.host.s!=NULL)
		return &tb->parsed_uri;
	
	if (parse_uri(tb->uri.s, tb->uri.len , &tb->parsed_uri)<0)
	{
		LM_ERR("failed to parse From uri\n");
		memset(&tb->parsed_uri, 0, sizeof(struct sip_uri));
		return NULL;
	}
	return &tb->parsed_uri;
}
