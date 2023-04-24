/*
 * Copyright (C) 2001-2004 FhG Fokus
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
 */

/*!
 * \file
 * \brief Route & Record-Route module, loose routing support
 * \ingroup rr
 */



#include <string.h>
#include "../../core/ut.h"
#include "../../core/str.h"
#include "../../core/dprint.h"
#include "../../core/forward.h"
#include "../../core/data_lump.h"
#include "../../core/socket_info.h"
#include "../../core/parser/parse_rr.h"
#include "../../core/parser/parse_uri.h"
#include "../../core/parser/parse_from.h"
#include "../../core/parser/parse_param.h"
#include "../../core/mem/mem.h"
#include "../../core/dset.h"
#include "loose.h"
#include "rr_cb.h"
#include "rr_mod.h"


#define RR_ROUTE_PREFIX ROUTE_PREFIX "<"
#define RR_ROUTE_PREFIX_LEN (sizeof(RR_ROUTE_PREFIX)-1)

#define ROUTE_SUFFIX ">\r\n"  /*!< SIP header suffix */
#define ROUTE_SUFFIX_LEN (sizeof(ROUTE_SUFFIX)-1)

/*! variables used to hook the param part of the local route */
static msg_ctx_id_t routed_msg_id = {0};
static str routed_params = {0,0};

extern int rr_force_send_socket;
extern int rr_sockname_mode;

/*!
 * \brief Test whether we are processing pre-loaded route set by looking at the To tag
 * \param msg SIP message
 * \return -1 on failure, 0 on success
 */
static int is_preloaded(struct sip_msg* msg)
{
	str tag;

	if (!msg->to && parse_headers(msg, HDR_TO_F, 0) == -1) {
		LM_ERR("failed to parse To header field\n");
		return -1;
	}

	if (!msg->to) {
		LM_ERR("To header field not found\n");
		return -1;
	}

	tag = get_to(msg)->tag_value;
	if (tag.s == 0 || tag.len == 0) {
		LM_DBG("is_preloaded: Yes\n");
		return 1;
	}

	LM_DBG("is_preloaded: No\n");
	return 0;
}


/*!
 * \brief Parse the message and find first occurrence of Route header field.
 * \param _m SIP message 
 * \return -1 or -2 on a parser error, 0 if there is a Route HF and 1 if there is no Route HF
 */
static inline int find_first_route(struct sip_msg* _m)
{
	if (parse_headers(_m, HDR_ROUTE_F, 0) == -1) {
		LM_ERR("failed to parse headers\n");
		return -1;
	} else {
		if (_m->route) {
			if (parse_rr(_m->route) < 0) {
				LM_ERR("failed to parse Route HF\n");
				return -2;
			}
			return 0;
		} else {
			LM_DBG("No Route headers found\n");
			return 1;
		}
	}
}


/*!
 * \brief Check if URI is myself
 * \param _host host
 * \param _port port
 * \return 0 if the URI is not myself, 1 otherwise
 */
static inline int is_myself(sip_uri_t *_puri)
{
	int ret;

	if(_puri->host.len==0) {
		/* catch uri without host (e.g., tel uri) */
		return 0;
	}

	ret = check_self(&_puri->host,
			_puri->port_no?_puri->port_no:SIP_PORT, 0);/* match all protos*/
	if (ret < 0) return 0;

#ifdef ENABLE_USER_CHECK
	if(ret==1 && i_user.len && i_user.len==_puri->user.len
			&& strncmp(i_user.s, _puri->user.s, _puri->user.len)==0)
	{
		LM_DBG("ignore user matched - URI is not to the server itself\n");
		return 0;
	}
#endif

	if(ret==1) {
		/* match on host:port, but if gruu, then fail */
		if(_puri->gr.s!=NULL)
			return 0;
	}

	return ret;
}


/*!
 * \brief Find and parse next Route header field
 * \param _m SIP message
 * \param _hdr SIP header
 * \return negative on failure, 0 if the Route header was already parsed, 1 if no next
 * Route header could be found
 */
static inline int find_next_route(struct sip_msg* _m, struct hdr_field** _hdr)
{
	struct hdr_field* ptr;

	ptr = (*_hdr)->next;

	/* Try to find already parsed Route headers */
	while(ptr) {
		if (ptr->type == HDR_ROUTE_T) goto found;
		ptr = ptr->next;
	}

	/* There are no already parsed Route headers, try to find next
	 * occurrence of Route header
	 */
	if (parse_headers(_m, HDR_ROUTE_F, 1) == -1) {
		LM_ERR("failed to parse headers\n");
		return -1;
	}

	if ((_m->last_header->type!=HDR_ROUTE_T) || (_m->last_header==*_hdr)) {
		LM_DBG("No next Route HF found\n");
		return 1;
	}

	ptr = _m->last_header;

 found:
	if (parse_rr(ptr) < 0) {
		LM_ERR("failed to parse Route body\n");
		return -2;
	}

	*_hdr = ptr;
	return 0;
}


/*!
 * \brief Check if the given uri contains lr parameter which marks loose routers
 * \param _params URI string
 * \return 1 if URI contains no lr parameter, 0 if it contains a lr parameter
 */
static inline int is_strict(str* _params)
{
	str s;
	int i, state = 0;

	if (_params->len == 0) return 1;

	s.s = _params->s;
	s.len = _params->len;

	for(i = 0; i < s.len; i++) {
		switch(state) {
		case 0:
			switch(s.s[i]) {
			case ' ':
			case '\r':
			case '\n':
			case '\t':           break;
			case 'l':
			case 'L': state = 1; break;
			default:  state = 4; break;
			}
			break;

		case 1:
			switch(s.s[i]) {
			case 'r':
			case 'R': state = 2; break;
			default:  state = 4; break;
			}
			break;

		case 2:
			switch(s.s[i]) {
			case ';':  return 0;
			case '=':  return 0;
			case ' ':
			case '\r':
			case '\n':
			case '\t': state = 3; break;
			default:   state = 4; break;
			}
			break;

		case 3:
			switch(s.s[i]) {
			case ';':  return 0;
			case '=':  return 0;
			case ' ':
			case '\r':
			case '\n':
			case '\t': break;
			default:   state = 4; break;
			}
			break;

		case 4:
			switch(s.s[i]) {
			case '\"': state = 5; break;
			case ';':  state = 0; break;
			default:              break;
			}
			break;
			
		case 5:
			switch(s.s[i]) {
			case '\\': state = 6; break;
			case '\"': state = 4; break;
			default:              break;
			}
			break;

		case 6: state = 5; break;
		}
	}
	
	if ((state == 2) || (state == 3)) return 0;
	else return 1;
}


/*!
 * \brief Set Request-URI as last Route header of a SIP
 *
 * Set Request-URI as last Route header of a SIP message,
 * this is necessary when forwarding to a strict router.
 * Allocates memory for message lump in private memory.
 * \param _m SIP message
 * \return negative on failure, 0 on success
 */
static inline int save_ruri(struct sip_msg* _m)
{
	struct lump* anchor;
	char *s;
	int len;

	/* We must parse the whole message header here, because
	 * the Request-URI must be saved in last Route HF in the message
	 */
	if (parse_headers(_m, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse message\n");
		return -1;
	}

	/* Create an anchor */
	anchor = anchor_lump(_m, _m->unparsed - _m->buf, 0, 0);
	if (anchor == 0) {
		LM_ERR("failed to get anchor\n");
		return -2;
	}

	/* Create buffer for new lump */
	len = RR_ROUTE_PREFIX_LEN + _m->first_line.u.request.uri.len
			+ ROUTE_SUFFIX_LEN;
	s = (char*)pkg_malloc(len);
	if (!s) {
		PKG_MEM_ERROR;
		return -3;
	}

	/* Create new header field */
	memcpy(s, RR_ROUTE_PREFIX, RR_ROUTE_PREFIX_LEN);
	memcpy(s + RR_ROUTE_PREFIX_LEN, _m->first_line.u.request.uri.s,
			_m->first_line.u.request.uri.len);
	memcpy(s + RR_ROUTE_PREFIX_LEN + _m->first_line.u.request.uri.len,
			ROUTE_SUFFIX, ROUTE_SUFFIX_LEN);

	LM_DBG("New header: '%.*s'\n", len, ZSW(s));

	/* Insert it */
	if (insert_new_lump_before(anchor, s, len, 0) == 0) {
		pkg_free(s);
		LM_ERR("failed to insert lump\n");
		return -4;
	}

	return 0;
}


/*!
 * \brief Parse URI and check if it has a maddr parameter
 * \param uri URI to be checked if it has maddr, and also the output URI
 * \param puri parsed URI
 * \return -1 on failure, 0 on success
 */
static inline int get_maddr_uri(str *uri, struct sip_uri *puri)
{
	/* The max length of the maddr parameter is 127 */
	static char builturi[127+1];
	struct sip_uri turi;

	if(uri==NULL || uri->s==NULL)
		return RR_ERROR;
	if(puri==NULL)
	{
		if (parse_uri(uri->s, uri->len, &turi) < 0)
		{
			LM_ERR("failed to parse the URI\n");
			return RR_ERROR;
		}
		puri = &turi;
	}

	if(puri->maddr.s==NULL)
		return 0;

	/* sip: + maddr + : + port */
	if( (puri->maddr_val.len) > (127 - 10) )
	{
		LM_ERR( "Too long maddr parameter\n");
		return RR_ERROR;
	}
	memcpy( builturi, "sip:", 4 );
	memcpy( builturi+4, puri->maddr_val.s, puri->maddr_val.len );
		
	if( puri->port.len > 0 )
	{
		builturi[4+puri->maddr_val.len] =':';
		memcpy(builturi+5+puri->maddr_val.len, puri->port.s,
				puri->port.len);
	}

	uri->len = 4+puri->maddr_val.len
					+ ((puri->port.len>0)?(1+puri->port.len):0);
	builturi[uri->len]='\0';
	uri->s = builturi;

	LM_DBG("uri is %s\n", builturi );
	return 0;
}


/*!
 * \brief Necessary logic to forward request to strict routers
 * \param _m SIP message
 * \param _hdr SIP header field
 * \param _r Route & Record-Route header field body
 * \return 0 on success, negative on an error
 */
static inline int handle_sr(struct sip_msg* _m, struct hdr_field* _hdr, rr_t* _r)
{
	str uri;
	char* rem_off;
	int rem_len;

	/* Next hop is strict router, save R-URI here */
	if (save_ruri(_m) < 0) {
		LM_ERR("failed to save Request-URI\n");
		return -1;
	}
	
	/* Put the first Route in Request-URI */
	uri = _r->nameaddr.uri;
	if(get_maddr_uri(&uri, 0)!=0) {
		LM_ERR("failed to check maddr\n");
		return RR_ERROR;
	}
	if (rewrite_uri(_m, &uri) < 0) {
		LM_ERR("failed to rewrite request URI\n");
		return -2;
	}

	if (!_r->next) {
		rem_off = _hdr->name.s;
		rem_len = _hdr->len;
	} else {
		rem_off = _hdr->body.s;
		rem_len = _r->next->nameaddr.name.s - _hdr->body.s;
	}

	if (!del_lump(_m, rem_off - _m->buf, rem_len, 0)) {
		LM_ERR("failed to remove Route HF\n");
		return -9;
	}			

	return 0;
}


/*!
 * \brief Find last route in the last Route header field
 *
 * Find last route in the last Route header field, if there was a previous
 * route in the last Route header field, it will be saved in _p parameter
 * \param _m SIP message
 * \param _h SIP header field
 * \param _l Route & Record-Route header field body
 * \param _p Route & Record-Route header field body
 * \return negative on failure, 0 on success
 */
static inline int find_rem_target(struct sip_msg* _m, struct hdr_field** _h, rr_t** _l, rr_t** _p)
{
	struct hdr_field* ptr, *last;

	if (parse_headers(_m, HDR_EOH_F, 0) == -1) {
		LM_ERR("failed to parse message header\n");
		return -1;
	}

	ptr = _m->route;
	last = 0;

	while(ptr) {
		if (ptr->type == HDR_ROUTE_T) last = ptr;
		ptr = ptr->next;
	}

	if (last) {
		if (parse_rr(last) < 0) {
			LM_ERR("failed to parse last Route HF\n");
			return -2;
		}

		*_p = 0;
		*_l = (rr_t*)last->parsed;
		*_h = last;
		while ((*_l)->next) {
			*_p = *_l;
			*_l = (*_l)->next;
		}
		return 0;
	} else {
		LM_ERR("search for last Route HF failed\n");
		return 1;
	}
}

/* Largest route URI is of the form:
	sip:[1234:5678:9012:3456:7890:1234:5678:9012]:12345;transport=sctp
   this is 66 characters long */
#define MAX_ROUTE_URI_LEN	66
static char uri_buf[MAX_ROUTE_URI_LEN];

/*!
 * \brief Perform outbound processing - force local socket and set destination URI
 * \param _m SIP message
 * \param flow_token string containing the flow-token extracted from the Route: header
 * \param dst_uri string to write the destination URI to (extracted from flow-token)
 * \return -1 on error, 0 when outbound not in use, 1 when outbound in use
 */
static inline int process_outbound(struct sip_msg *_m, str flow_token)
{
	int ret;
	struct receive_info *rcv = NULL;
	struct socket_info *si;
	str dst_uri;

	if (!rr_obb.decode_flow_token)
		return 0;

	ret = rr_obb.decode_flow_token(_m, &rcv, flow_token);

	if (ret == -2) {
		LM_DBG("no flow token found - outbound not in use\n");
		return 0;
	} else if (ret == -1) {
		LM_INFO("failed to decode flow token\n");
		return -1;
	} else if (!ip_addr_cmp(&rcv->src_ip, &_m->rcv.src_ip)
			|| rcv->src_port != _m->rcv.src_port) {
		LM_DBG("\"incoming\" request found. Using flow-token for "
			"routing\n");

		/* First, force the local socket */
		si = find_si(&rcv->dst_ip, rcv->dst_port, rcv->proto);
		if (si)
			set_force_socket(_m, si);
		else {
			LM_INFO("cannot find socket from flow-token\n");
			return -1;
		}

		/* Second, override the destination URI */
		dst_uri.s = uri_buf;
		dst_uri.len = 0;

		dst_uri.len += snprintf(dst_uri.s + dst_uri.len,
					MAX_ROUTE_URI_LEN - dst_uri.len,
					"sip:%s",
					rcv->src_ip.af == AF_INET6 ? "[" : "");
		dst_uri.len += ip_addr2sbuf(&rcv->src_ip,
					dst_uri.s + dst_uri.len,
					MAX_ROUTE_URI_LEN - dst_uri.len);
		dst_uri.len += snprintf(dst_uri.s + dst_uri.len,
					MAX_ROUTE_URI_LEN - dst_uri.len,
					"%s:%d;transport=%s",
					rcv->src_ip.af == AF_INET6 ? "]" : "",
					rcv->src_port,
					get_proto_name(rcv->proto));

		if (set_dst_uri(_m, &dst_uri) < 0) {
			LM_ERR("failed to set dst_uri\n");
			return -1;
		}
		ruri_mark_new();

		return 1;
	}

	LM_DBG("Not using flow-token for routing\n");
	return 0;
}

/*!
 * \brief Previous hop was a strict router, handle this case
 * \param _m SIP message
 * \return -1 on error, 1 on success
 */
static inline int after_strict(struct sip_msg* _m)
{
	int res, rem_len;
	struct hdr_field* hdr;
	struct sip_uri puri;
	rr_t* rt, *prev;
	char* rem_off;
	str uri;
	struct socket_info *si;

	hdr = _m->route;
	rt = (rr_t*)hdr->parsed;
	uri = rt->nameaddr.uri;

	/* reset rr handling static vars for safety in error case */
	routed_msg_id.msgid = 0;
	routed_msg_id.pid = 0;
	routed_params.s = NULL;
	routed_params.len = 0;

	if (parse_uri(uri.s, uri.len, &puri) < 0) {
		LM_ERR("failed to parse the first route URI\n");
		return RR_ERROR;
	}

	if ( enable_double_rr && is_2rr(&puri.params) && is_myself(&puri)) {
		/* double route may occur due different IP and port, so force as
		 * send interface the one advertise in second Route */
		si = grep_sock_info( &puri.host, puri.port_no, puri.proto);
		if (si) {
			set_force_socket(_m, si);
		} else {
			if (enable_socket_mismatch_warning)
				LM_WARN("no socket found for match second RR\n");
		}

		if (!rt->next) {
			/* No next route in the same header, remove the whole header
			 * field immediately
			 */
			if (!del_lump(_m, hdr->name.s - _m->buf, hdr->len, 0)) {
				LM_ERR("failed to remove Route HF\n");
				return RR_ERROR;
			}
			res = find_next_route(_m, &hdr);
			if (res < 0) {
				LM_ERR("searching next route failed\n");
				return RR_ERROR;
			}
			if (res > 0) { /* No next route found */
				LM_DBG("after_strict: No next URI found\n");
				return RR_NOT_DRIVEN;
			}
			rt = (rr_t*)hdr->parsed;
		} else rt = rt->next;

		/* parse the new found uri */
		uri = rt->nameaddr.uri;
		if (parse_uri(uri.s, uri.len, &puri) < 0) {
			LM_ERR("failed to parse URI\n");
			return RR_ERROR;
		}
	}

	/* set the hooks for the param
	 * important note: RURI is already parsed by the above function, so 
	 * we just used it without any checking */
	routed_msg_id.msgid = _m->id;
	routed_msg_id.pid = _m->pid;
	routed_params = _m->parsed_uri.params;

	if (is_strict(&puri.params)) {
		LM_DBG("Next hop: '%.*s' is strict router\n", uri.len, ZSW(uri.s));
		/* Previous hop was a strict router and the next hop is strict
		 * router too. There is no need to save R-URI again because it
		 * is saved already. In fact, in this case we will behave exactly
		 * like a strict router. */

		/* Note: when there is only one Route URI left (endpoint), it will
		 * always be a strict router because endpoints don't use ;lr parameter
		 * In this case we will simply put the URI in R-URI and forward it, 
		 * which will work perfectly */
		if(get_maddr_uri(&uri, &puri)!=0) {
			LM_ERR("failed to check maddr\n");
			return RR_ERROR;
		}
		if (rewrite_uri(_m, &uri) < 0) {
			LM_ERR("failed to rewrite request URI\n");
			return RR_ERROR;
		}
		
		if (rt->next) {
			rem_off = hdr->body.s;
			rem_len = rt->next->nameaddr.name.s - hdr->body.s;
		} else {
			rem_off = hdr->name.s;
			rem_len = hdr->len;
		}
		if (!del_lump(_m, rem_off - _m->buf, rem_len, 0)) {
			LM_ERR("failed to remove Route HF\n");
			return RR_ERROR;
		}
	} else {
		LM_DBG("Next hop: '%.*s' is loose router\n",
			uri.len, ZSW(uri.s));

		if(get_maddr_uri(&uri, &puri)!=0) {
			LM_ERR("failed to check maddr\n");
			return RR_ERROR;
		}
		if (set_dst_uri(_m, &uri) < 0) {
			LM_ERR("failed to set dst_uri\n");
			return RR_ERROR;
		}

		/* Next hop is a loose router - Which means that is is not endpoint yet
		 * In This case we have to recover from previous strict routing, that 
		 * means we have to find the last Route URI and put in in R-URI and 
		 * remove the last Route URI. */
		if (rt != hdr->parsed) {
			/* There is a previous route uri which was 2nd uri of mine
			 * and must be removed here */
			rem_off = hdr->body.s;
			rem_len = rt->nameaddr.name.s - hdr->body.s;
			if (!del_lump(_m, rem_off - _m->buf, rem_len, 0)) {
				LM_ERR("failed to remove Route HF\n");
				return RR_ERROR;
			}
		}


		res = find_rem_target(_m, &hdr, &rt, &prev);
		if (res < 0) {
			LM_ERR("searching for last Route URI failed\n");
			return RR_ERROR;
		} else if (res > 0) {
			/* No remote target is an error */
			return RR_ERROR;
		}

		uri = rt->nameaddr.uri;
		if(get_maddr_uri(&uri, 0)!=0) {
			LM_ERR("checking maddr failed\n");
			return RR_ERROR;
		}
		if (rewrite_uri(_m, &uri) < 0) {
			LM_ERR("failed to rewrite R-URI\n");
			return RR_ERROR;
		}

		/* The first character if uri will be either '<' when it is the 
		 * only URI in a Route header field or ',' if there is more than 
		 * one URI in the header field */
		LM_DBG("The last route URI: '%.*s'\n", rt->nameaddr.uri.len,
				ZSW(rt->nameaddr.uri.s));

		if (prev) {
			rem_off = prev->nameaddr.name.s + prev->len;
			rem_len = rt->nameaddr.name.s + rt->len - rem_off;
		} else {
			rem_off = hdr->name.s;
			rem_len = hdr->len;
		}
		if (!del_lump(_m, rem_off - _m->buf, rem_len, 0)) {
			LM_ERR("failed to remove Route HF\n");
			return RR_ERROR;
		}
	}

	/* run RR callbacks only if we have Route URI parameters */
	if(routed_params.len > 0)
		run_rr_callbacks( _m, &routed_params );

	return RR_DRIVEN;
}


static inline void rr_do_force_send_socket(sip_msg_t *_m, sip_uri_t *puri,
		rr_t* rt, int rr2on)
{
	socket_info_t *si = NULL;
	param_hooks_t phooks;
	param_t* plist = NULL;
	param_t *pit=NULL;
	str s;


	if(rr_sockname_mode!=0 && puri->params.len>0) {
		s = puri->params;
		if(s.s[s.len-1]==';') {
			s.len--;
		}
		if (parse_params(&s, CLASS_ANY, &phooks, &plist)<0) {
			LM_ERR("bad sip uri parameters: %.*s\n", s.len, s.s);
			return;
		}
		for (pit = plist; pit; pit=pit->next) {
			if (pit->name.len==SOCKNAME_ATTR_LEN
					&& strncasecmp(pit->name.s, SOCKNAME_ATTR,
							SOCKNAME_ATTR_LEN)==0) {
				if(pit->body.len>0) {
					si = ksr_get_socket_by_name(&pit->body);
					if(si != NULL) {
						LM_DBG("found socket with name: %.*s\n",
								pit->body.len, pit->body.s);
						set_force_socket(_m, si);
						free_params(plist);
						return;
					} else {
						LM_DBG("failed to find socket with name: %.*s\n",
								pit->body.len, pit->body.s);
					}
				}
			}
		}
		LM_DBG("use of sockname parameter enabled, but failed to find it\n");
		free_params(plist);
	}

	if ((si = grep_sock_info(&puri->host,
				puri->port_no?puri->port_no:proto_default_port(puri->proto),
				puri->proto)) != 0) {
		LM_DBG("set send socket %p for local route uri: %.*s\n", si,
				rt->nameaddr.uri.len, ZSW(rt->nameaddr.uri.s));
		set_force_socket(_m, si);
	} else if ((si = grep_sock_info(&puri->host, puri->port_no,
					puri->proto)) != 0) {
		LM_DBG("set send socket %p for local route uri: %.*s\n", si,
				rt->nameaddr.uri.len, ZSW(rt->nameaddr.uri.s));
		set_force_socket(_m, si);
	} else {
		if (enable_socket_mismatch_warning && rr2on) {
			LM_WARN("no socket found to match second RR (%.*s)\n",
					rt->nameaddr.uri.len, ZSW(rt->nameaddr.uri.s));
			if(!is_myself(puri)) {
				LM_WARN("second RR uri is not myself (%.*s)\n",
						rt->nameaddr.uri.len, ZSW(rt->nameaddr.uri.s));
			}
		} else {
			LM_DBG("no socket found to match second RR (%.*s)\n",
					rt->nameaddr.uri.len, ZSW(rt->nameaddr.uri.s));
		}
	}
}

/*!
 * \brief Previous hop was a loose router, handle this case
 * \param _m SIP message
 * \param preloaded do we have a preloaded route set
 * \return -1 on failure, 1 on success
 */
static inline int after_loose(struct sip_msg* _m, int preloaded)
{
	struct hdr_field* hdr;
	struct sip_uri puri;
	rr_t* rt;
	int res;
	int status = RR_DRIVEN;
	str uri;
	int uri_is_myself;
	int use_ob = 0;
	str rparams;

	hdr = _m->route;
	rt = (rr_t*)hdr->parsed;
	uri = rt->nameaddr.uri;

	/* reset rr handling static vars for safety in error case */
	routed_msg_id.msgid = 0;
	routed_msg_id.pid = 0;

	if (parse_uri(uri.s, uri.len, &puri) < 0) {
		LM_ERR("failed to parse the first route URI (%.*s)\n",
				uri.len, ZSW(uri.s));
		return RR_ERROR;
	}

	routed_params = puri.params;
	uri_is_myself = is_myself(&puri);

	/* IF the URI was added by me, remove it */
	if (uri_is_myself>0)
	{
		LM_DBG("Topmost route URI: '%.*s' is me\n",
			uri.len, ZSW(uri.s));
		/* set the hooks for the params */
		routed_msg_id.msgid = _m->id;
		routed_msg_id.pid = _m->pid;

		if ((use_ob = process_outbound(_m, puri.user)) < 0) {
			LM_INFO("failed to process outbound flow-token\n");
			return RR_FLOW_TOKEN_BROKEN;
		}

		if (rr_force_send_socket && !use_ob) {
			if (!enable_double_rr || !is_2rr(&puri.params)) {
				rr_do_force_send_socket(_m, &puri, rt, 0);
			}
		}
		if (!rt->next) {
			/* No next route in the same header, remove the whole header
			 * field immediately
			 */
			if (!del_lump(_m, hdr->name.s - _m->buf, hdr->len, 0)) {
				LM_ERR("failed to remove Route HF\n");
				return RR_ERROR;
			}

			res = find_next_route(_m, &hdr);
			if (res < 0) {
				LM_ERR("failed to find next route\n");
				return RR_ERROR;
			}
			if (res > 0) { /* No next route found */
				LM_DBG("No next URI found\n");
				status = (preloaded ? RR_PRELOADED : RR_DRIVEN);
				goto done;
			}
			rt = (rr_t*)hdr->parsed;
		} else rt = rt->next;

		if (enable_double_rr && is_2rr(&puri.params)) {
			/* double route may occur due different IP and port, so force as
			 * send interface the one advertise in second Route */
			if (parse_uri(rt->nameaddr.uri.s,rt->nameaddr.uri.len,&puri)<0) {
				LM_ERR("failed to parse the double route URI (%.*s)\n",
						rt->nameaddr.uri.len, ZSW(rt->nameaddr.uri.s));
				return RR_ERROR;
			}

			if (!use_ob) {
				rr_do_force_send_socket(_m, &puri, rt, 1);
			}

			if (!rt->next) {
				/* No next route in the same header, remove the whole header
				 * field immediately */
				if (!del_lump(_m, hdr->name.s - _m->buf, hdr->len, 0)) {
					LM_ERR("failed to remove Route HF\n");
					return RR_ERROR;
				}
				res = find_next_route(_m, &hdr);
				if (res < 0) {
					LM_ERR("failed to find next route\n");
					return RR_ERROR;
					}
				if (res > 0) { /* No next route found */
					LM_DBG("no next URI found\n");
					status = (preloaded ? RR_PRELOADED : RR_DRIVEN);
					goto done;
				}
				rt = (rr_t*)hdr->parsed;
			} else rt = rt->next;
		}

		uri = rt->nameaddr.uri;
		if (parse_uri(uri.s, uri.len, &puri) < 0) {
			LM_ERR("failed to parse the next route URI (%.*s)\n",
					uri.len, ZSW(uri.s));
			return RR_ERROR;
		}
		_m->msg_flags |= FL_ROUTE_ADDR;
	} else {
#ifdef ENABLE_USER_CHECK
		/* check if it the ignored user */
		if(uri_is_myself < 0)
			return RR_NOT_DRIVEN;
#endif
		LM_DBG("Topmost URI is NOT myself\n");
		routed_params.s = NULL;
		routed_params.len = 0;
	}

	LM_DBG("URI to be processed: '%.*s'\n", uri.len, ZSW(uri.s));
	if (is_strict(&puri.params)) {
		LM_DBG("Next URI is a strict router\n");
		if (handle_sr(_m, hdr, rt) < 0) {
			LM_ERR("failed to handle strict router\n");
			return RR_ERROR;
		}
	} else {
		/* Next hop is loose router */
		LM_DBG("Next URI is a loose router\n");

		if (!use_ob) {
			if(get_maddr_uri(&uri, &puri)!=0) {
				LM_ERR("checking maddr failed\n");
				return RR_ERROR;
			}

			if (set_dst_uri(_m, &uri) < 0) {
				LM_ERR("failed to set dst_uri\n");
				return RR_ERROR;
			}
			/* dst_uri changed, so it makes sense to re-use the current uri for
			forking */
			ruri_mark_new(); /* re-use uri for serial forking */
		}

		/* There is a previous route uri which was 2nd uri of mine
		 * and must be removed here */
		if (rt != hdr->parsed) {
			if (!del_lump(_m, hdr->body.s - _m->buf,
			rt->nameaddr.name.s - hdr->body.s, 0)) {
				LM_ERR("failed to remove Route HF\n");
				return RR_ERROR;
			}
		}
	}

done:
	if (use_ob == 1)
		status = RR_OB_DRIVEN;

	/* run RR callbacks only if we have Route URI parameters */
	if(routed_params.len > 0) {
		rparams = routed_params;
		run_rr_callbacks( _m, &rparams );
	}
	return status;
}


/*!
 * \brief Do loose routing as per RFC3261
 * \param _m SIP message
 * \param _mode - 0: try loose or strict routing; 1: try loose routing only
 * \return negative on failure or preloaded, 1 on success
 */
int loose_route_mode(sip_msg_t* _m, int _mode)
{
	int ret;

	if (find_first_route(_m) != 0) {
		LM_DBG("There is no Route HF\n");
		return -1;
	}

	if (parse_sip_msg_uri(_m)<0) {
		LM_ERR("failed to parse Request URI\n");
		return -1;
	}

	ret = is_preloaded(_m);
	if (ret < 0) {
		return -1;
	} else if (ret == 1) {
		return after_loose(_m, 1);
	} else {
		if ((_mode==0) && (is_myself(&_m->parsed_uri))) {
			return after_strict(_m);
		} else {
			return after_loose(_m, 0);
		}
	}
}

/*!
 * \brief Do loose routing as per RFC3261
 * \param _m SIP message
 * \return negative on failure or preloaded, 1 on success
 */
int loose_route(struct sip_msg* _m)
{
	return loose_route_mode(_m, 0);
}

/**
 *
 */
int redo_route_params(sip_msg_t *msg)
{
	hdr_field_t *hdr;
	sip_uri_t puri;
	rr_t* rt;
	str uri;
	int uri_is_myself;

	int redo = 0;

	if(msg->first_line.type != SIP_REQUEST) {
		return -1;
	}

	if(msg->route==NULL) {
		return -1;
	}

	if(msg->route->parsed==NULL) {
		if (parse_rr(msg->route) < 0) {
			LM_ERR("failed to parse Route HF\n");
			return -1;
		}
	}

	if(msg->route->parsed==NULL) {
		LM_ERR("NULL parsed Route header\n");
		return -1;
	}

	/* check if the hooked params belong to the same message */
	if (routed_msg_id.msgid != msg->id || routed_msg_id.pid != msg->pid) {
		redo = 1;
	}
	if((redo==0) && (routed_params.s==NULL || routed_params.len<=0)) {
		redo = 1;
	}
	if((redo==0) && (routed_params.s<msg->buf
				|| routed_params.s>msg->buf+msg->len)) {
		redo = 1;
	}
	if(redo==1) {
		hdr = msg->route;
		rt = (rr_t*)hdr->parsed;
		uri = rt->nameaddr.uri;

		/* reset rr handling static vars for safety in error case */
		routed_msg_id.msgid = 0;
		routed_msg_id.pid = 0;

		if (parse_uri(uri.s, uri.len, &puri) < 0) {
			LM_ERR("failed to parse the first route URI (%.*s)\n",
					uri.len, ZSW(uri.s));
			return -1;
		}

		uri_is_myself = is_myself(&puri);

		/* if the URI was added by me, remove it */
		if (uri_is_myself>0) {
			LM_DBG("Topmost route URI: '%.*s' is me\n",
				uri.len, ZSW(uri.s));
			/* set the hooks for the params */
			routed_msg_id.msgid = msg->id;
			routed_msg_id.pid = msg->pid;
			routed_params = puri.params;
			return 0;
		} else {
			return -1;
		}
	}
	return 0;
}

/*!
 * \brief Check if the route hdr has the required parameter
 *
 * The function checks for the request "msg" if the URI parameters
 * of the local Route header (corresponding to the local server)
 * matches the given regular expression "re". It must be called
 * after the loose_route was done.
 *
 * \param msg SIP message request that will has the Route header parameters checked
 * \param re compiled regular expression to be checked against the Route header parameters
 * \return -1 on failure, 1 on success
 */
int check_route_param(sip_msg_t * msg, regex_t* re)
{
	regmatch_t pmatch;
	char bk;
	str params;
	str rruri;

	/* check if the hooked params belong to the same message */
	if(redo_route_params(msg)<0) {
		return -1;
	}

	/* check if params are present */
	if ( !routed_params.s || routed_params.len<=0 ) {
		return -1;
	}
	rruri = ((rr_t*)(msg->route->parsed))->nameaddr.uri;

	/* include also the first ';' */
	for( params=routed_params ;
			params.s>rruri.s && params.s[0]!=';' ;
			params.s--,params.len++ );

	LM_DBG("route params checking against [%.*s] (orig: [%.*s])\n",
			params.len, params.s, routed_params.len, routed_params.s);

	/* do the well-known trick to convert to null terminted */
	bk = params.s[params.len];
	params.s[params.len] = 0;
	LM_DBG("params are <%s>\n", params.s);
	if (regexec( re, params.s, 1, &pmatch, 0)!=0) {
		params.s[params.len] = bk;
		return -1;
	} else {
		params.s[params.len] = bk;
		return 0;
	}
}


/*!
 * \brief Gets the value of a route parameter
 *
 * The function search in to the "msg"'s Route header parameters
 * the parameter called "name" and returns its value into "val".
 * It must be called only after the loose_route is done.
 *
 * \param msg - request that will have the Route header parameter searched
 * \param name - contains the Route header parameter to be serached
 * \param val returns the value of the searched Route header parameter if found.
 * It might be an empty string if the parameter had no value.
 * \return 0 if parameter was found (even if it has no value), -1 otherwise
 */
int get_route_param(sip_msg_t *msg, str *name, str *val)
{
	char *p;
	char *end;
	char c;
	int quoted;

	/* check if the hooked params belong to the same message */
	if(redo_route_params(msg)<0) {
		goto notfound;
	}

	/* check if params are present */
	if ( !routed_params.s || routed_params.len<=0 )
		goto notfound;

	end = routed_params.s + routed_params.len;
	p = routed_params.s;


	/* parse the parameters string and find the "name" param */
	while ( end-p>name->len+2 ) {
		if (p!=routed_params.s) {
			/* go to first ';' char */
			for( quoted=0 ; p<end && !(*p==';' && !quoted) ; p++ )
				if ( (*p=='"' || *p=='\'') && *(p-1)!='\\' )
					quoted ^= 0x1;
			if (p==end)
				goto notfound;
			p++;
		}
		/* get first non space char */
		while( p<end && (*p==' ' || *p=='\t') )
			p++;
		/* check the name - length first and content after */
		if ( end-p<name->len+2 )
			goto notfound;
		if ( memcmp(p,name->s,name->len)!=0 ) {
			p++;
			continue;
		}
		p+=name->len;
		while( p<end && (*p==' ' || *p=='\t') )
			p++;
		if (p==end|| *p==';') {
			/* empty val */
			val->len = 0;
			val->s = 0;
			goto found;
		}
		if (*(p++)!='=')
			continue;
		while( p<end && (*p==' ' || *p=='\t') )
			p++;
		if (p==end)
			goto notfound;
		/* get value */
		if ( *p=='\'' || *p=='"') {
			for( val->s = ++p ; p<end ; p++) {
				if ((*p=='"' || *p=='\'') && *(p-1)!='\\' )
					break;
			}
		} else {
			for( val->s=p ; p<end ; p++) {
				if ( (c=*p)==';' || c==' ' || c=='\t' )
					break;
			}
		}
		val->len = p-val->s;
		if (val->len==0)
			val->s = 0;
		goto found;
	}

notfound:
	return -1;
found:
	return 0;
}


/*!
 * \brief Check the direction of the message
 *
 * The function checks the flow direction of the request "msg". As
 * for checking it's used the "ftag" Route header parameter, the
 * append_fromtag module parameter must be enables.
 * Also this must be called only after the loose_route is done.

 * \param msg SIP message request that will have the direction checked
 * \param dir direction to be checked against. It may be RR_FLOW_UPSTREAM or RR_FLOW_DOWNSTREAM
 * \return 0 if the request flow direction is the same as the given direction, -1 otherwise
 */
int is_direction(struct sip_msg * msg, int dir)
{
	static str ftag_param = {"ftag",4};
	static msg_ctx_id_t last_id = {0};
	static unsigned int last_dir = 0;
	str ftag_val;
	str tag;

	if ( last_id.msgid==msg->id && last_id.pid==msg->pid && last_dir!=0) {
		if (last_dir==RR_FLOW_UPSTREAM)
			goto upstream;
		else
			goto downstream;
	}

	ftag_val.s = 0;
	ftag_val.len = 0;

	if (get_route_param( msg, &ftag_param, &ftag_val)!=0) {
		LM_DBG("param ftag not found\n");
		goto downstream;
	}

	if ( ftag_val.s==0 || ftag_val.len==0 ) {
		LM_DBG("param ftag has empty val\n");
		goto downstream;
	}

	/* get the tag value from FROM hdr */
	if ( parse_from_header(msg)!=0 )
		goto downstream;

	tag = ((struct to_body*)msg->from->parsed)->tag_value;
	if (tag.s==0 || tag.len==0)
		goto downstream;

	/* compare the 2 strings */
	if (tag.len!=ftag_val.len || memcmp(tag.s,ftag_val.s,ftag_val.len))
		goto upstream;

downstream:
	last_id.msgid = msg->id;
	last_id.pid = msg->pid;
	last_dir = RR_FLOW_DOWNSTREAM;
	return (dir==RR_FLOW_DOWNSTREAM)?0:-1;
upstream:
	last_id.msgid = msg->id;
	last_id.pid = msg->pid;
	last_dir = RR_FLOW_UPSTREAM;
	return (dir==RR_FLOW_UPSTREAM)?0:-1;
}
