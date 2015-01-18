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
 *
 */

/** Parser :: parse header files
 * @file
 * @ingroup parser
 */


#include "hf.h"
#include "parse_via.h"
#include "parse_to.h"
#include "parse_cseq.h"
#include "parse_date.h"
#include "parse_identity.h"
#include "parse_identityinfo.h"
#include "../dprint.h"
#include "../mem/mem.h"
#include "parse_def.h"
#include "digest/digest.h" /* free_credentials */
#include "parse_event.h"
#include "parse_expires.h"
#include "parse_sipifmatch.h"
#include "parse_rr.h"
#include "parse_subscription_state.h"
#include "contact/parse_contact.h"
#include "parse_disposition.h"
#include "parse_allow.h"
#include "../ut.h"
#include "parse_ppi_pai.h"

/** Frees a hdr_field structure.
 * WARNING: it frees only parsed (and not name.s, body.s)
 */
void clean_hdr_field(struct hdr_field* const hf)
{
	void** h_parsed;

	if (hf->parsed){
		h_parsed=&hf->parsed; /* strict aliasing warnings workarround */
		switch(hf->type){
		/* headers with pkg alloc for parsed structure (alphabetic order) */
		case HDR_ACCEPT_T:
			pkg_free(hf->parsed);
			break;

		case HDR_ALLOW_T:
			free_allow_header(hf);
			break;

		case HDR_AUTHORIZATION_T:
			free_credentials((auth_body_t**)h_parsed);
			break;

		case HDR_CONTACT_T:
			free_contact((contact_body_t**)h_parsed);
			break;

		case HDR_CONTENTDISPOSITION_T:
			free_disposition( ((struct disposition**)h_parsed));
			break;

		case HDR_CSEQ_T:
			free_cseq(hf->parsed);
			break;

		case HDR_DATE_T:
			free_date(hf->parsed);
			break;

		case HDR_DIVERSION_T:
			free_to(hf->parsed);
			break;

		case HDR_EVENT_T:
			free_event((event_t**)h_parsed);
			break;

		case HDR_EXPIRES_T:
			free_expires((exp_body_t**)h_parsed);
			break;

		case HDR_FROM_T:
			free_to(hf->parsed);
			break;

		case HDR_IDENTITY_INFO_T:
			free_identityinfo(hf->parsed);
			break;

		case HDR_IDENTITY_T:
			free_identity(hf->parsed);
			break;

		case HDR_PAI_T:
			free_pai_ppi_body(hf->parsed);
			break;

		case HDR_PPI_T:
			free_pai_ppi_body(hf->parsed);
			break;

		case HDR_PROXYAUTH_T:
			free_credentials((auth_body_t**)h_parsed);
			break;

		case HDR_RECORDROUTE_T:
			free_rr((rr_t**)h_parsed);
			break;

		case HDR_REFER_TO_T:
			free_to(hf->parsed);
			break;

		case HDR_ROUTE_T:
			free_rr((rr_t**)h_parsed);
			break;

		case HDR_RPID_T:
			free_to(hf->parsed);
			break;

		case HDR_SESSIONEXPIRES_T:
			hdr_free_parsed(h_parsed);
			break;

		case HDR_SIPIFMATCH_T:
			free_sipifmatch((str **)h_parsed);
			break;

		case HDR_SUBSCRIPTION_STATE_T:
			free_subscription_state((subscription_state_t**)h_parsed);
			break;

		case HDR_SUPPORTED_T:
			hdr_free_parsed(h_parsed);
			break;

		case HDR_TO_T:
			free_to(hf->parsed);
			break;

		case HDR_VIA_T:
			free_via_list(hf->parsed);
			break;

		/* headers with no alloc for parsed structure */
		case HDR_CALLID_T:
		case HDR_MAXFORWARDS_T:
		case HDR_CONTENTTYPE_T:
		case HDR_CONTENTLENGTH_T:
		case HDR_RETRY_AFTER_T:
		case HDR_REQUIRE_T:
		case HDR_PROXYREQUIRE_T:
		case HDR_UNSUPPORTED_T:
		case HDR_ACCEPTLANGUAGE_T:
		case HDR_ORGANIZATION_T:
		case HDR_PRIORITY_T:
		case HDR_SUBJECT_T:
		case HDR_USERAGENT_T:
		case HDR_SERVER_T:
		case HDR_MIN_SE_T:
		case HDR_ACCEPTCONTACT_T:
		case HDR_ALLOWEVENTS_T:
		case HDR_CONTENTENCODING_T:
		case HDR_REFERREDBY_T:
		case HDR_REJECTCONTACT_T:
		case HDR_REQUESTDISPOSITION_T:
		case HDR_WWW_AUTHENTICATE_T:
		case HDR_PROXY_AUTHENTICATE_T:
		case HDR_PATH_T:
		case HDR_PRIVACY_T:
		case HDR_REASON_T:
			break;

		default:
			LOG(L_CRIT, "BUG: clean_hdr_field: unknown header type %d\n",
			    hf->type);
			break;
		}
	}
}


/** Frees a hdr_field list.
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

/* print the content of hdr_field */
void dump_hdr_field(struct hdr_field const* const hf )
{
	LOG(L_ERR, "DEBUG: dump_hdr_field: type=%d, name=%.*s, "
		"body=%.*s, parsed=%p, next=%p\n",
		hf->type, hf->name.len, ZSW(hf->name.s),
		hf->body.len, ZSW(hf->body.s),
		hf->parsed, hf->next );
}

/**
 * free hdr parsed structure using inner free function
 * - hdr parsed struct must have as first file a free function,
 *   so it can be caseted to hf_parsed_t
 */
void hdr_free_parsed(void **h_parsed)
{
	if(h_parsed==NULL || *h_parsed==NULL)
		return;

	if(((hf_parsed_t*)(*h_parsed))->hfree) {
		((hf_parsed_t*)(*h_parsed))->hfree(*h_parsed);
	}
	*h_parsed = 0;
}
