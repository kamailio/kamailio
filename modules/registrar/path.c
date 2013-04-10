/*
 * $Id$
 *
 * Helper functions for Path support.
 *
 * Copyright (C) 2006 Andreas Granig <agranig@linguin.org>
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
 *
 */
/*!
 * \file
 * \brief SIP registrar module - Helper functions for Path support
 * \ingroup registrar   
 */  


#include "../../data_lump.h"
#include "../../parser/parse_rr.h"
#include "../../parser/parse_uri.h"
#include "path.h"
#include "reg_mod.h"


/*! \brief Unscape all printable ASCII characters */
int unescape_string(str *sin)
{
	char *at, *p, c;

	if (sin == NULL || sin->s == NULL || sin->len < 0)
		return -1;

	at = sin->s;
	p  = sin->s;
	while(p < sin->s + sin->len)
	{
	    if (*p == '%')
		{
			p++;
			switch (*p)
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				    c = (*p - '0') << 4;
			    break;
				case 'a':
				case 'b':
				case 'c':
				case 'd':
				case 'e':
				case 'f':
				    c = (*p - 'a' + 10) << 4;
			    break;
				case 'A':
				case 'B':
				case 'C':
				case 'D':
				case 'E':
				case 'F':
				    c = (*p - 'A' + 10) << 4;
			    break;
				default:
				    LM_ERR("invalid hex digit <%u>\n", (unsigned int)*p);
				    return -1;
			}
			p++;
			switch (*p)
			{
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				    c =  c + (*p - '0');
			    break;
				case 'a':
				case 'b':
				case 'c':
				case 'd':
				case 'e':
				case 'f':
				    c = c + (*p - 'a' + 10);
			    break;
				case 'A':
				case 'B':
				case 'C':
				case 'D':
				case 'E':
				case 'F':
				    c = c + (*p - 'A' + 10);
			    break;
				default:
				    LM_ERR("invalid hex digit <%u>\n", (unsigned int)*p);
				    return -1;
			}
			if ((c < 32) || (c > 126))
			{
			    LM_ERR("invalid escaped character <%u>\n", (unsigned int)c);
			    return -1;
			}
			*at++ = c;
	    } else {
			*at++ = *p;
	    }
	    p++;
	}

	sin->len = at - sin->s;
	
	return 0;
}

/*! \brief
 * Combines all Path HF bodies into one string.
 */
int build_path_vector(struct sip_msg *_m, str *path, str *received)
{
	static char buf[MAX_PATH_SIZE];
	char *p;
	struct hdr_field *hdr;
	struct sip_uri puri;

	rr_t *route = 0;

	path->len = 0;
	path->s = 0;
	received->s = 0;
	received->len = 0;

	if(parse_headers(_m, HDR_EOH_F, 0) < 0) {
		LM_ERR("failed to parse the message\n");
		goto error;
	}

	for( hdr=_m->path,p=buf ; hdr ; hdr = next_sibling_hdr(hdr)) {
		/* check for max. Path length */
		if( p-buf+hdr->body.len+1 >= MAX_PATH_SIZE) {
			LM_ERR("Overall Path body exceeds max. length of %d\n",
					MAX_PATH_SIZE);
			goto error;
		}
		if(p!=buf)
			*(p++) = ',';
		memcpy( p, hdr->body.s, hdr->body.len);
		p +=  hdr->body.len;
	}

	if (p!=buf) {
		/* check if next hop is a loose router */
		if (parse_rr_body( buf, p-buf, &route) < 0) {
			LM_ERR("failed to parse Path body, no head found\n");
			goto error;
		}
		if (parse_uri(route->nameaddr.uri.s,route->nameaddr.uri.len,&puri)<0){
			LM_ERR("failed to parse the first Path URI\n");
			goto error;
		}
		if (!puri.lr.s) {
			LM_ERR("first Path URI is not a loose-router, not supported\n");
			goto error;
		}
		if (path_use_params) {
			param_hooks_t hooks;
			param_t *params;

			if (parse_params(&(puri.params),CLASS_CONTACT,&hooks,&params)!=0){
				LM_ERR("failed to parse parameters of first hop\n");
				goto error;
			}

			if (hooks.contact.received) {
				*received = hooks.contact.received->body;
				if (unescape_string(received) < 0)
				    LM_ERR("unescaping received value <%.*s> failed\n",
					   received->len, received->s);
			}
				
			/*for (;params; params = params->next) {
				if (params->type == P_RECEIVED) {
					*received = hooks.contact.received->body;
					break;
				}
			}*/
			free_params(params);
		}
		free_rr(&route);
	}

	path->s = buf;
	path->len = p-buf;
	return 0;
error:
	if(route) free_rr(&route);
	return -1;
}

