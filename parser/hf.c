/* 
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
 *
 * History:
 * -------
 * 2003-03-26 Frees also hdr->parsed for Route & Record-Route (janakj)
 * 2003-04-26 ZSW (jiri)
 * 2003-08-05 free the parsed part of Accept header (bogdan)
 */


#include "hf.h"
#include "parse_via.h"
#include "parse_to.h"
#include "parse_cseq.h"
#include "../dprint.h"
#include "../mem/mem.h"
#include "parse_def.h"
#include "digest/digest.h" /* free_credentials */
#include "parse_event.h"
#include "parse_expires.h"
#include "parse_rr.h"
#include "contact/parse_contact.h"
#include "parse_disposition.h"
#include "../ut.h"


/* 
 * Frees a hdr_field structure,
 * WARNING: it frees only parsed (and not name.s, body.s)
 */
void clean_hdr_field(struct hdr_field* hf)
{
	if (hf->parsed){
		switch(hf->type){
		case HDR_VIA:
			free_via_list(hf->parsed);
			break;

		case HDR_TO:
			free_to(hf->parsed);
			break;

		case HDR_FROM:
			free_to(hf->parsed);
			break;

		case HDR_CSEQ:
			free_cseq(hf->parsed);
			break;

		case HDR_CALLID:
			break;

		case HDR_CONTACT:
			free_contact((contact_body_t**)(&(hf->parsed)));
			break;

		case HDR_MAXFORWARDS:
			break;

		case HDR_ROUTE:
			free_rr((rr_t**)(&hf->parsed));
			break;

		case HDR_RECORDROUTE:
			free_rr((rr_t**)(&hf->parsed));
			break;

		case HDR_CONTENTTYPE:
			break;

		case HDR_CONTENTLENGTH:
			break;

		case HDR_AUTHORIZATION:
			break;

		case HDR_EXPIRES:
			free_expires((exp_body_t**)(&(hf->parsed)));
			break;

		case HDR_PROXYAUTH:
			free_credentials((auth_body_t**)(&(hf->parsed)));
			break;

		case HDR_SUPPORTED:
			break;

		case HDR_PROXYREQUIRE:
			break;

		case HDR_UNSUPPORTED:
			break;

		case HDR_ALLOW:
			break;

		case HDR_EVENT:
			free_event((event_t**)(&(hf->parsed)));
			break;

		case HDR_ACCEPT:
			pkg_free(hf->parsed);
			break;

		case HDR_ACCEPTLANGUAGE:
			break;
			
		case HDR_ORGANIZATION:
			break;
			
		case HDR_PRIORITY:
			break;

		case HDR_SUBJECT:
			break;

		case HDR_USERAGENT:
			break;

		case HDR_ACCEPTDISPOSITION:
			break;

		case HDR_CONTENTDISPOSITION:
			free_disposition( ((struct disposition**)(&hf->parsed)) );
			break;

		default:
			LOG(L_CRIT, "BUG: clean_hdr_field: unknown header type %d\n",
			    hf->type);
			break;
		}
	}
}


/* 
 * Frees a hdr_field list,
 * WARNING: frees only ->parsed and ->next*/
void free_hdr_field_lst(struct hdr_field* hf)
{
	struct hdr_field* foo;
	
	while(hf) {
		foo=hf;
		hf=hf->next;
		clean_hdr_field(foo);
		pkg_free(foo);
	}
}

void dump_hdr_field( struct hdr_field* hf )
{
	LOG(L_ERR, "DEBUG: dump_hdr_field: type=%d, name=%.*s, "
		"body=%.*s, parsed=%p, next=%p\n",
		hf->type, hf->name.len, ZSW(hf->name.s),
		hf->body.len, ZSW(hf->body.s),
		hf->parsed, hf->next );
}
