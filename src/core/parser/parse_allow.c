/*
 * Copyright (c) 2004 Juha Heinanen
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
 * \brief Parser :: Allow header
 *
 * \ingroup parser
 */


#include <stdlib.h>
#include <string.h>
#include "../dprint.h"
#include "../mem/mem.h"
#include "parse_allow.h"
#include "parse_methods.h"
#include "msg_parser.h"

 
/*! \brief
 * This method is used to parse Allow header.
 *
 * \param _hf message header field
 * \return 0 on success, -1 on failure.
 */
int parse_allow_header(struct hdr_field* _hf)
{
	struct allow_body* ab = 0;

	if (!_hf) {
		LOG(L_ERR, "parse_allow_header: Invalid parameter value\n");
		return -1;
	}
	
	/* maybe the header is already parsed! */
 	if (_hf->parsed) {
 		return 0;
	}

	ab = (struct allow_body*)pkg_malloc(sizeof(struct allow_body));
	if (ab == 0) {
		LOG(L_ERR, "ERROR:parse_allow_header: out of pkg_memory\n");
		return -1;
	}
	memset(ab,'\0', sizeof(struct allow_body));
	
	if (parse_methods(&(_hf->body), &(ab->allow)) !=0 ) {
		LOG(L_ERR, "ERROR:parse_allow_header: bad allow body header\n");		
		goto error;
	}
	
	ab->allow_all = 0;	
	_hf->parsed = (void*)ab;
 	return 0;

error:
	if (ab) pkg_free(ab);
	return -1;
}

/*!
 * \brief This method is used to parse all Allow HF body.
 * \param msg sip msg
 * \return 0 on success,-1 on failure.
 */
int parse_allow(struct sip_msg *msg)
{       
	unsigned int allow;
	struct hdr_field  *hdr;

	/* maybe the header is already parsed! */
	if (msg->allow && msg->allow->parsed) {
		return 0;
	}

	/* parse to the end in order to get all ALLOW headers */
	if (parse_headers(msg,HDR_EOH_F,0)==-1 || !msg->allow) {
		return -1;
	}
	allow = 0;

	for(hdr = msg->allow ; hdr ; hdr = next_sibling_hdr(hdr)) {
		if (hdr->parsed == 0) {
			if(parse_allow_header(hdr) < 0) {
				return -1;
			}
		}

		allow |= ((struct allow_body*)hdr->parsed)->allow;
	}
	
	((struct allow_body*)msg->allow->parsed)->allow_all = allow;
    return 0;
}


/*
 * Release memory
 */
void free_allow_body(struct allow_body **ab)
{
	if (ab && *ab) {	
		pkg_free(*ab);		
		*ab = 0;
	}
}


void free_allow_header(struct hdr_field* hf)
{
	free_allow_body((struct allow_body**)(void*)(&(hf->parsed)));
}
