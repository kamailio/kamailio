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

/** Parser :: parse headers.
 * @file 
 *
 * @ingroup parser
 */



#ifndef HF_H
#define HF_H

#include "../str.h"
#include "../comp_defs.h"



/** header types enum.
 * 
 * @note
 * if you add a new type:
 *  - make sure it's not greater than 63
 *  - make sure you add the corresponding flag to the hdr_flags_t defs below
 *  - update clean_hdr_field (in hf.c)
 *  - update sip_msg_cloner (modules/tm/sip_msg.c)
 *  - update parse_headers (msg_parser.c)
 *  - update get_hdr_field (msg_parser.c)
 */

enum _hdr_types_t {
	HDR_ERROR_T					= -1   /*!< Error while parsing */,
	HDR_OTHER_T					=  0   /*!< Some other header field */,
	HDR_VIA_T					=  1   /*!< Via header field */,
	HDR_VIA1_T					=  1   /*!< First Via header field */,
	HDR_VIA2_T					=  2   /*!< only used as flag */,
	HDR_TO_T					       /*!< To header field */,
	HDR_FROM_T					       /*!< From header field */,
	HDR_CSEQ_T					       /*!< CSeq header field */,
	HDR_CALLID_T				       /*!< Call-Id header field */,
	HDR_CONTACT_T				       /*!< Contact header field */,
	HDR_MAXFORWARDS_T			       /*!< MaxForwards header field */,
	HDR_ROUTE_T					       /*!< Route header field */,
	HDR_RECORDROUTE_T			       /*!< Record-Route header field */,
	HDR_CONTENTTYPE_T			       /*!< Content-Type header field */,
	HDR_CONTENTLENGTH_T			       /*!< Content-Length header field */,
	HDR_AUTHORIZATION_T			       /*!< Authorization header field */,
	HDR_EXPIRES_T				       /*!< Expires header field */,
	HDR_PROXYAUTH_T				       /*!< Proxy-Authorization hdr field */,
	HDR_SUPPORTED_T				       /*!< Supported  header field */,
	HDR_REQUIRE_T				       /*!< Require header */,
	HDR_PROXYREQUIRE_T			       /*!< Proxy-Require header field */,
	HDR_UNSUPPORTED_T			       /*!< Unsupported header field */,
	HDR_ALLOW_T					       /*!< Allow header field */,
	HDR_EVENT_T					       /*!< Event header field */,
	HDR_ACCEPT_T				       /*!< Accept header field */,
	HDR_ACCEPTLANGUAGE_T		       /*!< Accept-Language header field */,
	HDR_ORGANIZATION_T			       /*!< Organization header field */,
	HDR_PRIORITY_T				       /*!< Priority header field */,
	HDR_SUBJECT_T				       /*!< Subject header field */,
	HDR_USERAGENT_T				       /*!< User-Agent header field */,
	HDR_SERVER_T				       /*!< Server header field */,
	HDR_CONTENTDISPOSITION_T	       /*!< Content-Disposition hdr field */,
	HDR_DIVERSION_T				       /*!< Diversion header field */,
	HDR_RPID_T					       /*!< Remote-Party-ID header field */,
	HDR_REFER_TO_T				       /*!< Refer-To header fiels */,
	HDR_SIPIFMATCH_T			       /*!< SIP-If-Match header field */,
	HDR_SESSIONEXPIRES_T		       /*!< Session-Expires header */,
	HDR_MIN_SE_T				       /*!< Min-SE */,
	HDR_SUBSCRIPTION_STATE_T	       /*!< Subscription-State */,
	HDR_ACCEPTCONTACT_T			       /*!< Accept-Contact header */,
	HDR_ALLOWEVENTS_T			       /*!< Allow-Events header */,
	HDR_CONTENTENCODING_T		       /*!< Content-Encoding header */,
	HDR_REFERREDBY_T			       /*!< Referred-By header */,
	HDR_REJECTCONTACT_T			       /*!< Reject-Contact header */,
	HDR_REQUESTDISPOSITION_T	       /*!< Request-Disposition header */,
	HDR_WWW_AUTHENTICATE_T		       /*!< WWW-Authenticate header field */,
	HDR_PROXY_AUTHENTICATE_T	       /*!< Proxy-Authenticate header field */,
	HDR_DATE_T			       /*!< Date header field */,
	HDR_IDENTITY_T			       /*!< Identity header field */,
	HDR_IDENTITY_INFO_T		       /*!< Identity-info header field */,
	HDR_RETRY_AFTER_T		           /*!< Retry-After header field */,
	HDR_PPI_T                          /*!< P-Preferred-Identity header field*/,
	HDR_PAI_T                          /*!< P-Asserted-Identity header field*/,
	HDR_PATH_T                         /*!< Path header field */,
	HDR_PRIVACY_T				       /*!< Privacy header field */,
	HDR_REASON_T				       /**< Reason header field */,
	HDR_EOH_T					       /*!< End of message header */
};


typedef unsigned long long hdr_flags_t;

/** type to flag conversion.
 * WARNING: HDR_ERROR_T has no corresponding FLAG ! */
#define HDR_T2F(type)	\
		(((type)!=HDR_EOH_T)?((hdr_flags_t)1<<(type)):(~(hdr_flags_t)0))

/** helper macro for easy defining and keeping in sync the flags enum. */
#define HDR_F_DEF(name)		HDR_T2F(HDR_##name##_T)

/** @name flags definitions.
 * (enum won't work with all the compiler (e.g. icc) due to the 64bit size) */
/*!{ */
#define HDR_EOH_F					HDR_F_DEF(EOH)
#define HDR_VIA_F					HDR_F_DEF(VIA)
#define HDR_VIA1_F					HDR_F_DEF(VIA1)
#define HDR_VIA2_F					HDR_F_DEF(VIA2)
#define HDR_TO_F					HDR_F_DEF(TO)
#define HDR_FROM_F					HDR_F_DEF(FROM)
#define HDR_CSEQ_F					HDR_F_DEF(CSEQ)
#define HDR_CALLID_F				HDR_F_DEF(CALLID)
#define HDR_CONTACT_F				HDR_F_DEF(CONTACT)
#define HDR_MAXFORWARDS_F			HDR_F_DEF(MAXFORWARDS)
#define HDR_ROUTE_F					HDR_F_DEF(ROUTE)
#define HDR_RECORDROUTE_F			HDR_F_DEF(RECORDROUTE)
#define HDR_CONTENTTYPE_F			HDR_F_DEF(CONTENTTYPE)
#define HDR_CONTENTLENGTH_F			HDR_F_DEF(CONTENTLENGTH)
#define HDR_AUTHORIZATION_F			HDR_F_DEF(AUTHORIZATION)
#define HDR_EXPIRES_F				HDR_F_DEF(EXPIRES)
#define HDR_PROXYAUTH_F				HDR_F_DEF(PROXYAUTH)
#define HDR_SUPPORTED_F				HDR_F_DEF(SUPPORTED)
#define HDR_REQUIRE_F				HDR_F_DEF(REQUIRE)
#define HDR_PROXYREQUIRE_F			HDR_F_DEF(PROXYREQUIRE)
#define HDR_UNSUPPORTED_F			HDR_F_DEF(UNSUPPORTED)
#define HDR_ALLOW_F					HDR_F_DEF(ALLOW)
#define HDR_EVENT_F					HDR_F_DEF(EVENT)
#define HDR_ACCEPT_F				HDR_F_DEF(ACCEPT)
#define HDR_ACCEPTLANGUAGE_F		HDR_F_DEF(ACCEPTLANGUAGE)
#define HDR_ORGANIZATION_F			HDR_F_DEF(ORGANIZATION)
#define HDR_PRIORITY_F				HDR_F_DEF(PRIORITY)
#define HDR_SUBJECT_F				HDR_F_DEF(SUBJECT)
#define HDR_USERAGENT_F				HDR_F_DEF(USERAGENT)
#define HDR_SERVER_F				HDR_F_DEF(SERVER)
#define HDR_CONTENTDISPOSITION_F	HDR_F_DEF(CONTENTDISPOSITION)
#define HDR_DIVERSION_F				HDR_F_DEF(DIVERSION)
#define HDR_RPID_F					HDR_F_DEF(RPID)
#define HDR_REFER_TO_F				HDR_F_DEF(REFER_TO)
#define HDR_SIPIFMATCH_F			HDR_F_DEF(SIPIFMATCH)
#define HDR_SESSIONEXPIRES_F		HDR_F_DEF(SESSIONEXPIRES)
#define HDR_MIN_SE_F				HDR_F_DEF(MIN_SE)
#define HDR_SUBSCRIPTION_STATE_F	HDR_F_DEF(SUBSCRIPTION_STATE)
#define HDR_ACCEPTCONTACT_F			HDR_F_DEF(ACCEPTCONTACT)
#define HDR_ALLOWEVENTS_F			HDR_F_DEF(ALLOWEVENTS)
#define HDR_CONTENTENCODING_F		HDR_F_DEF(CONTENTENCODING)
#define HDR_REFERREDBY_F			HDR_F_DEF(REFERREDBY)
#define HDR_REJECTCONTACT_F			HDR_F_DEF(REJECTCONTACT)
#define HDR_REQUESTDISPOSITION_F	HDR_F_DEF(REQUESTDISPOSITION)
#define HDR_WWW_AUTHENTICATE_F		HDR_F_DEF(WWW_AUTHENTICATE)
#define HDR_PROXY_AUTHENTICATE_F	HDR_F_DEF(PROXY_AUTHENTICATE)
#define HDR_DATE_F			HDR_F_DEF(DATE)
#define HDR_IDENTITY_F			HDR_F_DEF(IDENTITY)
#define HDR_IDENTITY_INFO_F		HDR_F_DEF(IDENTITY_INFO)
#define HDR_RETRY_AFTER_F			HDR_F_DEF(RETRY_AFTER)
#define HDR_PPI_F                   HDR_F_DEF(PPI)
#define HDR_PAI_F                   HDR_F_DEF(PAI)
#define HDR_PATH_F                  HDR_F_DEF(PATH)
#define HDR_PRIVACY_F               HDR_F_DEF(PRIVACY)
#define HDR_REASON_F				HDR_F_DEF(REASON)

#define HDR_OTHER_F					HDR_F_DEF(OTHER)

/*!} */ /* Doxygen end marker*/

typedef enum _hdr_types_t hdr_types_t;

/** Format: name':' body.
 */
typedef struct hdr_field {
	hdr_types_t type;       /*!< Header field type */
	str name;               /*!< Header field name */
	str body;               /*!< Header field body (may not include CRLF) */
	int len;		/*!< length from hdr start until EoHF (incl.CRLF) */
	void* parsed;           /*!< Parsed data structures */
	struct hdr_field* next; /*!< Next header field in the list */
} hdr_field_t;


/* type of the function to free the structure of parsed header field */
typedef void (*hf_parsed_free_f)(void *parsed);

/* structure to hold the function to free the parsed header field */
typedef struct hdr_parsed {
	hf_parsed_free_f hfree;
} hf_parsed_t;

/** returns true if the header links allocated memory on parse field. */
static inline int hdr_allocs_parse(struct hdr_field* hdr)
{
	switch(hdr->type){
		case HDR_ACCEPT_T:
		case HDR_ALLOW_T:
		case HDR_AUTHORIZATION_T:
		case HDR_CONTACT_T:
		case HDR_CONTENTDISPOSITION_T:
		case HDR_CSEQ_T:
		case HDR_DATE_T:
		case HDR_DIVERSION_T:
		case HDR_EVENT_T:
		case HDR_EXPIRES_T:
		case HDR_FROM_T:
		case HDR_IDENTITY_INFO_T:
		case HDR_IDENTITY_T:
		case HDR_PAI_T:
		case HDR_PPI_T:
		case HDR_PROXYAUTH_T:
		case HDR_RECORDROUTE_T:
		case HDR_REFER_TO_T:
		case HDR_ROUTE_T:
		case HDR_RPID_T:
		case HDR_SESSIONEXPIRES_T:
		case HDR_SIPIFMATCH_T:
		case HDR_SUBSCRIPTION_STATE_T:
		case HDR_SUPPORTED_T:
		case HDR_TO_T:
		case HDR_VIA_T:
			return 1;
		default:
			return 0;
	}
}

/** frees a hdr_field structure.
 * WARNING: it frees only parsed (and not name.s, body.s)
 */
void clean_hdr_field(struct hdr_field* const hf);


/** frees a hdr_field list.
 * WARNING: frees only ->parsed and ->next
 */
void free_hdr_field_lst(struct hdr_field* hf);

/* print content of hdr_field */
void dump_hdr_field( struct hdr_field const* const hf);

/**
 * free hdr parsed structure using inner free function
 * - hdr parsed struct must have as first file a free function,
 *   so it can be caseted to hf_parsed_t
 */
void hdr_free_parsed(void **h_parsed);

#endif /* HF_H */
