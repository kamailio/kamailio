/*
 * $Id$
 *
 * Helper functions for Path support.
 *
 * Copyright (C) 2006 Andreas Granig <agranig@linguin.org>
 *
 * This file is part of openser, a free SIP server.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "../../data_lump.h"
#include "../../mem/mem.h"
#include "path.h"

#define ROUTE_STR  "Route: "
#define ROUTE_LEN  (sizeof(ROUTE_STR)-1)

/*
 * Save given Path body as Route header in message.
 * 
 * If another Route HF is found, it's placed right before that. 
 * Otherwise, it's placed after the last Via HF. If also no 
 * Via HF is found, it's placed as first HF.
 */
int insert_path_as_route(struct sip_msg* msg, str* path)
{
	struct lump *anchor;
	char *route;
	struct hdr_field *hf, *last_via;

	for (hf = msg->headers; hf; hf = hf->next) {
		if (hf->type == HDR_ROUTE_T) {
			break;
		} else if (hf->type == HDR_VIA_T) {
			last_via = hf;
		}
	}
	if (hf) {
		/* Route HF found, insert before it */
		anchor = anchor_lump(msg, hf->name.s - msg->buf, 0, 0);
	} else if(last_via) {
		if (last_via->next) {
			/* Via HF in between, insert after it */
			anchor = anchor_lump(msg, hf->name.s + hf->len - msg->buf, 0, 0);
		} else {
			/* Via HF is last, so append */
			anchor = anchor_lump(msg, msg->unparsed - msg->buf, 0, 0);
		}
	} else {
		/* None of the above, insert as first */
		anchor = anchor_lump(msg, msg->headers->name.s - msg->buf, 0, 0);
	}

	if (anchor == 0) {
		LOG(L_ERR, "ERROR: insert_path_as_route(): Failed to get anchor\n");
		return -1;
	}

	route = pkg_malloc(ROUTE_LEN + path->len + CRLF_LEN);
	if (!route) {
		LOG(L_ERR, "ERROR: insert_path_as_route(): Out of memory\n");
		return -1;
	}
	memcpy(route, ROUTE_STR, ROUTE_LEN);
	memcpy(route + ROUTE_LEN, path->s, path->len);
	memcpy(route + ROUTE_LEN + path->len, CRLF, CRLF_LEN);

	if (insert_new_lump_before(anchor, route, ROUTE_LEN + path->len + CRLF_LEN, 0) == 0) {
		LOG(L_ERR, "ERROR: insert_path_as_route(): Failed to insert lump\n");
		return -1;
	}

	return 0;
}
