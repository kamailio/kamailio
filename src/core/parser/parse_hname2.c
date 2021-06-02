/*
 * Fast 32-bit Header Field Name Parser
 *
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

/** Parser :: Fast 32-bit Header Field Name Parser.
 * @file
 * @ingroup parser
 */

#include <stdio.h>
#include <ctype.h>

#include "../dprint.h"

#include "parse_hname2.h"

typedef struct ksr_hdr_map {
	str hname;
	hdr_types_t htype;
	hdr_flags_t hflag;
} ksr_hdr_map_t;

/* map with SIP header name
 * - must be groupped by first letter - indexed for faster search */
static ksr_hdr_map_t _ksr_hdr_map[] = {
	{ str_init("a"), HDR_ACCEPTCONTACT_T, HDR_ACCEPTCONTACT_F },
	{ str_init("Accept"), HDR_ACCEPT_T, HDR_ACCEPT_F },
	{ str_init("Accept-Contact"), HDR_ACCEPTCONTACT_T, HDR_ACCEPTCONTACT_F },
	{ str_init("Accept-Language"), HDR_ACCEPTLANGUAGE_T, HDR_ACCEPTLANGUAGE_F },
	{ str_init("Allow"), HDR_ALLOW_T, HDR_ALLOW_F },
	{ str_init("Allow-Events"), HDR_ALLOWEVENTS_T, HDR_ALLOWEVENTS_F },
	{ str_init("Authorization"), HDR_AUTHORIZATION_T, HDR_AUTHORIZATION_F },

	{ str_init("b"), HDR_REFERREDBY_T, HDR_REFERREDBY_F },

	{ str_init("c"), HDR_CONTENTTYPE_T, HDR_CONTENTTYPE_F },
	{ str_init("Call-Id"), HDR_CALLID_T, HDR_CALLID_F },
	{ str_init("Call-Info"), HDR_CALLINFO_T, HDR_CALLINFO_F },
	{ str_init("Contact"), HDR_CONTACT_T, HDR_CONTACT_F },
	{ str_init("Content-Disposition"), HDR_CONTENTDISPOSITION_T, HDR_CONTENTDISPOSITION_F },
	{ str_init("Content-Encoding"), HDR_CONTENTENCODING_T, HDR_CONTENTENCODING_F },
	{ str_init("Content-Length"), HDR_CONTENTLENGTH_T, HDR_CONTENTLENGTH_F },
	{ str_init("Content-Type"), HDR_CONTENTTYPE_T, HDR_CONTENTTYPE_F },
	{ str_init("CSeq"), HDR_CSEQ_T, HDR_CSEQ_F },

	{ str_init("d"), HDR_REQUESTDISPOSITION_T, HDR_REQUESTDISPOSITION_F },
	{ str_init("Date"), HDR_DATE_T, HDR_DATE_F },
	{ str_init("Diversion"), HDR_DIVERSION_T, HDR_DIVERSION_F },

	{ str_init("e"), HDR_CONTENTENCODING_T, HDR_CONTENTENCODING_F },
	{ str_init("Event"), HDR_EVENT_T, HDR_EVENT_F },
	{ str_init("Expires"), HDR_EXPIRES_T, HDR_EXPIRES_F },

	{ str_init("f"), HDR_FROM_T, HDR_FROM_F },
	{ str_init("From"), HDR_FROM_T, HDR_FROM_F },

	{ str_init("i"), HDR_CALLID_T, HDR_CALLID_F },
	{ str_init("Identity"), HDR_IDENTITY_T, HDR_IDENTITY_F },
	{ str_init("Identity-Info"), HDR_IDENTITY_INFO_T, HDR_IDENTITY_INFO_F },

	{ str_init("j"), HDR_REJECTCONTACT_T, HDR_REJECTCONTACT_F },

	{ str_init("k"), HDR_SUPPORTED_T, HDR_SUPPORTED_F },

	{ str_init("l"), HDR_CONTENTLENGTH_T, HDR_CONTENTLENGTH_F },

	{ str_init("m"), HDR_CONTACT_T, HDR_CONTACT_F },
	{ str_init("Max-Forwards"), HDR_MAXFORWARDS_T, HDR_MAXFORWARDS_F },
	{ str_init("Min-Expires"), HDR_MIN_EXPIRES_T, HDR_MIN_EXPIRES_F },
	{ str_init("Min-SE"), HDR_MIN_SE_T, HDR_MIN_SE_F },

	{ str_init("o"), HDR_EVENT_T, HDR_EVENT_F },
	{ str_init("Organization"), HDR_ORGANIZATION_T, HDR_ORGANIZATION_F },

	{ str_init("Path"), HDR_PATH_T, HDR_PATH_F },
	{ str_init("Priority"), HDR_PRIORITY_T, HDR_PRIORITY_F },
	{ str_init("Privacy"), HDR_PRIVACY_T, HDR_PRIVACY_F },
	{ str_init("Proxy-Authenticate"), HDR_PROXY_AUTHENTICATE_T, HDR_PROXY_AUTHENTICATE_F },
	{ str_init("Proxy-Authorization"), HDR_PROXYAUTH_T, HDR_PROXYAUTH_F },
	{ str_init("Proxy-Require"), HDR_PROXYREQUIRE_T, HDR_PROXYREQUIRE_F },
	{ str_init("P-Preferred-Identity"), HDR_PPI_T, HDR_PPI_F },
	{ str_init("P-Asserted-Identity"), HDR_PAI_T, HDR_PAI_F },

	{ str_init("r"), HDR_REFER_TO_T, HDR_REFER_TO_F },
	{ str_init("Reason"), HDR_REASON_T, HDR_REASON_F },
	{ str_init("Record-Route"), HDR_RECORDROUTE_T, HDR_RECORDROUTE_F },
	{ str_init("Refer-To"), HDR_REFER_TO_T, HDR_REFER_TO_F },
	{ str_init("Referred-By"), HDR_REFERREDBY_T, HDR_REFERREDBY_F },
	{ str_init("Reject-Contact"), HDR_REJECTCONTACT_T, HDR_REJECTCONTACT_F },
	{ str_init("Remote-Party-ID"), HDR_RPID_T, HDR_RPID_F },
	{ str_init("Request-Disposition"), HDR_REQUESTDISPOSITION_T, HDR_REQUESTDISPOSITION_F },
	{ str_init("Require"), HDR_REQUIRE_T, HDR_REQUIRE_F },
	{ str_init("Retry-After"), HDR_RETRY_AFTER_T, HDR_RETRY_AFTER_F },
	{ str_init("Route"), HDR_ROUTE_T, HDR_ROUTE_F },

	{ str_init("s"), HDR_SUBJECT_T, HDR_SUBJECT_F },
	{ str_init("Server"), HDR_SERVER_T, HDR_SERVER_F },
	{ str_init("Session-Expires"), HDR_SESSIONEXPIRES_T, HDR_SESSIONEXPIRES_F },
	{ str_init("SIP-If-Match"), HDR_SIPIFMATCH_T, HDR_SIPIFMATCH_F },
	{ str_init("Subject"), HDR_SUBJECT_T, HDR_SUBJECT_F },
	{ str_init("Subscription-State"), HDR_SUBSCRIPTION_STATE_T, HDR_SUBSCRIPTION_STATE_F },
	{ str_init("Supported"), HDR_SUPPORTED_T, HDR_SUPPORTED_F },

	{ str_init("t"), HDR_TO_T, HDR_TO_F },
	{ str_init("To"), HDR_TO_T, HDR_TO_F },

	{ str_init("u"), HDR_ALLOWEVENTS_T, HDR_ALLOWEVENTS_F },
	{ str_init("Unsupported"), HDR_UNSUPPORTED_T, HDR_UNSUPPORTED_F },
	{ str_init("User-Agent"), HDR_USERAGENT_T, HDR_USERAGENT_F },

	{ str_init("v"), HDR_VIA_T, HDR_VIA_F },
	{ str_init("Via"), HDR_VIA_T, HDR_VIA_F },

	{ str_init("x"), HDR_SESSIONEXPIRES_T, HDR_SESSIONEXPIRES_F },

	{ str_init("y"), HDR_IDENTITY_T, HDR_IDENTITY_F },

	{ str_init("WWW-Authenticate"), HDR_WWW_AUTHENTICATE_T, HDR_WWW_AUTHENTICATE_F },

	{ str_init(""), 0, 0 }
};

typedef struct ksr_hdr_map_idx {
	int idxs;
	int idxe;
} ksr_hdr_map_idx_t;

#define KSR_HDR_MAP_IDX_SIZE 256

/**
 * array to keep start and end indexes of header names groupped by first char
 */
static ksr_hdr_map_idx_t _ksr_hdr_map_idx[KSR_HDR_MAP_IDX_SIZE];

/**
 * valid chars in header names
 */
static unsigned char *_ksr_hname_chars_list = (unsigned char*)"0123456789AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz-._+~";

/**
 * additional valid chars in header names (core param)
 */
unsigned char *_ksr_hname_extra_chars = (unsigned char*)"";

/**
 * indexed valid chars in 256-array for 1-byte-index access check
 */
static unsigned char _ksr_hname_chars_idx[KSR_HDR_MAP_IDX_SIZE];


/**
 * init header name parsing structures and indexes at very beginning of start up
 */
int ksr_hname_init_index(void)
{
	unsigned char c;
	int i;

	for(i=0; i<KSR_HDR_MAP_IDX_SIZE; i++) {
		_ksr_hdr_map_idx[i].idxs = -1;
		_ksr_hdr_map_idx[i].idxe = -1;
		_ksr_hname_chars_idx[i] = 0;
	}

	for(i=0; _ksr_hdr_map[i].hname.len > 0; i++) {
		c = _ksr_hdr_map[i].hname.s[0];
		if(_ksr_hdr_map_idx[c].idxs == -1) {
			_ksr_hdr_map_idx[tolower(c)].idxs = i;
			_ksr_hdr_map_idx[toupper(c)].idxs = i;
		}
		if(c != _ksr_hdr_map[i+1].hname.s[0]) {
			_ksr_hdr_map_idx[tolower(c)].idxe = i;
			_ksr_hdr_map_idx[toupper(c)].idxe = i;
		}
	}

	for(i=0; _ksr_hname_chars_list[i] != 0; i++) {
		_ksr_hname_chars_idx[_ksr_hname_chars_list[i]] = 1;
	}

	return 0;
}

/**
 * init header name parsing structures and indexes after config parsing
 */
int ksr_hname_init_config(void)
{
	int i;

	for(i=0; _ksr_hname_extra_chars[i] != 0; i++) {
		_ksr_hname_chars_idx[_ksr_hname_extra_chars[i]] = 1;
	}

	return 0;
}

/**
 * parse the sip header name in the buffer starting at 'begin' till before 'end'
 * - fills hdr structure (must not be null)
 * - set hdr->type=HDR_ERROR_T in case of parsing error
 * - if emode==1, then parsing does not expect : after header name
 * - if logmode==1, then print error log messages on parsing failure
 * - returns pointer after : if emode==0 or after header name if emode==1
 *   in case of parsing error, returns begin and sets hdr->type to HDR_ERROR_T
 */
char *parse_sip_header_name(char* const begin, const char* const end,
		hdr_field_t* const hdr, int emode, int logmode)
{
	char *p;
	int i;

	if (begin == NULL || end == NULL || end <= begin) {
		hdr->type = HDR_ERROR_T;
		return begin;
	}
	if(_ksr_hname_chars_idx[(unsigned char)(*begin)] == 0) {
		if(likely(logmode)) {
			LM_ERR("invalid start of header name for [%.*s]\n",
					(int)(end-begin), begin);
		}
		hdr->type = HDR_ERROR_T;
		return begin;
	}
	hdr->type = HDR_OTHER_T;
	hdr->name.s = begin;

	for(p=begin+1; p<end; p++) {
		if(_ksr_hname_chars_idx[(unsigned char)(*p)] == 0) {
			/* char not allowed in header name */
			break;
		}
	}
	hdr->name.len = p - hdr->name.s;

	if(emode == 1) {
		/* allowed end of header name without finding : */
		p = p - 1; /* function returns (p+1) */
		goto done;
	}

	/* ensure no character or only white spaces till : */
	for(; p<end; p++) {
		if(*p == ':') {
			/* end of header name */
			break;
		}
		if(*p != ' ' && *p != '\t') {
			/* no white space - bad header name format */
			if(likely(logmode)) {
				LM_ERR("invalid header name for [%.*s]\n",
						(int)(end-begin), begin);
			}
			hdr->type = HDR_ERROR_T;
			return begin;
		}
	}

	if(p == end) {
		/* no : found - emode==0 */
		if(likely(logmode)) {
			LM_ERR("invalid end of header name for [%.*s]\n",
					(int)(end-begin), begin);
		}
		hdr->type = HDR_ERROR_T;
		return begin;
	}

done:
	/* lookup header type */
	if(_ksr_hdr_map_idx[(unsigned char)(hdr->name.s[0])].idxs >= 0) {
		for(i = _ksr_hdr_map_idx[(unsigned char)(hdr->name.s[0])].idxs;
					i <= _ksr_hdr_map_idx[(unsigned char)(hdr->name.s[0])].idxe; i++) {
			if(hdr->name.len == _ksr_hdr_map[i].hname.len
					&& strncasecmp(hdr->name.s, _ksr_hdr_map[i].hname.s,
							hdr->name.len) == 0) {
				hdr->type = _ksr_hdr_map[i].htype;
			}
		}
	}

	LM_DBG("parsed header name [%.*s] type %d\n", hdr->name.len, hdr->name.s,
				hdr->type);

	return (p+1);
}

char* parse_hname2(char* const begin, const char* const end, struct hdr_field* const hdr)
{
	return parse_sip_header_name(begin, end, hdr, 0, 1);
}

/**
 * kept for compatibility of code developed in the past
 * - to be replace with parse_hname2() across the code
 */
char* parse_hname2_short(char* const begin, const char* const end, struct hdr_field* const hdr)
{
	return parse_sip_header_name(begin, end, hdr, 0, 1);
}

char* parse_hname2_str (str* const hbuf, hdr_field_t* const hdr)
{
	return parse_sip_header_name(hbuf->s, hbuf->s + hbuf->len, hdr, 1, 1);
}

