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
#include "../../parser/parse_rr.h"
#include "../../parser/parse_uri.h"
#include "path.h"

/*
 * Combines all Path HF bodies into one string.
 */
int build_path_vector(struct sip_msg *_m, str *path)
{
	static char buf[MAX_PATH_SIZE];
	char *p;
	struct hdr_field *hdr;
	struct sip_uri puri;

	rr_t *route = 0;

	path->len = 0;
	path->s = 0;

	if(parse_headers(_m, HDR_EOH_F, 1) < 0) {
		LOG(L_ERR,"ERROR: build_path_vector(): Error while parsing message\n");
		goto error;
	}

	for( hdr=_m->path,p=buf ; hdr ; hdr=hdr->next) {
		if(hdr->type != HDR_PATH_T)
			continue;
		
		/* check for max. Path length */
		if( p-buf+hdr->body.len+1 >= MAX_PATH_SIZE) {
			LOG(L_ERR, "ERROR: build_path_vector(): Overall Path body "
				"exceeds max. length of %d\n",MAX_PATH_SIZE);
			goto error;
		}
		if(p!=buf)
			*(p++) = ',';
		memcpy( p, hdr->body.s, hdr->body.len);
		p +=  hdr->body.len;
	}

	if(p!=buf) {
		/* check if next hop is a loose router */
		if(parse_rr_body( buf, p-buf, &route) < 0) {
			LOG(L_ERR, "ERROR: build_path_vector(): Failed to parse Path "
				"body, no head found\n");
			goto error;
		}
		if (parse_uri(route->nameaddr.uri.s, route->nameaddr.uri.len, &puri) == -1) {
			LOG(L_ERR, "ERROR: build_path_vector(): Error while parsing first Path URI\n");
			return -1;
		}
		if(!puri.lr.s) {
			LOG(L_ERR, "ERROR: build_path_vector(): First Path URI is not a "
				"loose-router, not supported\n");
			free_rr(&route);
			goto error;
		}
		free_rr(&route);
	}

	path->s = buf;
	path->len = p-buf;
	return 0;
error:
	return -1;
}

/*
 * Path must be available. Function returns the first uri 
 * from Path without any dupication.
 */
int get_path_dst_uri(str *_p, str **_dst)
{
	rr_t *route = 0;

	DBG("DEBUG: get_path_dst_uri(): Path for branch: '%.*s'\n",
		_p->len, _p->s);

	if(parse_rr_body(_p->s, _p->len, &route) < 0) {
		LOG(L_ERR, "ERROR: get_path_dst_uri(): Failed to parse "
			"Path body\n");
		return -1;
	}
	if(!route) {
		LOG(L_ERR, "ERROR: get_path_dst_uri(): Failed to parse Path body,"
			" no head found\n");
		return -1;
	}

	*_dst = &route->nameaddr.uri;
	free_rr(&route);

	return 0;
}
