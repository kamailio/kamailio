/*
 * $Id$
 *
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
 * \brief P-Preferred-Identity header parser
 * \ingroup parser
 */

#include "parse_ppi.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h"
#include <stdlib.h>
#include <string.h>
#include "../../dprint.h"
#include "../../parser/msg_parser.h"
#include "../../ut.h"
#include "errinfo.h"
#include "../../mem/mem.h"


/*!
 * \brief This method is used to parse P-Preferred-Identity header (RFC 3325).
 *
 * Currently only one name-addr / addr-spec is supported in the header
 * and it must contain a sip or sips URI.
 * \param msg sip msg
 * \return 0 on success, -1 on failure.
 */
int parse_ppi_header( struct sip_msg *msg )
{
    struct to_body* ppi_b;

    if ( !msg->ppi &&
	 (parse_headers(msg, HDR_PPI_F,0)==-1 || !msg->ppi)) {
	goto error;
    }
 
    /* maybe the header is already parsed! */
    if (msg->ppi->parsed)
	return 0;
 
    /* bad luck! :-( - we have to parse it */
    /* first, get some memory */
    ppi_b = pkg_malloc(sizeof(struct to_body));
    if (ppi_b == 0) {
	LM_ERR("out of pkg_memory\n");
	goto error;
    }
 
    /* now parse it!! */
    memset(ppi_b, 0, sizeof(struct to_body));
    parse_to(msg->ppi->body.s,
	     msg->ppi->body.s + msg->ppi->body.len+1,
	     ppi_b);
    if (ppi_b->error == PARSE_ERROR) {
	LM_ERR("bad P-Preferred-Identity header\n");
	free_to(ppi_b);
	goto error;
    }
 	msg->ppi->parsed = ppi_b;
 
 	return 0;
 error:
 	return -1;
}


/*!
 * \brief Parse P-Preferred-Identity header URI
 */
struct sip_uri *parse_ppi_uri(struct sip_msg *msg)
{
	struct to_body *tb = NULL;
	
	if(msg==NULL)
		return NULL;

	if(parse_ppi_header(msg)<0)
	{
		LM_ERR("cannot parse P-P-I header\n");
		return NULL;
	}
	
	if(msg->ppi==NULL || get_ppi(msg)==NULL)
		return NULL;

	tb = get_ppi(msg);

	if(tb->parsed_uri.user.s!=NULL || tb->parsed_uri.host.s!=NULL)
		return &tb->parsed_uri;

	if (parse_uri(tb->uri.s, tb->uri.len , &tb->parsed_uri)<0)
	{
		LM_ERR("failed to parse P-P-I URI\n");
		memset(&tb->parsed_uri, 0, sizeof(struct sip_uri));
		set_err_info(OSER_EC_PARSER, OSER_EL_MEDIUM, "error parsing P-P-I URI");
		set_err_reply(400, "bad P-Preferred-Identity uri");
		return NULL;
	}

	return &tb->parsed_uri;
}
