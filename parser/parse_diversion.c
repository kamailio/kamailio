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
 * \brief Parser :: Diversion header
 * 
 * \ingroup parser
 */

 
#include <stdlib.h>
#include <string.h> 
#include "../dprint.h"
#include "../ut.h"
#include "../mem/mem.h"
#include "parse_from.h"
#include "parse_to.h"
#include "msg_parser.h"

/*! \brief
 * This method is used to parse DIVERSION header.
 *
 * params: msg : sip msg
 * returns 0 on success,
 *        -1 on failure.
 */
int parse_diversion_header(struct sip_msg *msg)
{
 	struct to_body* diversion_b;
	
 	if (!msg->diversion && (parse_headers(msg, HDR_DIVERSION_F, 0) == -1 ||
				!msg->diversion)) {
 		goto error;
 	}
 
 	/* maybe the header is already parsed! */
 	if (msg->diversion->parsed)
 		return 0;
 
 	/* bad luck! :-( - we have to parse it */
 	/* first, get some memory */
 	diversion_b = pkg_malloc(sizeof(struct to_body));
 	if (diversion_b == 0) {
 		LOG(L_ERR, "ERROR:parse_diversion_header: out of pkg_memory\n");
 		goto error;
 	}
 
 	/* now parse it!! */
 	memset(diversion_b, 0, sizeof(struct to_body));
 	parse_to(msg->diversion->body.s, msg->diversion->body.s + msg->diversion->body.len + 1, diversion_b);
 	if (diversion_b->error == PARSE_ERROR) {
 		LOG(L_ERR, "ERROR:parse_diversion_header: bad diversion header\n");
 		free_to(diversion_b);
 		goto error;
 	}
 	msg->diversion->parsed = diversion_b;
	
 	return 0;
 error:
 	return -1;
}


/*! \brief
 * Get the value of a given diversion parameter
 */
str *get_diversion_param(struct sip_msg *msg, str* name)
{
    struct to_param *params;

    if (parse_diversion_header(msg) < 0) {
		ERR("could not get diversion parameter\n");
		return 0;
    }

    params =  ((struct to_body*)(msg->diversion->parsed))->param_lst;

    while (params) {
		if ((params->name.len == name->len) &&
			(strncmp(params->name.s, name->s, name->len) == 0)) {
			return &params->value;
		}
		params = params->next;
    }
	
    return 0;
}
