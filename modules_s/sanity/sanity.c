/*
 * $Id$
 *
 * Sanity Checks Module
 *
 * Copyright (C) 2006 iptelorg GbmH
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
 */

#include "mod_sanity.h"
#include "sanity.h"
#include "../../ut.h"
#include "../../trim.h"
#include "../../data_lump_rpl.h"
#include "../../mem/mem.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_expires.h"
#include "../../parser/parse_content.h"

#define UNSUPPORTED_HEADER "Unsupported: "
#define UNSUPPORTED_HEADER_LEN (sizeof(UNSUPPORTED_HEADER)-1)

/* check if the given string is a valid unsigned int value */
int str2valid_uint(str* _number, unsigned int* _result) {
	int i;
	int result= 0;
	int equal = 1;
	char mui[10] = "4294967296";

	*_result = 0;
	if (_number->len > 10) {
#ifdef EXTRA_DEBUG
		DBG("valid_uint(): number is too long\n");
#endif
		return -1;
	}
	if (_number->len < 10) {
		equal = 0;
	}
	for (i=0; i < _number->len; i++) {
		if (_number->s[i] < '0' || _number->s[i] > '9') {
#ifdef EXTRA_DEBUG
			DBG("valid_uint(): number contains non-number char\n");
#endif
			return -1;
		}
		if (equal == 1) {
			if (_number->s[i] < mui[i]) {
				equal = 0;
			}
			else if (_number->s[i] > mui[i]) {
#ifdef EXTRA_DEBUG
				DBG("valid_uint(): number exceeds uint\n");
#endif
				return -1;
			}
		}
		result *= 10;
		result += _number->s[i] - '0';
	}
	*_result = result;
	return 0;
}

/* parses the given comma seperated string into a string list */
strl* parse_str_list(str* _string) {
	str input;
	strl *parsed_list, *pl;
	char *comma;

	/* make a copy because we trim it */
	input.s = _string->s;
	input.len = _string->len;

	trim(&input);

	if (input.len == 0) {
#ifdef EXTRA_DEBUG
		DBG("parse_str_list: list is empty\n");
#endif
		return NULL;
	}
	parsed_list = pkg_malloc(sizeof(strl));
	if (parsed_list == NULL) {
		LOG(L_ERR, "parse_str_list: OUT OF MEMORY for initial list element\n");
		return NULL;
	}
	memset(parsed_list, 0, sizeof(strl));
	parsed_list->string.s = input.s;
	parsed_list->string.len = input.len;

	comma = q_memchr(input.s, ',', input.len);
	pl = parsed_list;
	while (comma != NULL) {
		pl->next = pkg_malloc(sizeof(strl));
		if (pl->next == NULL) {
			LOG(L_ERR, "parse_str_list: OUT OF MEMORY for further list element\n");
			return parsed_list;
		}
		memset(pl->next, 0, sizeof(strl));
		pl->next->string.s = comma + 1;
		pl->next->string.len = pl->string.len - (pl->next->string.s - pl->string.s);
		pl->string.len = comma - pl->string.s;
		trim_trailing(&(pl->string));
		pl = pl->next;
		trim_leading(&(pl->string));
		comma = q_memchr(pl->string.s, ',', pl->string.len);
	}

	return parsed_list;
}

int parse_proxyrequire(struct hdr_field* _h) {
	strl *pr_l;

	if (_h->parsed) {
		return 0; /* Already parsed */
	}

	if ((pr_l = parse_str_list(&(_h->body))) == NULL) {
		LOG(L_ERR, "parse_proxy_require(): Error while parsing\n");
		return -1;
	}

	_h->parsed = pr_l;
	return 0;
}

/* check the SIP version in the request URI */
int check_ruri_sip_version(struct sip_msg* _msg) {
	int ret;
	char *sep;

	DBG("check_ruri_sip_version entered\n");

	if (_msg->first_line.u.request.version.len != 0) {
		sep = q_memchr(_msg->first_line.u.request.version.s, '/',
						_msg->first_line.u.request.version.len);
		if (sep == NULL) {
			LOG(L_ERR, "sanity_check(): check_ruri_sip_version(): failed to find / in ruri version\n");
			return -1;
		}
		ret=memcmp(sep+1, SIP_VERSION_TWO_POINT_ZERO, 
					SIP_VERSION_TWO_POINT_ZERO_LENGTH );
		if (ret != 0) {
			if (sl_reply(_msg, (char*)505, "Version Not Supported (R-URI)") == -1) {
				LOG(L_ERR, "sanity_check(): check_ruri_sip_version(): failed to send 505 via send_reply\n");
				return -1;
			}
			DBG("check_ruri_sip_version failed\n");
			return 1;
		}
	}
#ifdef EXTRA_DEBUG
	DBG("check_ruri_sip_version passed\n");
#endif

	return 0;
}

/* check if the r-uri scheme */
int check_ruri_scheme(struct sip_msg* _msg) {

	DBG("sanit_check(): check_ruri_scheme(): this is useless check for now; check the source code comments\n");
	return 0;

	/* FIXME unsupported uri scheme end up allready with
	 * an error in the uri parser, thus this check does not
	 * make too much sence yet 
	DBG("check_ruri_scheme entered\n");

	if (_msg->parsed_uri_ok == 0 &&
			parse_sip_msg_uri(_msg) != 1) {
		// FIXME unsupported schemes end up here already
		LOG(L_ERR, "sanity_check(): check_ruri_scheme(): failed to parse request uri\n");
		return -1;
	}
	if (_msg->parsed_uri.type == ERROR_URI_T) {
		if (sl_reply(_msg, (char*)416, "Unsupported URI Scheme (R-URI)") == -1) {
			LOG(L_ERR, "sanity_check(): check_ruri_scheme(): failed to send 416 via send_reply\n");
			return -1;
		}
		DBG("check_ruri_scheme failed\n");
		return 1;
	}
#ifdef EXTRA_DEBUG
	DBG("check_ruri_scheme passed\n");
#endif

	return 0;
	*/
}

/* check for the presence of the minimal required headers */
int check_required_headers(struct sip_msg* _msg) {

	DBG("check_required_headers entered\n");

	if (!check_transaction_quadruple(_msg)) {
		if (sl_reply(_msg, (char*)400, "Missing Required Header in Request") == -1) {
			LOG(L_ERR, "sanity_check(): check_required_headers(): failed to send 400 via send_reply\n");
			return -1;
		}
		DBG("check_required_headers failed\n");
		return 1;
	}
	/* TODO: check for other required headers according to request type */
#ifdef EXTRA_DEBUG
	DBG("check_required_headers passed\n");
#endif

	return 0;
}

/* check if the SIP version in the Via header is 2.0 */
int check_via_sip_version(struct sip_msg* _msg) {

	DBG("sanity_check(): check_via_sip_version(): this is a useless check for now; check the source code comments for details\n");
	return 0;

	/* FIMXE the Via parser fails already on non-2.0 versions
	 * thus this check makes no sence yet
	DBG("check_via_sip_version entered\n");

	// FIXME via parser fails on non 2.0 number
	if (parse_headers(_msg, HDR_VIA1_F, 0) != 0) {
		LOG(L_ERR, "sanity_check(): check_via_sip_version(): failed to parse the first Via header\n");
		return -1;
	}

	if (_msg->via1->version.len != 3 ||
			memcmp(_msg->via1->version.s, SIP_VERSION_TWO_POINT_ZERO, 
					SIP_VERSION_TWO_POINT_ZERO_LENGTH ) != 0) {
		if (sl_reply(_msg, (char*)505, "Version Not Supported (Via)") == -1) {
			LOG(L_ERR, "sanity_check(): check_via_sip_version(): failed to send 505 via send_reply\n");
			return -1;
		}
		DBG("check_via_sip_version failed\n");
		return 1;
	}
#ifdef EXTRA_DEBUG
	DBG("check_via_sip_version passed\n");
#endif

	return 0;
	*/
}

/* compare the protocol string in the Via header with the transport */
int check_via_protocol(struct sip_msg* _msg) {

	DBG("sanity_check(): check_via_protocol(): this is a useless check for now; check the source code comment for details\n");
	return 0;

	/* FIXME as the Via parser fails already on unknown transports
	 * this function makes no sence yet
	DBG("check_via_protocol entered\n");

	// FIXME via parser fails on unknown transport
	if (parse_headers(_msg, HDR_VIA1_F, 0) != 0) {
		LOG(L_ERR, "sanity_check(): check_via_protocol(): failed to parse the first Via header\n");
		return -1;
	}
	if (_msg->via1->transport.len != 3 &&
			_msg->via1->transport.len != 4) {
		if (sl_reply(_msg, (char*)400, "Unsupported Transport in Topmost Via") == -1) {
			LOG(L_ERR, "sanity_check(): check_via_protocol(): failed to send 400 via send_reply\n");
			return -1;
		}
		DBG("check_via_protocol failed\n");
		return 1;
	}
	switch (_msg->rcv.proto) {
		case PROTO_UDP:
			if (memcmp(_msg->via1->transport.s, "UDP", 3) != 0) {
				if (sl_reply(_msg, (char*)400, "Transport Missmatch in Topmost Via") == -1) {
					LOG(L_ERR, "sanity_check(): check_via_protocol(): failed to send 505 via send_reply\n");
					return -1;
				}
				DBG("check_via_protocol failed\n");
				return 1;
			}
			break;
		case PROTO_TCP:
			if (memcmp(_msg->via1->transport.s, "TCP", 3) != 0) {
				if (sl_reply(_msg, (char*)400, "Transport Missmatch in Topmost Via") == -1) {
					LOG(L_ERR, "sanity_check(): check_via_protocol(): failed to send 505 via send_reply\n");
					return -1;
				}
				DBG("check_via_protocol failed\n");
				return 1;
			}
			break;
		case PROTO_TLS:
			if (memcmp(_msg->via1->transport.s, "TLS", 3) != 0) {
				if (sl_reply(_msg, (char*)400, "Transport Missmatch in Topmost Via") == -1) {
					LOG(L_ERR, "sanity_check(): check_via_protocol(): failed to send 505 via send_reply\n");
					return -1;
				}
				DBG("check_via_protocol failed\n");
				return 1;
			}
			break;
		case PROTO_SCTP:
			if (memcmp(_msg->via1->transport.s, "SCTP", 4) != 0) {
				if (sl_reply(_msg, (char*)400, "Transport Missmatch in Topmost Via") == -1) {
					LOG(L_ERR, "sanity_check(): check_via_protocol(): failed to send 505 via send_reply\n");
					return -1;
				}
				DBG("check_via_protocol failed\n");
				return 1;
			}
			break;
		default:
			LOG(L_ERR, "sanity_check(): check_via_protocol(): unknown protocol in received structure\n");
			return -1;
	}
#ifdef EXTRA_DEBUG
	DBG("check_via_protocol passed\n");
#endif

	return 0;
	*/
}

/* compare the method in the CSeq header with the request line value */
int check_cseq_method(struct sip_msg* _msg) {

	DBG("check_cseq_method entered\n");

	if (parse_headers(_msg, HDR_CSEQ_F, 0) != 0) {
		LOG(L_ERR, "sanity_check(): check_cseq_method(): failed to parse the CSeq header\n");
		return -1;
	}
	if (_msg->cseq != NULL && _msg->cseq->parsed != NULL) {
		if (((struct cseq_body*)_msg->cseq->parsed)->method.len == 0) {
			if (sl_reply(_msg, (char*)400, "Missing method in CSeq header") == -1) {
				LOG(L_ERR, "sanity_check(): check_cseq_method(): failed to send 400 via send_reply\n");
				return -1;
			}
			DBG("check_cseq_method failed (missing method)\n");
			return 1;
		}

		if (((struct cseq_body*)_msg->cseq->parsed)->method.len != 
					_msg->first_line.u.request.method.len ||
			memcmp(((struct cseq_body*)_msg->cseq->parsed)->method.s, 
				_msg->first_line.u.request.method.s,
				((struct cseq_body*)_msg->cseq->parsed)->method.len) != 0) {
			if (sl_reply(_msg, (char*)400, "CSeq method does not match request method") == -1) {
				LOG(L_ERR, "sanity_check(): check_cseq_method(): failed to send 400 via send_reply 2\n");
				return -1;
			}
			DBG("check_cseq_method failed (non-equal method)\n");
			return 1;
		}
	}
	else {
		LOG(L_ERR, "sanity_check(): check_cseq_method(): missing CSeq header\n");
		return -1;
	}
#ifdef EXTRA_DEBUG
	DBG("check_cseq_method passed\n");
#endif

	return 0;
}

/* check the number within the CSeq header */
int check_cseq_value(struct sip_msg* _msg) {
	unsigned int cseq;

	DBG("check_cseq_value entered\n");

	if (parse_headers(_msg, HDR_CSEQ_F, 0) != 0) {
		LOG(L_ERR, "sanity_check(): check_cseq_value(): failed to parse the CSeq header\n");
		return -1;
	}
	if (_msg->cseq != NULL && _msg->cseq->parsed != NULL) {
		if (((struct cseq_body*)_msg->cseq->parsed)->number.len == 0) {
			if (sl_reply(_msg, (char*)400, "Missing number in CSeq header") == -1) {
				LOG(L_ERR, "sanity_check(): check_cseq_value(): failed to send 400 via send_reply\n");
				return -1;
			}
			return 1;
		}
		if (str2valid_uint(&((struct cseq_body*)_msg->cseq->parsed)->number, &cseq) != 0) {
			if (sl_reply(_msg, (char*)400, "CSeq number is illegal") == -1) {
				LOG(L_ERR, "sanity_check(): check_cseq_value(): failed to send 400 via send_reply 2\n");
				return -1;
			}
			DBG("check_cseq_value failed\n");
			return 1;
		}
	}
	else {
		LOG(L_ERR, "sanity_check(): check_cseq_method(): missing CSeq header\n");
		return -1;
	}
#ifdef EXTRA_DEBUG
	DBG("check_cseq_value passed\n");
#endif

	return 0;
}

/* compare the Content-Length value with the accutal body length */
int check_cl(struct sip_msg* _msg) {
	char *body;

	DBG("check_cl entered\n");

	if (parse_headers(_msg, HDR_CONTENTLENGTH_F, 0) != 0) {
		LOG(L_ERR, "sanity_check(): check_cl(): failed to parse content-length header\n");
		return -1;
	}
	if (_msg->content_length != NULL) {
		//dump_hdr_field(_msg->content_length);
		if ((body = get_body(_msg)) == NULL) {
#ifdef EXTRA_DEBUG
			DBG("check_cl(): no body\n");
#endif
			return -1;
		}
		if ((_msg->len - (body - _msg->buf)) != get_content_length(_msg)) {
			if (sl_reply(_msg, (char*)400, "Content-Length mis-match") == -1) {
				LOG(L_ERR, "sanity_check(): check_cl(): failed to send 400 via send_reply\n");
				return -1;
			}
			DBG("check_cl failed\n");
			return 1;
		}
		DBG("check_cl passed\n");
	}
#ifdef EXTRA_DEBUG
	else {
		WARN("check_cl(): content length header missing in request\n");
	}
#endif

	return 0;
}

/* check the number within the Expires header */
int check_expires_value(struct sip_msg* _msg) {
	unsigned int expires;

	DBG("check_expires_value entered\n");

	if (parse_headers(_msg, HDR_EXPIRES_F, 0) != 0) {
		LOG(L_ERR, "sanity_check(): check_expires_value(): failed to parse expires header\n");
		return -1;
	}
	if (_msg->expires != NULL) {
		//dump_hdr_field(_msg->expires);
		if (_msg->expires->parsed == NULL &&
				parse_expires(_msg->expires) < 0) {
			LOG(L_ERR, "sanity_check(): check_expires_value(): parse_expires failed\n");
			return -1;
		}
		if (((struct exp_body*)_msg->expires->parsed)->text.len == 0) {
			if (sl_reply(_msg, (char*)400, "Missing number in Expires header") == -1) {
				LOG(L_ERR, "sanity_check(): check_expires_value(): failed to send 400 via send_reply\n");
				return -1;
			}
			DBG("check_expires_value failed\n");
			return 1;
		}
		if (str2valid_uint(&((struct exp_body*)_msg->expires->parsed)->text, &expires) != 0) {
			if (sl_reply(_msg, (char*)400, "Expires value is illegal") == -1) {
				LOG(L_ERR, "sanity_check(): check_expires_value(): failed to send 400 via send_reply 2\n");
				return -1;
			}
			DBG("check_expires_value failed\n");
			return 1;
		}
		DBG("check_expires_value passed\n");
	}
#ifdef EXTRA_DEBUG
	else {
		DBG("check_expires_value(): no expires header found\n");
	}
#endif

	return 0;
}

/* check the content of the Proxy-Require header */
int check_proxy_require(struct sip_msg* _msg) {
	strl *r_pr, *l_pr;
	char *u;
	int u_len;

	DBG("check_proxy_require entered\n");

	if (parse_headers(_msg, HDR_PROXYREQUIRE_F, 0) != 0) {
		LOG(L_ERR, "sanity_check(): check_proxy_require(): failed to parse proxy require header\n");
		return -1;
	}
	if (_msg->proxy_require != NULL) {
		dump_hdr_field(_msg->proxy_require);
		if (_msg->proxy_require->parsed == NULL &&
				parse_proxyrequire(_msg->proxy_require) < 0) {
			LOG(L_ERR, "sanity_check(): check_proxy_require(): parse_proxy_require failed\n");
			return -1;
		}
		r_pr = _msg->proxy_require->parsed;
		while (r_pr != NULL) {
			l_pr = proxyrequire_list;
			while (l_pr != NULL) {
#ifdef EXTRA_DEBUG
				DBG("check_proxy_require(): comparing r='%.*s' l='%.*s'\n", r_pr->string.len, r_pr->string.s, l_pr->string.len, l_pr->string.s);
#endif
				if (l_pr->string.len == r_pr->string.len &&
						/* FIXME tokens are case in-sensitive */
						memcmp(l_pr->string.s, r_pr->string.s, l_pr->string.len) == 0) {
					break;
				}
				l_pr = l_pr->next;
			}
			if (l_pr == NULL) {
				DBG("sanit_check(): check_proxy_require(): request contains unsupported extension: %.*s\n", r_pr->string.len, r_pr->string.s);
				u_len = UNSUPPORTED_HEADER_LEN + 2 + r_pr->string.len;
				u = pkg_malloc(u_len);
				if (u == NULL) {
					LOG(L_ERR, "sanity_check(): check_proxy_require(): failed to allocate memory for Unsupported header\n");
				}
				else {
					memcpy(u, UNSUPPORTED_HEADER, UNSUPPORTED_HEADER_LEN);
					memcpy(u + UNSUPPORTED_HEADER_LEN, r_pr->string.s, r_pr->string.len);
					memcpy(u + UNSUPPORTED_HEADER_LEN + r_pr->string.len, "\r\n", 2);
					add_lump_rpl(_msg, u, u_len, LUMP_RPL_HDR);
				}
				if (sl_reply(_msg, (char*)420, "Bad Extension") == -1) {
					LOG(L_ERR, "sanity_check(): check_proxy_require(): failed to send 420 via send_reply\n");
					return -1;
				}
				DBG("check_proxy_require failed\n");
				return 1;
			}
			else {
				r_pr = r_pr->next;
			}
		}
		DBG("check_proxy_require passed\n");
	}
#ifdef EXTRA_DEBUG
	else {
		DBG("check_proxy_require(): no proxy-require header found\n");
	}
#endif

	return 0;
}
