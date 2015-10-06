/*
 * Sanity Checks Module
 *
 * Copyright (C) 2006 iptelorg GbmH
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

#include "mod_sanity.h"
#include "sanity.h"
#include "../../ut.h"
#include "../../trim.h"
#include "../../data_lump_rpl.h"
#include "../../mem/mem.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_expires.h"
#include "../../parser/parse_content.h"
#include "../../parser/digest/digest.h"
#include "../../parser/contact/parse_contact.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_from.h"

#define UNSUPPORTED_HEADER "Unsupported: "
#define UNSUPPORTED_HEADER_LEN (sizeof(UNSUPPORTED_HEADER)-1)


/**
 * wrapper to SL send reply function
 * - check if it is the case for sending a reply before doing it
 */
int sanity_reply(sip_msg_t *msg, int code, char *reason)
{
	if(msg->first_line.type == SIP_REPLY) {
		return 1;
	}

	if(msg->REQ_METHOD == METHOD_ACK) {
		return 1;
	}
	if(slb.zreply(msg, code, reason) < 0) {
		return -1;
	}
	return 0;
}

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
			LOG(L_ERR, "parse_str_list: OUT OF MEMORY for further list"
					" element\n");
			return parsed_list;
		}
		memset(pl->next, 0, sizeof(strl));
		pl->next->string.s = comma + 1;
		pl->next->string.len = pl->string.len
									- (pl->next->string.s - pl->string.s);
		pl->string.len = comma - pl->string.s;
		trim_trailing(&(pl->string));
		pl = pl->next;
		trim_leading(&(pl->string));
		comma = q_memchr(pl->string.s, ',', pl->string.len);
	}

	return parsed_list;
}

/* free the elements of the linked str list */
void free_str_list(strl *_list) {
	strl *cur, *next;

	if (_list != NULL) {
		cur = _list;
		while (cur != NULL) {
			next = cur->next;
			pkg_free(cur);
			cur = next;
		}
	}
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
	char *sep;
	str version;

#ifdef EXTRA_DEBUG
	DBG("check_ruri_sip_version entered\n");
#endif

	if (_msg->first_line.u.request.version.len != 0) {
		sep = q_memchr(_msg->first_line.u.request.version.s, '/',
						_msg->first_line.u.request.version.len);
		if (sep == NULL) {
			LOG(L_WARN, "sanity_check(): check_ruri_sip_version():"
					" failed to find / in ruri version\n");
			return SANITY_CHECK_FAILED;
		}
		version.s = sep + 1;
		version.len = _msg->first_line.u.request.version.len - (version.s - _msg->first_line.u.request.version.s);

		if (version.len != SIP_VERSION_TWO_POINT_ZERO_LENGTH ||
			(memcmp(version.s, SIP_VERSION_TWO_POINT_ZERO, 
				SIP_VERSION_TWO_POINT_ZERO_LENGTH) != 0)) {
			if (_msg->REQ_METHOD != METHOD_ACK) {
				if (sanity_reply(_msg, 505, "Version Not Supported (R-URI)")
						< 0) {
					LOG(L_WARN, "sanity_check(): check_ruri_sip_version():"
							" failed to send 505 via sl reply\n");
				}
			}
#ifdef EXTRA_DEBUG
			DBG("check_ruri_sip_version failed\n");
#endif
			return SANITY_CHECK_FAILED;
		}
	}
#ifdef EXTRA_DEBUG
	DBG("check_ruri_sip_version passed\n");
#endif
	return SANITY_CHECK_PASSED;
}

/* check if the r-uri scheme */
int check_ruri_scheme(struct sip_msg* _msg) {

#ifdef EXTRA_DEBUG
	DBG("check_ruri_scheme entered\n");
#endif

	if (_msg->parsed_uri_ok == 0 &&
			parse_sip_msg_uri(_msg) != 1) {
		/* unsupported schemes end up here already */
		LM_WARN("failed to parse request uri [%.*s]\n",
				GET_RURI(_msg)->len, GET_RURI(_msg)->s);
		if (_msg->REQ_METHOD != METHOD_ACK) {
			if (slb.zreply(_msg, 400, "Bad Request URI") < 0) {
				LOG(L_WARN, "sanity_check(): check_parse_uris():"
						" failed to send 400 via sl reply (bad ruri)\n");
			}
		}
		return SANITY_CHECK_FAILED;
	}
	if (_msg->parsed_uri.type == ERROR_URI_T) {
		if (_msg->REQ_METHOD != METHOD_ACK) {
			if (sanity_reply(_msg, 416, "Unsupported URI Scheme in Request URI")
					< 0) {
				LOG(L_WARN, "sanity_check(): check_ruri_scheme():"
						" failed to send 416 via sl reply\n");
			}
		}
		DBG("check_ruri_scheme failed\n");
		return SANITY_CHECK_FAILED;
	}
#ifdef EXTRA_DEBUG
	DBG("check_ruri_scheme passed\n");
#endif

	return SANITY_CHECK_PASSED;
}

/* check for the presence of the minimal required headers */
int check_required_headers(struct sip_msg* _msg) {

#ifdef EXTRA_DEBUG
	DBG("check_required_headers entered\n");
#endif

	if (!check_transaction_quadruple(_msg)) {
		if (_msg->REQ_METHOD != METHOD_ACK) {
			if (sanity_reply(_msg, 400, "Missing Required Header in Request")
					< 0) {
				LOG(L_WARN, "sanity_check(): check_required_headers():"
						" failed to send 400 via sl reply\n");
			}
		}
		DBG("check_required_headers failed\n");
		return SANITY_CHECK_FAILED;
	}
	/* TODO: check for other required headers according to request type */
#ifdef EXTRA_DEBUG
	DBG("check_required_headers passed\n");
#endif

	return SANITY_CHECK_PASSED;
}

/* check if the SIP version in the Via header is 2.0 */
int check_via_sip_version(struct sip_msg* _msg) {

	DBG("sanity_check(): check_via_sip_version(): this is a useless check"
			" for now; check the source code comments for details\n");
	return SANITY_CHECK_PASSED;

	/* FIMXE the Via parser fails already on non-2.0 versions
	 * thus this check makes no sence yet
	DBG("check_via_sip_version entered\n");

	// FIXME via parser fails on non 2.0 number
	if (parse_headers(_msg, HDR_VIA1_F, 0) != 0) {
		LOG(L_WARN, "sanity_check(): check_via_sip_version():"
			" failed to parse the first Via header\n");
		return SANITY_CHECK_FAILED;
	}

	if (_msg->via1->version.len != 3 ||
			memcmp(_msg->via1->version.s, SIP_VERSION_TWO_POINT_ZERO, 
					SIP_VERSION_TWO_POINT_ZERO_LENGTH ) != 0) {
		if (_msg->REQ_METHOD != METHOD_ACK) {
			if (sanity_reply(_msg, 505, "Version Not Supported (Via)") < 0) {
				LOG(L_WARN, "sanity_check(): check_via_sip_version():"
					" failed to send 505 via sl reply\n");
			}
		}
		DBG("check_via_sip_version failed\n");
		return SANITY_CHECK_FAILED;
	}
#ifdef EXTRA_DEBUG
	DBG("check_via_sip_version passed\n");
#endif

	return SANITY_CHECK_PASSED;
	*/
}

/* compare the protocol string in the Via header with the transport */
int check_via_protocol(struct sip_msg* _msg) {

	DBG("sanity_check(): check_via_protocol(): this is a useless check"
			" for now; check the source code comment for details\n");
	return SANITY_CHECK_PASSED;

	/* FIXME as the Via parser fails already on unknown transports
	 * this function makes no sence yet
	DBG("check_via_protocol entered\n");

	// FIXME via parser fails on unknown transport
	if (parse_headers(_msg, HDR_VIA1_F, 0) != 0) {
		LOG(L_WARN, "sanity_check(): check_via_protocol():"
			" failed to parse the first Via header\n");
		return SANITY_CHECK_FAILED;
	}
	if (_msg->via1->transport.len != 3 &&
			_msg->via1->transport.len != 4) {
		if (_msg->REQ_METHOD != METHOD_ACK) {
			if (sanity_reply(_msg, 400, "Unsupported Transport in Topmost Via")
					< 0) {
				LOG(L_WARN, "sanity_check(): check_via_protocol():"
					" failed to send 400 via sl reply\n");
			}
		}
		DBG("check_via_protocol failed\n");
		return SANITY_CHECK_FAILED;
	}
	switch (_msg->rcv.proto) {
		case PROTO_UDP:
			if (memcmp(_msg->via1->transport.s, "UDP", 3) != 0) {
				if (_msg->REQ_METHOD != METHOD_ACK) {
					if (sanity_reply(_msg, 400,
							"Transport Missmatch in Topmost Via") < 0) {
						LOG(L_WARN, "sanity_check(): check_via_protocol():"
								" failed to send 505 via sl reply\n");
					}
				}
				DBG("check_via_protocol failed\n");
				return SANITY_CHECK_FAILED;
			}
			break;
		case PROTO_TCP:
			if (memcmp(_msg->via1->transport.s, "TCP", 3) != 0) {
				if (_msg->REQ_METHOD != METHOD_ACK) {
					if (sanity_reply(_msg, 400,
							"Transport Missmatch in Topmost Via") < 0) {
						LOG(L_WARN, "sanity_check(): check_via_protocol():"
								" failed to send 505 via sl reply\n");
					}
				}
				DBG("check_via_protocol failed\n");
				return SANITY_CHECK_FAILED;
			}
			break;
		case PROTO_TLS:
			if (memcmp(_msg->via1->transport.s, "TLS", 3) != 0) {
				if (_msg->REQ_METHOD != METHOD_ACK) {
					if (sanity_reply(_msg, 400,
							"Transport Missmatch in Topmost Via") < 0) {
						LOG(L_WARN, "sanity_check(): check_via_protocol():"
								" failed to send 505 via sl reply\n");
					}
				}
				DBG("check_via_protocol failed\n");
				return SANITY_CHECK_FAILED;
			}
			break;
		case PROTO_SCTP:
			if (memcmp(_msg->via1->transport.s, "SCTP", 4) != 0) {
				if (_msg->REQ_METHOD != METHOD_ACK) {
					if (sanity_reply(_msg, 400,
							"Transport Missmatch in Topmost Via") < 0) {
						LOG(L_WARN, "sanity_check(): check_via_protocol():"
								" failed to send 505 via sl reply\n");
					}
				}
				DBG("check_via_protocol failed\n");
				return SANITY_CHECK_FAILED;
			}
			break;
		case PROTO_WS:
			if (memcmp(_msg->via1->transport.s, "WS", 2) != 0) {
				if (_msg->REQ_METHOD != METHOD_ACK) {
					if (sanity_reply(_msg, 400,
							"Transport Missmatch in Topmost Via") < 0) {
						LOG(L_WARN, "sanity_check(): check_via_protocol():"
								" failed to send 505 via sl reply\n");
					}
				}
				DBG("check_via_protocol failed\n");
				return SANITY_CHECK_FAILED;
			}
			break;
		case PROTO_WSS:
			if (memcmp(_msg->via1->transport.s, "WSS", 3) != 0) {
				if (_msg->REQ_METHOD != METHOD_ACK) {
					if (sanity_reply(_msg, 400,
							"Transport Missmatch in Topmost Via") < 0) {
						LOG(L_WARN, "sanity_check(): check_via_protocol():"
								" failed to send 505 via sl reply\n");
					}
				}
				DBG("check_via_protocol failed\n");
				return SANITY_CHECK_FAILED;
			}
			break;
		default:
			LOG(L_WARN, "sanity_check(): check_via_protocol():"
					" unknown protocol in received structure\n");
			return SANITY_CHECK_FAILED;
	}
#ifdef EXTRA_DEBUG
	DBG("check_via_protocol passed\n");
#endif

	return SANITY_CHECK_PASSED;
	*/
}

/* compare the method in the CSeq header with the request line value */
int check_cseq_method(struct sip_msg* _msg) {

#ifdef EXTRA_DEBUG
	DBG("check_cseq_method entered\n");
#endif

	if (parse_headers(_msg, HDR_CSEQ_F, 0) != 0) {
		LOG(L_WARN, "sanity_check(): check_cseq_method():"
				" failed to parse the CSeq header\n");
		return SANITY_CHECK_FAILED;
	}
	if (_msg->cseq != NULL && _msg->cseq->parsed != NULL) {
		if (((struct cseq_body*)_msg->cseq->parsed)->method.len == 0) {
			if (_msg->REQ_METHOD != METHOD_ACK) {
				if (sanity_reply(_msg, 400, "Missing method in CSeq header")
						< 0) {
					LOG(L_WARN, "sanity_check(): check_cseq_method():"
							" failed to send 400 via sl reply\n");
				}
			}
			DBG("check_cseq_method failed (missing method)\n");
			return SANITY_CHECK_FAILED;
		}

		if (((struct cseq_body*)_msg->cseq->parsed)->method.len != 
					_msg->first_line.u.request.method.len ||
			memcmp(((struct cseq_body*)_msg->cseq->parsed)->method.s, 
				_msg->first_line.u.request.method.s,
				((struct cseq_body*)_msg->cseq->parsed)->method.len) != 0) {
			if (_msg->REQ_METHOD != METHOD_ACK) {
				if (sanity_reply(_msg, 400,
							"CSeq method does not match request method") < 0) {
					LOG(L_WARN, "sanity_check(): check_cseq_method():"
							" failed to send 400 via sl reply 2\n");
				}
			}
			DBG("check_cseq_method failed (non-equal method)\n");
			return SANITY_CHECK_FAILED;
		}
	}
	else {
		LOG(L_WARN, "sanity_check(): check_cseq_method():"
				" missing CSeq header\n");
		return SANITY_CHECK_FAILED;
	}
#ifdef EXTRA_DEBUG
	DBG("check_cseq_method passed\n");
#endif

	return SANITY_CHECK_PASSED;
}

/* check the number within the CSeq header */
int check_cseq_value(struct sip_msg* _msg) {
	unsigned int cseq;

#ifdef EXTRA_DEBUG
	DBG("check_cseq_value entered\n");
#endif

	if (parse_headers(_msg, HDR_CSEQ_F, 0) != 0) {
		LOG(L_WARN, "sanity_check(): check_cseq_value():"
				" failed to parse the CSeq header\n");
		return SANITY_CHECK_FAILED;
	}
	if (_msg->cseq != NULL && _msg->cseq->parsed != NULL) {
		if (((struct cseq_body*)_msg->cseq->parsed)->number.len == 0) {
			if (_msg->REQ_METHOD != METHOD_ACK) {
				if (sanity_reply(_msg, 400, "Missing number in CSeq header")
						< 0) {
					LOG(L_WARN, "sanity_check(): check_cseq_value():"
							" failed to send 400 via sl reply\n");
				}
			}
			return SANITY_CHECK_FAILED;
		}
		if (str2valid_uint(&((struct cseq_body*)_msg->cseq->parsed)->number,
					&cseq) != 0) {
			if (_msg->REQ_METHOD != METHOD_ACK) {
				if (sanity_reply(_msg, 400, "CSeq number is illegal") < 0) {
					LOG(L_WARN, "sanity_check(): check_cseq_value():"
							" failed to send 400 via sl reply 2\n");
				}
			}
			DBG("check_cseq_value failed\n");
			return SANITY_CHECK_FAILED;
		}
	}
	else {
		LOG(L_WARN, "sanity_check(): check_cseq_method():"
				" missing CSeq header\n");
		return SANITY_CHECK_FAILED;
	}
#ifdef EXTRA_DEBUG
	DBG("check_cseq_value passed\n");
#endif

	return SANITY_CHECK_PASSED;
}

/* compare the Content-Length value with the accutal body length */
int check_cl(struct sip_msg* _msg) {
	char *body;

#ifdef EXTRA_DEBUG
	DBG("check_cl entered\n");
#endif

	if (parse_headers(_msg, HDR_CONTENTLENGTH_F, 0) != 0) {
		LOG(L_WARN, "sanity_check(): check_cl():"
				" failed to parse content-length header\n");
		return SANITY_CHECK_FAILED;
	}
	if (_msg->content_length != NULL) {
		//dump_hdr_field(_msg->content_length);
		if ((body = get_body(_msg)) == NULL) {
#ifdef EXTRA_DEBUG
			DBG("check_cl(): no body\n");
#endif
			return SANITY_CHECK_FAILED;
		}
		if ((_msg->len - (body - _msg->buf)) != get_content_length(_msg)) {
			if (_msg->REQ_METHOD != METHOD_ACK) {
				if (sanity_reply(_msg, 400, "Content-Length mis-match") < 0) {
					LOG(L_WARN, "sanity_check(): check_cl():"
							" failed to send 400 via sl reply\n");
				}
			}
			DBG("check_cl failed\n");
			return SANITY_CHECK_FAILED;
		}
#ifdef EXTRA_DEBUG
		DBG("check_cl passed\n");
#endif
	}
#ifdef EXTRA_DEBUG
	else {
		WARN("check_cl(): content length header missing in request\n");
	}
#endif

	return SANITY_CHECK_PASSED;
}

/* check the number within the Expires header */
int check_expires_value(struct sip_msg* _msg) {
	unsigned int expires;

#ifdef EXTRA_DEBUG
	DBG("check_expires_value entered\n");
#endif

	if (parse_headers(_msg, HDR_EXPIRES_F, 0) != 0) {
		LOG(L_WARN, "sanity_check(): check_expires_value():"
				" failed to parse expires header\n");
		return SANITY_CHECK_FAILED;
	}
	if (_msg->expires != NULL) {
		//dump_hdr_field(_msg->expires);
		if (_msg->expires->parsed == NULL &&
				parse_expires(_msg->expires) < 0) {
			LOG(L_WARN, "sanity_check(): check_expires_value():"
					" parse_expires failed\n");
			return SANITY_CHECK_FAILED;
		}
		if (((struct exp_body*)_msg->expires->parsed)->text.len == 0) {
			if (_msg->REQ_METHOD != METHOD_ACK) {
				if (sanity_reply(_msg, 400, "Missing number in Expires header")
						< 0) {
					LOG(L_WARN, "sanity_check(): check_expires_value():"
							" failed to send 400 via sl reply\n");
				}
			}
			DBG("check_expires_value failed\n");
			return SANITY_CHECK_FAILED;
		}
		if (str2valid_uint(&((struct exp_body*)_msg->expires->parsed)->text, &expires) != 0) {
			if (_msg->REQ_METHOD != METHOD_ACK) {
				if (sanity_reply(_msg, 400, "Expires value is illegal") < 0) {
					LOG(L_WARN, "sanity_check(): check_expires_value():"
							" failed to send 400 via sl reply 2\n");
				}
			}
			DBG("check_expires_value failed\n");
			return SANITY_CHECK_FAILED;
		}
#ifdef EXTRA_DEBUG
		DBG("check_expires_value passed\n");
#endif
	}
#ifdef EXTRA_DEBUG
	else {
		DBG("check_expires_value(): no expires header found\n");
	}
#endif

	return SANITY_CHECK_PASSED;
}

/* check the content of the Proxy-Require header */
int check_proxy_require(struct sip_msg* _msg) {
	strl *r_pr, *l_pr;
	char *u;
	int u_len;

#ifdef EXTRA_DEBUG
	LM_DBG("checking proxy require\n");
#endif

	if (parse_headers(_msg, HDR_PROXYREQUIRE_F, 0) != 0) {
		LM_WARN("failed to parse proxy require header\n");
		return SANITY_CHECK_FAILED;
	}
	if (_msg->proxy_require != NULL) {
		//dump_hdr_field(_msg->proxy_require);
		if (_msg->proxy_require->parsed == NULL &&
				parse_proxyrequire(_msg->proxy_require) < 0) {
			LM_WARN("parse_proxy_require failed\n");
			return SANITY_CHECK_FAILED;
		}
		r_pr = _msg->proxy_require->parsed;
		while (r_pr != NULL) {
			l_pr = proxyrequire_list;
			while (l_pr != NULL) {
#ifdef EXTRA_DEBUG
				LM_DBG("comparing r='%.*s' l='%.*s'\n",
						r_pr->string.len, r_pr->string.s, l_pr->string.len,
						l_pr->string.s);
#endif
				if (l_pr->string.len == r_pr->string.len &&
						/* FIXME tokens are case in-sensitive */
						memcmp(l_pr->string.s, r_pr->string.s,
								l_pr->string.len) == 0) {
					break;
				}
				l_pr = l_pr->next;
			}
			if (l_pr == NULL) {
				LM_DBG("request contains unsupported extension: %.*s\n",
						r_pr->string.len, r_pr->string.s);
				u_len = UNSUPPORTED_HEADER_LEN + 2 + r_pr->string.len;
				u = pkg_malloc(u_len);
				if (u == NULL) {
					LM_ERR("failed to allocate memory for"
							" Unsupported header\n");
				}
				else {
					memcpy(u, UNSUPPORTED_HEADER, UNSUPPORTED_HEADER_LEN);
					memcpy(u + UNSUPPORTED_HEADER_LEN, r_pr->string.s,
							r_pr->string.len);
					memcpy(u + UNSUPPORTED_HEADER_LEN + r_pr->string.len,
							CRLF, CRLF_LEN);
					add_lump_rpl(_msg, u, u_len, LUMP_RPL_HDR);
				}

				if (_msg->REQ_METHOD != METHOD_ACK) {
					if (sanity_reply(_msg, 420, "Bad Extension") < 0) {
						LM_WARN("failed to send 420 via sl reply\n");
					}
				}
#ifdef EXTRA_DEBUG
				LM_DBG("checking proxy require failed\n");
#endif
				if (u) pkg_free(u);
				return SANITY_CHECK_FAILED;
			}
			else {
				r_pr = r_pr->next;
			}
		}
#ifdef EXTRA_DEBUG
		LM_DBG("checking proxy require passed\n");
#endif
		if (_msg->proxy_require->parsed) {
			/* TODO we have to free it here, because it is not automatically
			 * freed when the message freed. Lets hope nobody needs to access
			 * this header again later on */
			free_str_list(_msg->proxy_require->parsed);
		}
	}
#ifdef EXTRA_DEBUG
	else {
		LM_DBG("no proxy-require header found\n");
	}
#endif

	return SANITY_CHECK_PASSED;
}

/* check if the typical URI's are parseable */
int check_parse_uris(struct sip_msg* _msg, int checks) {

	struct to_body *ft_body = NULL;
	struct sip_uri uri;

#ifdef EXTRA_DEBUG
	DBG("check_parse_uris entered\n");
#endif

	/* check R-URI */
	if (SANITY_URI_CHECK_RURI & checks) {
#ifdef EXTRA_DEBUG
		DBG("check_parse_uris(): parsing ruri\n");
#endif
		if (_msg->parsed_uri_ok == 0 &&
				parse_sip_msg_uri(_msg) != 1) {
			LOG(L_WARN, "sanity_check(): check_parse_uris():"
					" failed to parse request uri\n");
			if (_msg->REQ_METHOD != METHOD_ACK) {
				if (sanity_reply(_msg, 400, "Bad Request URI") < 0) {
					LOG(L_WARN, "sanity_check(): check_parse_uris():"
							" failed to send 400 via sl reply (bad ruri)\n");
				}
			}
			return SANITY_CHECK_FAILED;
		}
		/* FIXME: would it make sense to check here for "mandatory"
		 * or "requested" parts of the URI? */
	}
	/* check From URI */
	if (SANITY_URI_CHECK_FROM & checks) {
#ifdef EXTRA_DEBUG
		DBG("check_parse_uris(): looking up From header\n");
#endif
		if ((!_msg->from && parse_headers(_msg, HDR_FROM_F, 0) != 0)
				|| !_msg->from) {
			LOG(L_WARN, "sanity_check(): check_parse_uris():"
					" missing from header\n");
			if (_msg->REQ_METHOD != METHOD_ACK) {
				if (sanity_reply(_msg, 400, "Missing From Header") < 0) {
					LOG(L_WARN, "sanity_check(): check_parse_uris():"
						" failed to send 400 via sl reply (missing From)\n");
				}
			}
			return SANITY_CHECK_FAILED;
		}
		if (!_msg->from->parsed) {
#ifdef EXTRA_DEBUG
			DBG("check_parse_uris(): parsing From header\n");
#endif
			ft_body = pkg_malloc(sizeof(struct to_body));
			if (!ft_body) {
				LOG(L_ERR, "sanity_check(): check_parse_uris():"
						" out of pkg_memory (From)\n");
				return SANITY_CHECK_ERROR;
			}
			memset(ft_body, 0, sizeof(struct to_body));
			parse_to(_msg->from->body.s, _msg->from->body.s + \
					_msg->from->body.len + 1, ft_body);
			if (ft_body->error == PARSE_ERROR) {
				LOG(L_WARN, "sanity_check(): check_parse_uris():"
						" failed to parse From header [%.*s]\n",
						_msg->from->body.len, _msg->from->body.s);
				free_to(ft_body);
				if (_msg->REQ_METHOD != METHOD_ACK) {
					if (sanity_reply(_msg, 400, "Bad From header") < 0) {
						LOG(L_WARN, "sanity_check(): check_parse_uris():"
								" failed to send 400 via sl reply"
								" (bad from header)\n");
					}
				}
				return SANITY_CHECK_FAILED;
			}
			_msg->from->parsed = ft_body;
			ft_body = NULL;
		}
		if (((struct to_body*)_msg->from->parsed)->uri.s) {
#ifdef EXTRA_DEBUG
			DBG("check_parse_uris(): parsing From URI\n");
#endif
			if (parse_uri(((struct to_body*)_msg->from->parsed)->uri.s, 
					((struct to_body*)_msg->from->parsed)->uri.len, &uri) != 0) {
			    LOG(L_WARN, "sanity_check(): check_parse_uris():"
						" failed to parse From uri\n");
			    if (_msg->REQ_METHOD != METHOD_ACK) {
				if (sanity_reply(_msg, 400, "Bad From URI") < 0) {
				    LOG(L_WARN, "sanity_check(): check_parse_uris():"
							" failed to send 400 via sl reply"
							" (bad from uri)\n");
				}
			    }
			    return SANITY_CHECK_FAILED;
			}
			/* FIXME: we should store this parsed struct somewhere so that
			 * it could be re-used */
			/* FIXME 2: would it make sense to check here for "mandatory"
			 * or "requested" parts of the URI? */
		}
	}
	/* check To URI */
	if (SANITY_URI_CHECK_TO & checks) {
#ifdef EXTRA_DEBUG
		DBG("check_parse_uris(): looking up To header\n");
#endif
		if ((!_msg->to && parse_headers(_msg, HDR_TO_F, 0) != 0)
				|| !_msg->to) {
			LOG(L_WARN, "sanity_check(): check_parse_uris():"
					" missing to header\n");
			if (_msg->REQ_METHOD != METHOD_ACK) {
				if (sanity_reply(_msg, 400, "Missing To Header") < 0) {
					LOG(L_WARN, "sanity_check(): check_parse_uris():"
							" failed to send 400 via sl reply (missing To)\n");
				}
			}
			return SANITY_CHECK_FAILED;
		}
		/* parse_to is automatically called for HDR_TO_F */
		if (!_msg->to->parsed) {
			LOG(L_WARN, "sanity_check(): check_parse_uris():"
					" failed to parse To header\n");
			if (_msg->REQ_METHOD != METHOD_ACK) {
				if (sanity_reply(_msg, 400, "Bad To URI") < 0) {
					LOG(L_WARN, "sanity_check(): check_parse_uris():"
							" failed to send 400 via sl reply (bad to uri)\n");
				}
			}
			return SANITY_CHECK_FAILED;
		}
		if (((struct to_body*)_msg->to->parsed)->uri.s) {
#ifdef EXTRA_DEBUG
			DBG("check_parse_uris(): parsing To URI\n");
#endif
			if (parse_uri(((struct to_body*)_msg->to->parsed)->uri.s, 
					((struct to_body*)_msg->to->parsed)->uri.len, &uri) != 0) {
				LOG(L_WARN, "sanity_check(): check_parse_uris():"
						" failed to parse To uri\n");
				if (_msg->REQ_METHOD != METHOD_ACK) {
					if (sanity_reply(_msg, 400, "Bad To URI") < 0) {
						LOG(L_WARN, "sanity_check(): check_parse_uris():"
								" failed to send 400 via sl reply"
								" (bad to uri)\n");
					}
				}
				return SANITY_CHECK_FAILED;
			}
			/* FIXME: we should store this parsed struct somewhere so that
			 * it could be re-used */
			/* FIXME 2: would it make sense to check here for "mandatory"
			 * or "requested" parts of the URI? */
		}
	}
	/* check Contact URI */
	if (SANITY_URI_CHECK_CONTACT & checks) {
#ifdef EXTRA_DEBUG
		DBG("check_parse_uris(): looking up Contact header\n");
#endif
		if ((!_msg->contact && parse_headers(_msg, HDR_CONTACT_F, 0) != 0)
				|| !_msg->contact) {
			LOG(L_WARN, "sanity_check(): check_parse_uris():"
					" missing contact header\n");
		}
		if (_msg->contact) {
#ifdef EXTRA_DEBUG
			DBG("check_parse_uris(): parsing Contact header\n");
#endif
			if (parse_contact(_msg->contact) < 0) {
				LOG(L_WARN, "sanity_check(): check_parse_uris():"
						" failed to parse Contact header\n");
				if (_msg->REQ_METHOD != METHOD_ACK) {
					if (sanity_reply(_msg, 400, "Bad Contact Header") < 0) {
						LOG(L_WARN, "sanity_check(): check_parse_uris():"
								" failed to send 400 via send_reply"
								" (bad Contact)\n");
					}
				}
				return SANITY_CHECK_FAILED;
			}
			if (parse_uri(
				((struct contact_body*)_msg->contact->parsed)->contacts->uri.s,
				((struct contact_body*)_msg->contact->parsed)->contacts->uri.len,
				&uri) != 0) {
				LOG(L_WARN, "sanity_check(): check_parse_uris():"
						" failed to parse Contact uri\n");
				if (_msg->REQ_METHOD != METHOD_ACK) {
					if (sanity_reply(_msg, 400, "Bad Contact URI") < 0) {
						LOG(L_WARN, "sanity_check(): check_parse_uris():"
								" failed to send 400 via send_reply"
								" (bad Contact uri)\n");
					}
				}
				return SANITY_CHECK_FAILED;
			}
		}
	}

#ifdef EXTRA_DEBUG
	DBG("check_parse_uris passed\n");
#endif
	return SANITY_CHECK_PASSED;
}


/* Make sure that username attribute in all digest credentials
 * instances has a meaningful value
 */
int check_digest(struct sip_msg* msg, int checks)
{
    struct hdr_field* ptr;
    dig_cred_t* cred;
    int ret;
    int hf_type;

    if (parse_headers(msg, HDR_EOH_F, 0) != 0) {
	LOG(L_ERR, "sanity_check(): check_digest:"
			" failed to parse proxy require header\n");
	return SANITY_CHECK_FAILED;
    }

    if (!msg->authorization && !msg->proxy_auth) {
#ifdef EXTRA_DEBUG
	DBG("sanity_check(): check_digest: Nothing to check\n");
#endif
	return SANITY_CHECK_PASSED;
    }

    if (msg->authorization) {
	hf_type = HDR_AUTHORIZATION_T;
	ptr = msg->authorization;
    } else {
	hf_type = HDR_PROXYAUTH_T;
	ptr = msg->proxy_auth;
    }
    while(ptr) {
	if ((ret = parse_credentials(ptr)) != 0) {
	    DBG("sanity_check(): check_digest: Cannot parse credentials: %d\n",
				ret);
	    return SANITY_CHECK_FAILED;
	}

	cred = &((auth_body_t*)ptr->parsed)->digest;

	if (check_dig_cred(cred) != E_DIG_OK) {
#ifdef EXTRA_DEBUG
	    DBG("sanity_check(): check_digest: Digest credentials malformed\n");
#endif
	    return SANITY_CHECK_FAILED;
	}

	if (cred->username.whole.len == 0) {
#ifdef EXTRA_DEBUG
	    DBG("sanity_check(): check_digest: Empty username\n");
#endif
	    return SANITY_CHECK_FAILED;
	}
	
	if (cred->nonce.len == 0) {
#ifdef EXTRA_DEBUG
	    DBG("sanity_check(): check_digest: Empty nonce attribute\n");
#endif
	    return SANITY_CHECK_FAILED;
	}

	if (cred->response.len == 0) {
#ifdef EXTRA_DEBUG
	    DBG("sanity_check(): check_digest: Empty response attribute\n");
#endif
	    return SANITY_CHECK_FAILED;
	}

	do {
	    ptr = ptr->next;
	} while(ptr && ptr->type != hf_type);

	if (!ptr && hf_type == HDR_AUTHORIZATION_T) {
	    hf_type = HDR_PROXYAUTH_T;
	    ptr = msg->proxy_auth;
	}
    }

    return SANITY_CHECK_PASSED;
}


/* check for the presence of duplicate tag prameters in To/From headers */
int check_duptags(sip_msg_t* _msg)
{
	to_body_t *tb;
	to_param_t *tp;
	int n;

	if(parse_from_header(_msg)<0 || parse_to_header(_msg)<0) {
		DBG("check_duptags failed while parsing\n");
		return SANITY_CHECK_FAILED;
	}
	tb = get_from(_msg);
	if(tb->tag_value.s!=NULL) {
		n = 0;
		for(tp = tb->param_lst; tp; tp = tp->next) {
			if(tp->type==TAG_PARAM)
				n++;
		}
		if(n>1) {
			DBG("check_duptags failed for From header\n");
			return SANITY_CHECK_FAILED;
		}
	}
	tb = get_to(_msg);
	if(tb->tag_value.s!=NULL) {
		n = 0;
		for(tp = tb->param_lst; tp; tp = tp->next) {
			if(tp->type==TAG_PARAM)
				n++;
		}
		if(n>1) {
			DBG("check_duptags failed for To header\n");
			return SANITY_CHECK_FAILED;
		}
	}

	return SANITY_CHECK_PASSED;
}


