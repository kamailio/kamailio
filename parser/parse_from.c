/*
 * @author Stelios Sidiroglou-Douskos <ssi@fokus.gmd.de>
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 * This method is used to parse the from header. It was decided not to parse
 * anything in core that is not *needed* so this method gets called by 
 * rad_acc module and any other modules that needs the FROM header.
 *
 * params: hdr : Hook to the from header
 * returns 0 on success,
 *		   -1 on failure.
 */
int parse_from_header(struct hdr_field* hdr) 
{
	struct to_body* from_b;
	
	from_b = pkg_malloc(sizeof(struct to_body));
	if (from_b == 0) {
		LOG(L_ERR, "parse_from_header: out of memory\n");
		goto error;
	}
			
	memset(from_b, 0, sizeof(struct to_body));
	parse_to(hdr->body.s, hdr->body.s + hdr->body.len + 1, from_b);
	if (from_b->error == PARSE_ERROR) {
		LOG(L_ERR, "ERROR: parse_from_header: bad from header\n");
		pkg_free(from_b);
		goto error;
	}
	hdr->parsed = from_b;	
	DBG("DEBUG: parse_from_header: <%s> [%d]; uri=[%.*s] \n",
		hdr->name.s, hdr->body.len, from_b->uri.len, from_b->uri.s);
	DBG("DEBUG: from body [%.*s]\n",from_b->body.len, from_b->body.s);	

	return 0;

	error:
	/* more debugging, msg->orig is/should be null terminated*/
	LOG(L_ERR, "ERROR: parse_from_header: \n");
	return -1;

}


