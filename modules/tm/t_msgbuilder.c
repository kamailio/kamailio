/*
 * message printing
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

#include "defs.h"

#ifdef EXTRA_DEBUG
#include <assert.h>
#endif
#include "../../comp_defs.h"
#include "../../hash_func.h"
#include "../../globals.h"
#include "t_funcs.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../parser/parse_to.h"
#include "../../ut.h"
#include "../../parser/msg_parser.h"
#include "../../parser/contact/parse_contact.h"
#include "lw_parser.h"
#include "t_msgbuilder.h"
#include "uac.h"
#ifdef USE_DNS_FAILOVER
#include "../../dns_cache.h"
#include "../../cfg_core.h" /* cfg_get(core, core_cfg, use_dns_failover) */
#endif


/* convenience macros */
#define memapp(_d,_s,_len) \
	do{\
		memcpy((_d),(_s),(_len));\
		(_d) += (_len);\
	}while(0)


/* Build a local request based on a previous request; main
   customers of this function are local ACK and local CANCEL
 */
char *build_local(struct cell *Trans,unsigned int branch,
	unsigned int *len, char *method, int method_len, str *to
#ifdef CANCEL_REASON_SUPPORT
	, struct cancel_reason* reason
#endif /* CANCEL_REASON_SUPPORT */
	)
{
	char                *cancel_buf, *p, *via;
	unsigned int         via_len;
	struct hdr_field    *hdr;
	char branch_buf[MAX_BRANCH_PARAM_LEN];
	int branch_len;
	str branch_str;
	str via_id;
	struct hostport hp;
#ifdef CANCEL_REASON_SUPPORT
	int reason_len, code_len;
	struct hdr_field *reas1, *reas_last;
#endif /* CANCEL_REASON_SUPPORT */

	/* init */
	via_id.s=0;
	via_id.len=0;

	/* method, separators, version: "CANCEL sip:p2@iptel.org SIP/2.0" */
	*len=SIP_VERSION_LEN + method_len + 2 /* spaces */ + CRLF_LEN;
	*len+=Trans->uac[branch].uri.len;

	/*via*/
	if (!t_calc_branch(Trans,  branch, 
		branch_buf, &branch_len ))
		goto error;
	branch_str.s=branch_buf;
	branch_str.len=branch_len;
	set_hostport(&hp, (is_local(Trans))?0:(Trans->uas.request));
#ifdef USE_TCP
	if (!is_local(Trans) && ((Trans->uas.request->rcv.proto==PROTO_TCP)
#ifdef USE_TLS
				|| (Trans->uas.request->rcv.proto==PROTO_TLS)
#endif /* USE_TLS */
		)){
		if ((via_id.s=id_builder(Trans->uas.request,
									(unsigned int*)&via_id.len))==0){
			LOG(L_ERR, "ERROR: build_local: id builder failed\n");
			/* try to continue without id */
		}
	}
#endif /* USE_TCP */
	via=via_builder(&via_len, &Trans->uac[branch].request.dst,
		&branch_str, via_id.s?&via_id:0 , &hp );
	
	/* via_id.s not needed anylonger => free it */
	if (via_id.s){
		pkg_free(via_id.s);
		via_id.s=0;
		via_id.len=0;
	}
	
	if (!via)
	{
		LOG(L_ERR, "ERROR: build_local: "
			"no via header got from builder\n");
		goto error;
	}
	*len+= via_len;
	/*headers*/
	*len+=Trans->from.len+Trans->callid.len+to->len+
		+Trans->cseq_n.len+1+method_len+CRLF_LEN+MAXFWD_HEADER_LEN; 


	/* copy'n'paste Route headers */
	if (!is_local(Trans)) {
		for ( hdr=Trans->uas.request->headers ; hdr ; hdr=hdr->next )
			 if (hdr->type==HDR_ROUTE_T)
				*len+=hdr->len;
	}

	/* User Agent */
	if (server_signature) {
		*len += user_agent_hdr.len + CRLF_LEN;
	}
	/* Content Length, EoM */
	*len+=CONTENT_LENGTH_LEN+1 + CRLF_LEN;
#ifdef CANCEL_REASON_SUPPORT
	reason_len = 0;
	reas1 = 0;
	reas_last = 0;
	/* compute reason size (if no reason or disabled => reason_len == 0)*/
	if (reason && reason->cause != CANCEL_REAS_UNKNOWN){
		if (likely(reason->cause > 0 &&
					cfg_get(tm, tm_cfg, local_cancel_reason))){
			/* Reason: SIP;cause=<reason->cause>[;text=<reason->u.text.s>] */
			reason_len = REASON_PREFIX_LEN + USHORT2SBUF_MAX_LEN +
				(reason->u.text.s?
					REASON_TEXT_LEN + 1 + reason->u.text.len + 1 : 0) +
				CRLF_LEN;
		} else if (likely(reason->cause == CANCEL_REAS_PACKED_HDRS &&
					!(Trans->flags & T_NO_E2E_CANCEL_REASON))) {
			reason_len = reason->u.packed_hdrs.len;
		} else if (reason->cause == CANCEL_REAS_RCVD_CANCEL &&
					reason->u.e2e_cancel &&
					!(Trans->flags & T_NO_E2E_CANCEL_REASON)) {
			/* parse the entire cancel, to get all the Reason headers */
			parse_headers(reason->u.e2e_cancel, HDR_EOH_F, 0);
			for(hdr=get_hdr(reason->u.e2e_cancel, HDR_REASON_T), reas1=hdr;
					hdr; hdr=next_sibling_hdr(hdr)) {
				/* hdr->len includes CRLF */
				reason_len += hdr->len;
				reas_last=hdr;
			}
		} else if (unlikely(reason->cause < CANCEL_REAS_MIN))
			BUG("unhandled reason cause %d\n", reason->cause);
	}
	*len+= reason_len;
#endif /* CANCEL_REASON_SUPPORT */
	*len+= CRLF_LEN; /* end of msg. */

	cancel_buf=shm_malloc( *len+1 );
	if (!cancel_buf)
	{
		LOG(L_ERR, "ERROR: build_local: cannot allocate memory\n");
		goto error01;
	}
	p = cancel_buf;

	append_str( p, method, method_len );
	append_str( p, " ", 1 );
	append_str( p, Trans->uac[branch].uri.s, Trans->uac[branch].uri.len);
	append_str( p, " " SIP_VERSION CRLF, 1+SIP_VERSION_LEN+CRLF_LEN );

	/* insert our via */
	append_str(p,via,via_len);

	/*other headers*/
	append_str( p, Trans->from.s, Trans->from.len );
	append_str( p, Trans->callid.s, Trans->callid.len );
	append_str( p, to->s, to->len );

	append_str( p, Trans->cseq_n.s, Trans->cseq_n.len );
	append_str( p, " ", 1 );
	append_str( p, method, method_len );
	append_str( p, MAXFWD_HEADER, MAXFWD_HEADER_LEN );
	append_str( p, CRLF, CRLF_LEN );

	if (!is_local(Trans))  {
		for ( hdr=Trans->uas.request->headers ; hdr ; hdr=hdr->next )
			if(hdr->type==HDR_ROUTE_T) {
				append_str(p, hdr->name.s, hdr->len );
			}
	}

	/* User Agent header */
	if (server_signature) {
		append_str(p, user_agent_hdr.s, user_agent_hdr.len );
		append_str(p, CRLF, CRLF_LEN );
	}
	/* Content Length */
	append_str(p, CONTENT_LENGTH "0" CRLF, CONTENT_LENGTH_LEN + 1 + CRLF_LEN);
#ifdef CANCEL_REASON_SUPPORT
	/* add reason if needed */
	if (reason_len) {
		if (likely(reason->cause > 0)) {
			append_str(p, REASON_PREFIX, REASON_PREFIX_LEN);
			code_len=ushort2sbuf(reason->cause, p,
									*len-(int)(p-cancel_buf));
			if (unlikely(code_len==0))
				BUG("not enough space to write reason code");
			p+=code_len;
			if (reason->u.text.s){
				append_str(p, REASON_TEXT, REASON_TEXT_LEN);
				*p='"'; p++;
				append_str(p, reason->u.text.s, reason->u.text.len);
				*p='"'; p++;
			}
			append_str(p, CRLF, CRLF_LEN);
		} else if (likely(reason->cause == CANCEL_REAS_PACKED_HDRS)) {
			append_str(p, reason->u.packed_hdrs.s, reason->u.packed_hdrs.len);
		} else if (reason->cause == CANCEL_REAS_RCVD_CANCEL) {
			for(hdr=reas1; hdr; hdr=next_sibling_hdr(hdr)) {
				/* hdr->len includes CRLF */
				append_str(p, hdr->name.s, hdr->len);
				if (likely(hdr==reas_last))
					break;
			}
		}
	}
#endif /* CANCEL_REASON_SUPPORT */
	append_str(p, CRLF, CRLF_LEN); /* msg. end */
	*p=0;

	pkg_free(via);
	return cancel_buf;
error01:
	pkg_free(via);
error:
	return NULL;
}

/* Re-parsing version of build_local() function:
 * it builds a local CANCEL or ACK (for non-200 response) request based on
 * the previous INVITE which was sent out.
 *
 * Can not be used to build other type of requests!
 */
char *build_local_reparse(struct cell *Trans,unsigned int branch,
	unsigned int *len, char *method, int method_len, str *to
#ifdef CANCEL_REASON_SUPPORT
	, struct cancel_reason *reason
#endif /* CANCEL_REASON_SUPPORT */
	)
{
	char	*invite_buf, *invite_buf_end;
	char	*cancel_buf;
	char	*s, *s1, *d;	/* source and destination buffers */
	short	invite_len;
	enum _hdr_types_t	hf_type;
	int	first_via, to_len;
	int cancel_buf_len;
#ifdef CANCEL_REASON_SUPPORT
	int reason_len, code_len;
	struct hdr_field *reas1, *reas_last, *hdr;
#endif /* CANCEL_REASON_SUPPORT */

	invite_buf = Trans->uac[branch].request.buffer;
	invite_len = Trans->uac[branch].request.buffer_len;

	if (!invite_buf || !invite_len) {
		LOG(L_ERR, "ERROR: build_local_reparse: INVITE is missing\n");
		goto error;
	}
	if ((*invite_buf != 'I') && (*invite_buf != 'i')) {
		LOG(L_ERR, "ERROR: trying to call build_local_reparse()"
					" for a non-INVITE request?\n");
		goto error;
	}
	
#ifdef CANCEL_REASON_SUPPORT
	reason_len = 0;
	reas1 = 0;
	reas_last = 0;
	/* compute reason size (if no reason or disabled => reason_len == 0)*/
	if (reason && reason->cause != CANCEL_REAS_UNKNOWN){
		if (likely(reason->cause > 0 &&
					cfg_get(tm, tm_cfg, local_cancel_reason))){
			/* Reason: SIP;cause=<reason->cause>[;text=<reason->u.text.s>] */
			reason_len = REASON_PREFIX_LEN + USHORT2SBUF_MAX_LEN +
				(reason->u.text.s?
					REASON_TEXT_LEN + 1 + reason->u.text.len + 1 : 0) +
				CRLF_LEN;
		} else if (likely(reason->cause == CANCEL_REAS_PACKED_HDRS &&
					!(Trans->flags & T_NO_E2E_CANCEL_REASON))) {
			reason_len = reason->u.packed_hdrs.len;
		} else if (reason->cause == CANCEL_REAS_RCVD_CANCEL &&
					reason->u.e2e_cancel &&
					!(Trans->flags & T_NO_E2E_CANCEL_REASON)) {
			/* parse the entire cancel, to get all the Reason headers */
			parse_headers(reason->u.e2e_cancel, HDR_EOH_F, 0);
			for(hdr=get_hdr(reason->u.e2e_cancel, HDR_REASON_T), reas1=hdr;
					hdr; hdr=next_sibling_hdr(hdr)) {
				/* hdr->len includes CRLF */
				reason_len += hdr->len;
				reas_last=hdr;
			}
		} else if (unlikely(reason->cause < CANCEL_REAS_MIN))
			BUG("unhandled reason cause %d\n", reason->cause);
	}
#endif /* CANCEL_REASON_SUPPORT */

	invite_buf_end = invite_buf + invite_len;
	s = invite_buf;

	/* Allocate memory for the new message.
	The new request will be smaller than the INVITE, so the same size is enough.
	I just extend it with the length of new To HF to be sure.
	Ugly, but we avoid lots of checks and memory allocations this way */
	to_len = to ? to->len : 0;
#ifdef CANCEL_REASON_SUPPORT
	cancel_buf_len = invite_len + to_len + reason_len;
#else
	cancel_buf_len = invite_len + to_len;
#endif /* CANCEL_REASON_SUPPORT */
	cancel_buf = shm_malloc(sizeof(char)*cancel_buf_len);
	if (!cancel_buf)
	{
		LOG(L_ERR, "ERROR: cannot allocate shared memory\n");
		goto error;
	}
	d = cancel_buf;

	/* method name + space */
	append_str(d, method, method_len);
	*d = ' ';
	d++;
	/* skip "INVITE " and copy the rest of the line including CRLF */
	s += 7;
	s1 = s;
	s = eat_line(s, invite_buf_end - s);
	append_str(d, s1, s - s1);

	/* check every header field name,
	we must exclude and modify some of the headers */
	first_via = 1;
	while (s < invite_buf_end) {
		s1 = s;
		if ((*s == '\n') || (*s == '\r')) {
			/* end of SIP msg */
			hf_type = HDR_EOH_T;
		} else {
			/* parse HF name */
			s = lw_get_hf_name(s, invite_buf_end,
						&hf_type);
		}

		switch(hf_type) {
			case HDR_CSEQ_T:
				/* find the method name and replace it */
				while ((s < invite_buf_end)
					&& ((*s == ':') || (*s == ' ') || (*s == '\t') ||
						((*s >= '0') && (*s <= '9')))
					) s++;
				append_str(d, s1, s - s1);
				append_str(d, method, method_len);
				append_str(d, CRLF, CRLF_LEN);
				s = lw_next_line(s, invite_buf_end);
				break;

			case HDR_VIA_T:
				s = lw_next_line(s, invite_buf_end);
				if (first_via) {
					/* copy hf */
					append_str(d, s1, s - s1);
					first_via = 0;
				} /* else skip this line, we need olny the first via */
				break;

			case HDR_TO_T:
				if (to_len == 0) {
					/* there is no To tag required, just copy paste
					   the header */
					s = lw_next_line(s, invite_buf_end);
					append_str(d, s1, s - s1);
				} else {
					/* use the given To HF instead of the original one */
					append_str(d, to->s, to->len);
					/* move the pointer to the next line */
					s = lw_next_line(s, invite_buf_end);
				}
				break;

			case HDR_FROM_T:
			case HDR_CALLID_T:
			case HDR_ROUTE_T:
			case HDR_MAXFORWARDS_T:
				/* copy hf */
				s = lw_next_line(s, invite_buf_end);
				append_str(d, s1, s - s1);
				break;

			case HDR_REQUIRE_T:
			case HDR_PROXYREQUIRE_T:
				/* skip this line */
				s = lw_next_line(s, invite_buf_end);
				break;

			case HDR_CONTENTLENGTH_T:
				/* copy hf name with 0 value */
				append_str(d, s1, s - s1);
				append_str(d, ": 0" CRLF, 3 + CRLF_LEN);
				/* move the pointer to the next line */
				s = lw_next_line(s, invite_buf_end);
				break;

			case HDR_EOH_T:
				/* end of SIP message found */
#ifdef CANCEL_REASON_SUPPORT
				/* add reason if needed */
				if (reason_len) {
					/* if reason_len !=0, no need for any reason enabled
					   checks */
					if (likely(reason->cause > 0)) {
						append_str(d, REASON_PREFIX, REASON_PREFIX_LEN);
						code_len=ushort2sbuf(reason->cause, d,
										cancel_buf_len-(int)(d-cancel_buf));
						if (unlikely(code_len==0))
							BUG("not enough space to write reason code");
						d+=code_len;
						if (reason->u.text.s){
							append_str(d, REASON_TEXT, REASON_TEXT_LEN);
							*d='"'; d++;
							append_str(d, reason->u.text.s,
											reason->u.text.len);
							*d='"'; d++;
						}
						append_str(d, CRLF, CRLF_LEN);
					} else if (likely(reason->cause ==
										CANCEL_REAS_PACKED_HDRS)) {
							append_str(d, reason->u.packed_hdrs.s,
											reason->u.packed_hdrs.len);
					} else if (reason->cause == CANCEL_REAS_RCVD_CANCEL) {
						for(hdr=reas1; hdr; hdr=next_sibling_hdr(hdr)) {
							/* hdr->len includes CRLF */
							append_str(d, hdr->name.s, hdr->len);
							if (likely(hdr==reas_last))
								break;
						}
					}
				}
#endif /* CANCEL_REASON_SUPPORT */
				/* final (end-of-headers) CRLF */
				append_str(d, CRLF, CRLF_LEN);
				*len = d - cancel_buf;
				/* LOG(L_DBG, "DBG: build_local: %.*s\n", *len, cancel_buf); */
				return cancel_buf;

			default:
				s = lw_next_line(s, invite_buf_end);

				if (cfg_get(tm, tm_cfg, ac_extra_hdrs).len
				&& (s1 + cfg_get(tm, tm_cfg, ac_extra_hdrs).len < invite_buf_end)
				&& (strncasecmp(s1,
						cfg_get(tm, tm_cfg, ac_extra_hdrs).s,
						cfg_get(tm, tm_cfg, ac_extra_hdrs).len) == 0)
				) {
					append_str(d, s1, s - s1);
				} /* else skip this line */
				break;
		}
	}

	/* HDR_EOH_T was not found in the buffer, the message is corrupt */
	LOG(L_ERR, "ERROR: build_local_reparse: HDR_EOH_T was not found\n");

	shm_free(cancel_buf);
error:
	LOG(L_ERR, "ERROR: build_local_reparse: cannot build %.*s request\n", method_len, method);
	return NULL;

}


typedef struct rte {
	rr_t* ptr;
	/* 'ptr' above doesn't point to a mem chunk linked to a sip_msg, so it
	 * won't be free'd along with it => it must be free'd "manually" */
	int free_rr;
	struct rte* next;
} rte_t;

  	 
static inline void free_rte_list(struct rte* list)
{
	struct rte* ptr;
	
	while(list) {
		ptr = list;
		list = list->next;
		if (ptr->free_rr)
			free_rr(&ptr->ptr);
		pkg_free(ptr);
	}
}


static inline int calc_routeset_len(struct rte* list, str* contact)
{
	struct rte* ptr;
	int ret;
	
	if (list || contact) {
		ret = ROUTE_PREFIX_LEN + CRLF_LEN;
	} else {
		return 0;
	}
	
	ptr = list;
	while(ptr) {
		if (ptr != list) {
			ret += ROUTE_SEPARATOR_LEN;
		}
		ret += ptr->ptr->len;
		ptr = ptr->next;
	}
	
	if (contact) {
		if (list) ret += ROUTE_SEPARATOR_LEN;
		ret += 2 + contact->len;
	}
	
	return ret;
}


     /*
      * Print the route set
      */
static inline char* print_rs(char* p, struct rte* list, str* contact)
{
	struct rte* ptr;
	
	if (list || contact) {
		memapp(p, ROUTE_PREFIX, ROUTE_PREFIX_LEN);
	} else {
		return p;
	}
	
	ptr = list;
	while(ptr) {
		if (ptr != list) {
			memapp(p, ROUTE_SEPARATOR, ROUTE_SEPARATOR_LEN);
		}
		
		memapp(p, ptr->ptr->nameaddr.name.s, ptr->ptr->len);
		ptr = ptr->next;
	}
	
	if (contact) {
		if (list) memapp(p, ROUTE_SEPARATOR, ROUTE_SEPARATOR_LEN);
		*p++ = '<';
		append_str(p, contact->s, contact->len);
		*p++ = '>';
	}
	
	memapp(p, CRLF, CRLF_LEN);
	return p;
}


     /*
      * Parse Contact header field body and extract URI
      * Does not parse headers !
      */
static inline int get_contact_uri(struct sip_msg* msg, str* uri)
{
	contact_t* c;
	
	uri->len = 0;
	if (!msg->contact) return 1;
	
	if (parse_contact(msg->contact) < 0) {
		LOG(L_ERR, "get_contact_uri: Error while parsing Contact body\n");
		return -1;
	}
	
	c = ((contact_body_t*)msg->contact->parsed)->contacts;
	
	if (!c) {
		LOG(L_ERR, "get_contact_uri: Empty body or * contact\n");
		return -2;
	}
	
	*uri = c->uri;
	return 0;
}

/**
 * Extract route set from the message (out of Record-Route, if reply, OR
 * Route, if request).
 * The route set is returned into the "UAC-format" (keep order for Rs, reverse
 * RRs).
 */
static inline int get_uac_rs(sip_msg_t *msg, int is_req, struct rte **rtset)
{
	struct hdr_field* ptr;
	rr_t *p, *new_p;
	struct rte *t, *head, *old_head;

	head = 0;
	for (ptr = is_req ? msg->route : msg->record_route; ptr; ptr = ptr->next) {
		switch (ptr->type) {
			case HDR_RECORDROUTE_T:
				if (is_req)
					continue;
				break;
			case HDR_ROUTE_T:
				if (! is_req)
					continue;
				break;
			default:
				continue;
		}
		if (parse_rr(ptr) < 0) {
			ERR("failed to parse Record-/Route HF (%d).\n", ptr->type);
			goto err;
		}
			
		p = (rr_t*)ptr->parsed;
		while(p) {
			if (! (t = pkg_malloc(sizeof(struct rte)))) {
				ERR("out of pkg mem (asked for: %d).\n",
						(int)sizeof(struct rte));
				goto err;
			}
			if (is_req) {
				/* in case of requests, the sip_msg structure is free'd before
				 * rte list is evaluated => must do a copy of it */
				if (duplicate_rr(&new_p, p) < 0) {
					pkg_free(t);
					ERR("failed to duplicate RR");
					goto err;
				}
				t->ptr = new_p;
			} else {
				t->ptr = p;
			}
			t->free_rr = is_req;
			t->next = head;
			head = t;
			p = p->next;
		}
	}

	if (is_req) {
		/* harvesting the R/RR HF above inserts at head, which suites RRs (as
		 * they must be reversed, anyway), but not Rs => reverse once more */
		old_head = head;
		head = 0;
		while (old_head) {
			t = old_head;
			old_head = old_head->next;
			t->next = head;
			head = t;
		}
	}

	*rtset = head;
	return 0;
err:
	free_rte_list(head);
	return -1;
}


static inline unsigned short uri2port(const struct sip_uri *puri)
{
	if (puri->port.s) {
		return puri->port_no;
	} else switch (puri->type) {
		case SIP_URI_T:
		case TEL_URI_T:
			if (puri->transport_val.len == sizeof("TLS") - 1) {
				unsigned trans;
				trans = puri->transport_val.s[0] | 0x20; trans <<= 8;
				trans |= puri->transport_val.s[1] | 0x20; trans <<= 8;
				trans |= puri->transport_val.s[2] | 0x20;
				if (trans == 0x746C73) /* t l s */
					return SIPS_PORT;
			}
			return SIP_PORT;
		case SIPS_URI_T:
		case TELS_URI_T:
			return SIPS_PORT;
		default:
			BUG("unexpected URI type %d.\n", puri->type);
	}
	return 0;
}

/**
 * Evaluate if next hop is a strict or loose router, by looking at the
 * retr. buffer of the original INVITE.
 * Assumes:
 * 	orig_inv is a parsed SIP message;
 * 	rtset is not NULL.
 * @return:
 * 	F_RB_NH_LOOSE : next hop was loose router;
 * 	F_RB_NH_STRICT: nh is strict;
 * 	0 on error.
 */
static unsigned long nhop_type(sip_msg_t *orig_inv, rte_t *rtset,
		const struct dest_info *dst_inv, str *contact)
{
	struct sip_uri puri, topr_uri, lastr_uri, inv_ruri, cont_uri;
	struct ip_addr *uri_ia;
	union sockaddr_union uri_sau;
	unsigned int uri_port, dst_port, inv_port, cont_port, lastr_port;
	rte_t *last_r;
#ifdef TM_LOC_ACK_DO_REV_DNS
	struct ip_addr ia;
	struct hostent *he;
	char **alias;
#endif

#define PARSE_URI(_str_, _uri_) \
	do { \
		/* parse_uri() 0z the puri */ \
		if (parse_uri((_str_)->s, \
				(_str_)->len, _uri_) < 0) { \
			ERR("failed to parse route body '%.*s'.\n", STR_FMT(_str_)); \
			return 0; \
		} \
	} while (0)

#define HAS_LR(_rte_) \
	({ \
		PARSE_URI(&(_rte_)->ptr->nameaddr.uri, &puri); \
		puri.lr.s; \
	})

#define URI_PORT(_puri_, _port) \
	do { \
		if (! (_port = uri2port(_puri_))) \
			return 0; \
	} while (0)

	/* examine the easy/fast & positive cases foremost */

	/* [1] check if 1st route lacks ;lr */
	DEBUG("checking lack of ';lr' in 1st route.\n");
	if (! HAS_LR(rtset))
		return F_RB_NH_STRICT;
	topr_uri = puri; /* save 1st route's URI */

	/* [2] check if last route shows ;lr */
	DEBUG("checking presence of ';lr' in last route.\n");
	for (last_r = rtset; last_r->next; last_r = last_r->next)
		/* scroll down to last route */
		;
	if (HAS_LR(last_r))
		return F_RB_NH_LOOSE;

	/* [3] 1st route has ;lr -> check if the destination of original INV
	 * equals the address provided by this route; if does -> loose */
	DEBUG("checking INVITE's destination against its first route.\n");
	URI_PORT(&topr_uri, uri_port);
	if (! (dst_port = su_getport(&dst_inv->to)))
		return 0; /* not really expected */
	if (dst_port != uri_port)
		return F_RB_NH_STRICT;
	/* if 1st route contains an IP address, comparing it against .dst */
	if ((uri_ia = str2ip(&topr_uri.host))
			|| (uri_ia = str2ip6(&topr_uri.host))
			) {
		/* we have an IP address in route -> comparison can go swiftly */
		if (init_su(&uri_sau, uri_ia, uri_port) < 0)
			return 0; /* not really expected */
		if (su_cmp(&uri_sau, &dst_inv->to))
			/* ;lr and sent there */
			return F_RB_NH_LOOSE;
		else
			/* ;lr and NOT sent there (probably sent to RURI address) */
			return F_RB_NH_STRICT;
	} else {
		/*if 1st route contains a name, rev resolve the .dst and compare*/
		INFO("Failed to decode string '%.*s' in route set element as IP "
				"address. Trying name resolution.\n",STR_FMT(&topr_uri.host));

	/* TODO: alternatively, rev name and compare against dest. IP.  */
#ifdef TM_LOC_ACK_DO_REV_DNS
		ia.af = 0;
		su2ip_addr(&ia, (void *)&dst_inv->to);
		if (! ia.af)
			return 0; /* not really expected */
		if ((he = rev_resolvehost(&ia))) {
			if ((strlen(he->h_name) == topr_uri.host.len) &&
					(memcmp(he->h_name, topr_uri.host.s, 
							topr_uri.host.len) == 0))
				return F_RB_NH_LOOSE;
			for (alias = he->h_aliases; *alias; alias ++)
				if ((strlen(*alias) == topr_uri.host.len) &&
						(memcmp(*alias, topr_uri.host.s, 
								topr_uri.host.len) == 0))
					return F_RB_NH_LOOSE;
			return F_RB_NH_STRICT;
		} else {
			INFO("failed to resolve address '%s' to a name.\n", 
					ip_addr2a(&ia));
		}
#endif
	}

	WARN("failed to establish with certainty the type of next hop; trying an"
			" educated guess.\n");

	/* [4] compare (possibly updated) remote target to original RURI; if
	 * equal, a strict router's address wasn't filled in as RURI -> loose */
	DEBUG("checking remote target against INVITE's RURI.\n");
	PARSE_URI(contact, &cont_uri);
	PARSE_URI(GET_RURI(orig_inv), &inv_ruri);
	URI_PORT(&cont_uri, cont_port);
	URI_PORT(&inv_ruri, inv_port);
	if ((cont_port == inv_port) && (cont_uri.host.len == inv_ruri.host.len) &&
			(memcmp(cont_uri.host.s, inv_ruri.host.s, cont_uri.host.len) == 0))
		return F_RB_NH_LOOSE;

	/* [5] compare (possibly updated) remote target to last route; if equal, 
	 * strict router's address might have been filled as RURI and remote
	 * target appended to route set -> strict */
	DEBUG("checking remote target against INVITE's last route.\n");
	PARSE_URI(&last_r->ptr->nameaddr.uri, &lastr_uri);
	URI_PORT(&lastr_uri, lastr_port);
	if ((cont_port == lastr_port) && 
			(cont_uri.host.len == lastr_uri.host.len) &&
			(memcmp(cont_uri.host.s, lastr_uri.host.s, 
					lastr_uri.host.len) == 0))
		return F_RB_NH_STRICT;

	WARN("failed to establish the type of next hop; assuming loose router.\n");
	return F_RB_NH_LOOSE;

#undef PARSE_URI
#undef HAS_LR
#undef URI_PORT
}

/**
 * Evaluates the routing elements in locally originated request or reply to
 * locally originated request.
 * If original INVITE was in-dialog (had to-tag), it uses the
 * routes present there (b/c the 2xx for it does not have a RR set, normally).
 * Otherwise, use the reply (b/c the INVITE does not have yet the complete 
 * route set).
 *
 * @return: negative for failure; out params:
 *  - list: route set;
 *  - ruri: RURI to be used in ACK;
 *  - nexthop: where to first send the ACK.
 *
 *  NOTE: assumes rpl's parsed to EOF!
 *
 */
static int eval_uac_routing(sip_msg_t *rpl, const struct retr_buf *inv_rb, 
		str* contact, struct rte **list, str *ruri, str *next_hop)
{
	sip_msg_t orig_inv, *sipmsg; /* reparse original INVITE */
	rte_t *t, *prev_t, *rtset = NULL;
	int is_req;
	struct sip_uri puri;
	static size_t chklen;
	int ret = -1;
	
	/* parse the retr. buffer */
	memset(&orig_inv, 0, sizeof(struct sip_msg));
	orig_inv.buf = inv_rb->buffer;
	orig_inv.len = inv_rb->buffer_len;
	DEBUG("reparsing retransmission buffer of original INVITE:\n%.*s\n",
			(int)orig_inv.len, orig_inv.buf);
	if (parse_msg(orig_inv.buf, orig_inv.len, &orig_inv) != 0) {
		ERR("failed to parse retr buffer (weird!): \n%.*s\n",
				(int)orig_inv.len, orig_inv.buf);
		return -1;
	}

	/* check if we need to look at request or reply */
	if ((parse_headers(&orig_inv, HDR_TO_F, 0) < 0) || (! orig_inv.to)) {
		/* the bug is at message assembly */
		BUG("failed to parse INVITE retr. buffer and/or extract 'To' HF:"
				"\n%.*s\n", (int)orig_inv.len, orig_inv.buf);
		goto end;
	}
	if (((struct to_body *)orig_inv.to->parsed)->tag_value.len) {
		DEBUG("building ACK for in-dialog INVITE (using RS in orig. INV.)\n");
		if (parse_headers(&orig_inv, HDR_EOH_F, 0) < 0) {
			BUG("failed to parse INVITE retr. buffer to EOH:"
					"\n%.*s\n", (int)orig_inv.len, orig_inv.buf);
			goto end;
		}
		sipmsg = &orig_inv;
		is_req = 1;
	} else {
		DEBUG("building ACK for out-of-dialog INVITE (using RS in RR set).\n");
		sipmsg = rpl;
		is_req = 0;
	}

	/* extract the route set */
	if (get_uac_rs(sipmsg, is_req, &rtset) < 0) {
		ERR("failed to extract route set.\n");
		goto end;
	}

	if (! rtset) { /* No routes */
		*ruri = *contact;
		*next_hop = *contact;
	} else if (! is_req) { /* out of dialog req. */
		if (parse_uri(rtset->ptr->nameaddr.uri.s, rtset->ptr->nameaddr.uri.len,
				&puri) < 0) {
			ERR("failed to parse first route in set.\n");
			goto end;
		}
		
		if (puri.lr.s) { /* Next hop is loose router */
			*ruri = *contact;
			*next_hop = rtset->ptr->nameaddr.uri;
		} else { /* Next hop is strict router */
			*ruri = rtset->ptr->nameaddr.uri;
			*next_hop = *ruri;
			/* consume first route, b/c it will be put in RURI */
			t = rtset;
			rtset = rtset->next;
			pkg_free(t);
		}
	} else {
		unsigned long route_flags = inv_rb->flags;
		DEBUG("UAC rb flags: 0x%x.\n", (unsigned int)route_flags);
eval_flags:
		switch (route_flags & (F_RB_NH_LOOSE|F_RB_NH_STRICT)) {
		case 0:
			WARN("calculate_hooks() not called when built the local UAC of "
					"in-dialog request, or called with empty route set.\n");
			/* try to figure out what kind of hop is the next one
			 * (strict/loose) by reading the original invite */
			if ((route_flags = nhop_type(&orig_inv, rtset, &inv_rb->dst, 
					contact))) {
				DEBUG("original request's next hop type evaluated to: 0x%x.\n",
						(unsigned int)route_flags);
				goto eval_flags;
			} else {
				ERR("failed to establish what kind of router the next "
						"hop is.\n");
				goto end;
			}
			break;
		case F_RB_NH_LOOSE:
			*ruri = *contact;
			*next_hop = rtset->ptr->nameaddr.uri;
			break;
		case F_RB_NH_STRICT:
			/* find ptr to last route body that contains the (possibly) old 
			 * remote target 
			 */
			for (t = rtset, prev_t = t; t->next; prev_t = t, t = t->next)
				;
			if ((t->ptr->len == contact->len) && 
					(memcmp(t->ptr->nameaddr.name.s, contact->s, 
							contact->len) == 0)){
				/* the remote target didn't update -> keep the whole route set,
				 * including the last entry */
				/* do nothing */
			} else {
				/* trash last entry and replace with new remote target */
				free_rte_list(t);
				/* compact the rr_t struct along with rte. this way, free'ing
				 * it can be done along with rte chunk, independent of Route
				 * header parser's allocator (using pkg/shm) */
				chklen = sizeof(struct rte) + sizeof(rr_t);
				if (! (t = pkg_malloc(chklen))) {
					ERR("out of pkg memory (%d required)\n", (int)chklen);
					goto end;
				}
				/* this way, .free_rr is also set to 0 (!!!) */
				memset(t, 0, chklen); 
				((rr_t *)&t[1])->nameaddr.name = *contact;
				((rr_t *)&t[1])->len = contact->len;
				/* chain the new route elem in set */
				if (prev_t == rtset)
				 	/*there is only one elem in route set: the remote target*/
					rtset = t;
				else
					prev_t->next = t;
			}

			*ruri = *GET_RURI(&orig_inv); /* reuse original RURI */
			*next_hop = *ruri;
			break;
		default:
			/* probably a mem corruption */
			BUG("next hop of original request marked as both loose and strict"
					" router (buffer: %.*s).\n", inv_rb->buffer_len, 
					inv_rb->buffer);
#ifdef EXTRA_DEBUG
			abort();
#else
			goto end;
#endif
		}
	}

	*list = rtset;
	/* all went well */
	ret = 0;
end:
	free_sip_msg(&orig_inv);
	if (ret < 0)
		free_rte_list(rtset);
	return ret;
}

     /*
      * The function creates an ACK to 200 OK. Route set will be created
      * and parsed and the dst parameter will contain the destination to which 
	  * the request should be send. The function is used by tm when it 
	  * generates local ACK to 200 OK (on behalf of applications using uac)
      */
char *build_dlg_ack(struct sip_msg* rpl, struct cell *Trans, 
					unsigned int branch, str *hdrs, str *body,
					unsigned int *len, struct dest_info* dst)
{
	char *req_buf, *p, *via;
	unsigned int via_len;
	char branch_buf[MAX_BRANCH_PARAM_LEN];
	int branch_len;
	str branch_str;
	struct hostport hp;
	struct rte* list;
	str contact, ruri, *cont;
	str next_hop;
	str body_len;
	str _to, *to = &_to;
#ifdef USE_DNS_FAILOVER
	struct dns_srv_handle dns_h;
#endif
#ifdef WITH_AS_SUPPORT
	/* With AS support, TM allows for external modules to generate building of
	 * the ACK; in this case, the ACK's retransmission buffer is built once
	 * and kept in memory (to help when retransmitted 2xx are received and ACK
	 * must be resent).
	 * Allocation of the string raw buffer that holds the ACK is piggy-backed
	 * with allocation of the retransmission buffer (since both have the same
	 * life-cycle): both the string buffer and retransm. buffer are placed 
	 * into the same allocated chunk of memory (retr. buffer first, string 
	 * buffer follows).In this case, the 'len' param is used as in-out 
	 * parameter: 'in' to give the extra space needed by the retr. buffer,
	 * 'out' to return the lenght of the allocated string buffer.
	 */
	unsigned offset = *len;
#endif
	
	if (parse_headers(rpl, HDR_EOH_F, 0) == -1 || !rpl->to) {
		ERR("Error while parsing headers.\n");
		return 0;
	} else {
		_to.s = rpl->to->name.s;
		_to.len = rpl->to->len;
	}
	
	if (get_contact_uri(rpl, &contact) < 0) {
		return 0;
	}
	
	if (eval_uac_routing(rpl, &Trans->uac[branch].request, &contact, 
			&list, &ruri, &next_hop) < 0) {
		ERR("failed to evaluate routing elements.\n");
		return 0;
	}
	DEBUG("ACK RURI: `%.*s', NH: `%.*s'.\n", STR_FMT(&ruri), 
			STR_FMT(&next_hop));

	if ((contact.s != ruri.s) || (contact.len != ruri.len)) {
		     /* contact != ruri means that the next
		      * hop is a strict router, cont will be non-zero
		      * and print_routeset will append it at the end
		      * of the route set
		      */
		cont = &contact;
	} else {
		     /* Next hop is a loose router, nothing to append */
		cont = 0;
	}
	
	     /* method, separators, version: "ACK sip:p2@iptel.org SIP/2.0" */
	*len = SIP_VERSION_LEN + ACK_LEN + 2 /* spaces */ + CRLF_LEN;
	*len += ruri.len;
	
	/* dst */
	switch(cfg_get(tm, tm_cfg, local_ack_mode)){
		case 1:
			/* send the local 200 ack to the same dst as the corresp. invite*/
			*dst=Trans->uac[branch].request.dst;
			break;
		case 2: 
			/* send the local 200 ack to the same dst as the 200 reply source*/
			init_dst_from_rcv(dst, &rpl->rcv);
			dst->send_flags=rpl->fwd_send_flags;
			break;
		case 0:
		default:
			/* rfc conformant behaviour: use the next_hop determined from the
			   contact and the route set */
#ifdef USE_DNS_FAILOVER
		if (cfg_get(core, core_cfg, use_dns_failover)){
			dns_srv_handle_init(&dns_h);
			if ((uri2dst(&dns_h , dst, rpl, &next_hop, PROTO_NONE)==0) ||
					(dst->send_sock==0)){
				dns_srv_handle_put(&dns_h);
				LOG(L_ERR, "build_dlg_ack: no socket found\n");
				goto error;
			}
			dns_srv_handle_put(&dns_h); /* not needed any more */
		}else{
			if ((uri2dst(0 , dst, rpl, &next_hop, PROTO_NONE)==0) ||
					(dst->send_sock==0)){
				LOG(L_ERR, "build_dlg_ack: no socket found\n");
				goto error;
			}
		}
#else /* USE_DNS_FAILOVER */
		if ( (uri2dst( dst, rpl, &next_hop, PROTO_NONE)==0) ||
				(dst->send_sock==0)){
				LOG(L_ERR, "build_dlg_ack: no socket found\n");
			goto error;
		}
#endif /* USE_DNS_FAILOVER */
		break;
	}
	
	 /* via */
	if (!t_calc_branch(Trans,  branch, branch_buf, &branch_len)) goto error;
	branch_str.s = branch_buf;
	branch_str.len = branch_len;
	set_hostport(&hp, 0);
	via = via_builder(&via_len, dst, &branch_str, 0, &hp);
	if (!via) {
		LOG(L_ERR, "build_dlg_ack: No via header got from builder\n");
		goto error;
	}
	*len+= via_len;
	
	     /*headers*/
	*len += Trans->from.len + Trans->callid.len + to->len + Trans->cseq_n.len + 1 + ACK_LEN + CRLF_LEN;
	
	     /* copy'n'paste Route headers */
	
	*len += calc_routeset_len(list, cont);
	
	     /* User Agent */
	if (server_signature) *len += user_agent_hdr.len + CRLF_LEN;
		/* extra headers */
	if (hdrs)
		*len += hdrs->len;
		/* body */
	if (body) {
		body_len.s = int2str(body->len, &body_len.len);
		*len += body->len;
	} else {
		body_len.len = 0;
		body_len.s = NULL; /*4gcc*/
		*len += 1; /* for the (Cont-Len:) `0' */
	}
	     /* Content Length, EoM */
	*len += CONTENT_LENGTH_LEN + body_len.len + CRLF_LEN + CRLF_LEN;

#if WITH_AS_SUPPORT
	req_buf = shm_malloc(offset + *len + 1);
	req_buf += offset;
#else
	req_buf = shm_malloc(*len + 1);
#endif
	if (!req_buf) {
		ERR("Cannot allocate memory (%u+1)\n", *len);
		goto error01;
	}
	p = req_buf;
	
	append_str( p, ACK, ACK_LEN );
	append_str( p, " ", 1 );
	append_str(p, ruri.s, ruri.len);
	append_str( p, " " SIP_VERSION CRLF, 1 + SIP_VERSION_LEN + CRLF_LEN);
  	 
	     /* insert our via */
	append_str(p, via, via_len);
	
	     /*other headers*/
	append_str(p, Trans->from.s, Trans->from.len);
	append_str(p, Trans->callid.s, Trans->callid.len);
	append_str(p, to->s, to->len);
	
	append_str(p, Trans->cseq_n.s, Trans->cseq_n.len);
	append_str( p, " ", 1 );
	append_str( p, ACK, ACK_LEN);
	append_str(p, CRLF, CRLF_LEN);
	
	     /* Routeset */
	p = print_rs(p, list, cont);
	
	     /* User Agent header */
	if (server_signature) {
		append_str(p, user_agent_hdr.s, user_agent_hdr.len);
		append_str(p, CRLF, CRLF_LEN);
	}
	
	/* extra headers */
	if (hdrs)
		append_str(p, hdrs->s, hdrs->len);
	
	     /* Content Length, EoH, (body) */
	if (body) {
		append_str(p, CONTENT_LENGTH, CONTENT_LENGTH_LEN);
		append_str(p, body_len.s, body_len.len);
		append_str(p, /*end crr. header*/CRLF /*EoH*/CRLF, CRLF_LEN + 
				CRLF_LEN);
		append_str(p, body->s, body->len);
	} else {
		append_str(p, CONTENT_LENGTH "0" CRLF CRLF, 
				CONTENT_LENGTH_LEN + 1 + CRLF_LEN + CRLF_LEN);
	}

	/* EoM */
	*p = 0;
	
	pkg_free(via);
	free_rte_list(list);
	return req_buf;
	
 error01:
	pkg_free(via);
 error:
	free_rte_list(list);
	return 0;
}


/*
 * Convert length of body into asciiz
 */
static inline int print_content_length(str* dest, str* body)
{
	static char content_length[10];
	int len;
	int b_len;
	char* tmp;

	     /* Print Content-Length */
	b_len=body?body->len:0;
	tmp = int2str(b_len, &len);
	if (len >= sizeof(content_length)) {
		LOG(L_ERR, "ERROR: print_content_length: content_len too big\n");
		dest->s = 0;
		dest->len = 0;
		return -1;
	}
	memcpy(content_length, tmp, len); 
	dest->s = content_length;
	dest->len = len;
	return 0;
}


/*
 * Convert CSeq number into asciiz
 */
static inline int print_cseq_num(str* _s, dlg_t* _d)
{
	static char cseq[INT2STR_MAX_LEN];
	char* tmp;
	int len;

	tmp = int2str(_d->loc_seq.value, &len);
	if (len > sizeof(cseq)) {
		LOG(L_ERR, "print_cseq_num: cseq too big\n");
		return -1;
	}
	
	memcpy(cseq, tmp, len);
	_s->s = cseq;
	_s->len = len;
	return 0;
}


/*
 * Create Via header
 */
static inline int assemble_via(str* dest, struct cell* t, 
								struct dest_info* dst, int branch)
{
	static char branch_buf[MAX_BRANCH_PARAM_LEN];
	char* via;
	int len;
	unsigned int via_len;
	str branch_str;
	struct hostport hp;

	if (!t_calc_branch(t, branch, branch_buf, &len)) {
		LOG(L_ERR, "ERROR: assemble_via: branch calculation failed\n");
		return -1;
	}
	
	branch_str.s = branch_buf;
	branch_str.len = len;

#ifdef XL_DEBUG
	printf("!!!proto: %d\n", sock->proto);
#endif

	set_hostport(&hp, 0);
	via = via_builder(&via_len, dst, &branch_str, 0, &hp);
	if (!via) {
		LOG(L_ERR, "assemble_via: via building failed\n");
		return -2;
	}
	
	dest->s = via;
	dest->len = via_len;
	return 0;
}


/*
 * Print Request-URI
 */
static inline char* print_request_uri(char* w, str* method, dlg_t* dialog, struct cell* t, int branch)
{
	memapp(w, method->s, method->len); 
	memapp(w, " ", 1); 

	t->uac[branch].uri.s = w; 
	t->uac[branch].uri.len = dialog->hooks.request_uri->len;

	memapp(w, dialog->hooks.request_uri->s, dialog->hooks.request_uri->len); 
	memapp(w, " " SIP_VERSION CRLF, 1 + SIP_VERSION_LEN + CRLF_LEN);

	return w;
}


/*
 * Print To header field
 */
static inline char* print_to(char* w, dlg_t* dialog, struct cell* t, int bracket)
{
	t->to.s = w;
	t->to.len = TO_LEN + dialog->rem_uri.len + CRLF_LEN
		+ (((dialog->rem_uri.s[dialog->rem_uri.len - 1]!='>'))?2:0);

	memapp(w, TO, TO_LEN);
	if(bracket) memapp(w, "<", 1);
	memapp(w, dialog->rem_uri.s, dialog->rem_uri.len);
	if(bracket) memapp(w, ">", 1);

	if (dialog->id.rem_tag.len) {
		t->to.len += TOTAG_LEN + dialog->id.rem_tag.len ;
		memapp(w, TOTAG, TOTAG_LEN);
		memapp(w, dialog->id.rem_tag.s, dialog->id.rem_tag.len);
	}

	memapp(w, CRLF, CRLF_LEN);
	return w;
}


/*
 * Print From header field
 */
static inline char* print_from(char* w, dlg_t* dialog, struct cell* t, int bracket)
{
	t->from.s = w;
	t->from.len = FROM_LEN + dialog->loc_uri.len + CRLF_LEN
		+ ((dialog->loc_uri.s[dialog->loc_uri.len - 1]!='>')?2:0);

	memapp(w, FROM, FROM_LEN);
	if(bracket) memapp(w, "<", 1);
	memapp(w, dialog->loc_uri.s, dialog->loc_uri.len);
	if(bracket) memapp(w, ">", 1);

	if (dialog->id.loc_tag.len) {
		t->from.len += FROMTAG_LEN + dialog->id.loc_tag.len;
		memapp(w, FROMTAG, FROMTAG_LEN);
		memapp(w, dialog->id.loc_tag.s, dialog->id.loc_tag.len);
	}

	memapp(w, CRLF, CRLF_LEN);
	return w;
}


/*
 * Print CSeq header field
 */
char* print_cseq_mini(char* target, str* cseq, str* method) {
	memapp(target, CSEQ, CSEQ_LEN);
	memapp(target, cseq->s, cseq->len);
	memapp(target, " ", 1);
	memapp(target, method->s, method->len);
	return target;
}

static inline char* print_cseq(char* w, str* cseq, str* method, struct cell* t)
{
	t->cseq_n.s = w; 
	/* don't include method name and CRLF -- subsequent
	 * local requests ACK/CANCEL will add their own */
	t->cseq_n.len = CSEQ_LEN + cseq->len; 
	w = print_cseq_mini(w, cseq, method);
	return w;
}

/*
 * Print Call-ID header field
 * created an extra function for pure header field creation, that is used by t_cancel for 
 * t_uac_cancel FIFO function.
 */
char* print_callid_mini(char* target, str callid) {
	memapp(target, CALLID, CALLID_LEN);
	memapp(target, callid.s, callid.len);
	memapp(target, CRLF, CRLF_LEN);
	return target;
}

static inline char* print_callid(char* w, dlg_t* dialog, struct cell* t)
{
	/* begins with CRLF, not included in t->callid, don`t know why...?!? */
	memapp(w, CRLF, CRLF_LEN);
	t->callid.s = w;
	t->callid.len = CALLID_LEN + dialog->id.call_id.len + CRLF_LEN;
	
	w = print_callid_mini(w, dialog->id.call_id);
	return w;
}

/*
* Find the first occurrence of find in s, where the search is limited to the
* first slen characters of s.
*/
static
char * _strnstr(const char* s, const char* find, size_t slen) {
	char c, sc;
	size_t len;

	if ((c = *find++) != '\0') {
		len = strlen(find);
		do {
			do {
				if (slen-- < 1 || (sc = *s++) == '\0')
					return (NULL);
			} while (sc != c);
			if (len > slen)
				return (NULL);
		} while (strncmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}

/*
 * Create a request
 */
char* build_uac_req(str* method, str* headers, str* body, dlg_t* dialog, int branch, 
			struct cell *t, int* len, struct dest_info* dst)
{
	char* buf, *w, *p;
	str content_length, cseq, via;
	unsigned int maxfwd_len;
	int tbracket, fbracket;

	if (!method || !dialog) {
		LOG(L_ERR, "build_uac_req(): Invalid parameter value\n");
		return 0;
	}
	if (print_content_length(&content_length, body) < 0) {
		LOG(L_ERR, "build_uac_req(): Error while printing content-length\n");
		return 0;
	}
	if (print_cseq_num(&cseq, dialog) < 0) {
		LOG(L_ERR, "build_uac_req(): Error while printing CSeq number\n");
		return 0;
	}

	if(headers==NULL || headers->len<15
			|| _strnstr(headers->s, "Max-Forwards:", headers->len)==NULL) {
		maxfwd_len = MAXFWD_HEADER_LEN;
	} else {
		maxfwd_len = 0;
	}

	*len = method->len + 1 + dialog->hooks.request_uri->len + 1 + SIP_VERSION_LEN + CRLF_LEN;

	if (assemble_via(&via, t, dst, branch) < 0) {
		LOG(L_ERR, "build_uac_req(): Error while assembling Via\n");
		return 0;
	}
	*len += via.len;

	if((p=q_memrchr(dialog->rem_uri.s, '>', dialog->rem_uri.len))!=NULL) {
		if((p==dialog->rem_uri.s + dialog->rem_uri.len - 1)
				|| *(p+1)==';') {
			tbracket = 0;
		} else {
			tbracket = 1;
		}
	} else {
		tbracket = 1;
	}
	if((p=q_memrchr(dialog->loc_uri.s, '>', dialog->loc_uri.len))!=NULL) {
		if((p==dialog->loc_uri.s + dialog->loc_uri.len - 1)
				|| *(p+1)==';') {
			fbracket = 0;
		} else {
			fbracket = 1;
		}
	} else {
		fbracket = 1;
	}

	*len += TO_LEN + dialog->rem_uri.len
		+ (dialog->id.rem_tag.len ? (TOTAG_LEN + dialog->id.rem_tag.len) : 0) + CRLF_LEN;    /* To */
	if(tbracket) *len += 2; /* To-URI < > */
	*len += FROM_LEN + dialog->loc_uri.len
		+ (dialog->id.loc_tag.len ? (FROMTAG_LEN + dialog->id.loc_tag.len) : 0) + CRLF_LEN;  /* From */
	if(fbracket) *len += 2; /* From-URI < > */
	*len += CALLID_LEN + dialog->id.call_id.len + CRLF_LEN;                                      /* Call-ID */
	*len += CSEQ_LEN + cseq.len + 1 + method->len + CRLF_LEN;                                    /* CSeq */
	*len += calculate_routeset_length(dialog);                                                   /* Route set */
	*len += maxfwd_len;                                                                          /* Max-forwards */	
	*len += CONTENT_LENGTH_LEN + content_length.len + CRLF_LEN; /* Content-Length */
	*len += ((server_signature && user_agent_hdr.len>0)
							? (user_agent_hdr.len + CRLF_LEN) : 0);	                         /* Signature */
	if(headers && headers->len>2) {
		/* Additional headers */
		*len += headers->len;
		/* End of header if missing */
		if(headers->s[headers->len - 1] != '\n')
			*len += CRLF_LEN;
	}
	*len += (body ? body->len : 0);                                                              /* Message body */
	*len += CRLF_LEN;                                                                            /* End of Header */

	buf = shm_malloc(*len + 1);
	if (!buf) {
		LOG(L_ERR, "build_uac_req(): no shmem (%d)\n", *len);
		goto error;
	}
	
	w = buf;

	w = print_request_uri(w, method, dialog, t, branch);  /* Request-URI */
	memapp(w, via.s, via.len);                            /* Top-most Via */
	w = print_to(w, dialog, t, tbracket);                 /* To */
	w = print_from(w, dialog, t, fbracket);               /* From */
	w = print_cseq(w, &cseq, method, t);                  /* CSeq */
	w = print_callid(w, dialog, t);                       /* Call-ID */
	w = print_routeset(w, dialog);                        /* Route set */

	if(maxfwd_len>0)
		memapp(w, MAXFWD_HEADER, MAXFWD_HEADER_LEN);      /* Max-forwards */

     /* Content-Length */
	memapp(w, CONTENT_LENGTH, CONTENT_LENGTH_LEN);
	memapp(w, content_length.s, content_length.len);
	memapp(w, CRLF, CRLF_LEN);
	
	     /* Server signature */
	if (server_signature && user_agent_hdr.len>0) {
		memapp(w, user_agent_hdr.s, user_agent_hdr.len);
		memapp(w, CRLF, CRLF_LEN);
	}
	if(headers && headers->len>2) {
		memapp(w, headers->s, headers->len);
		if(headers->s[headers->len - 1] != '\n')
			memapp(w, CRLF, CRLF_LEN);
	}
	memapp(w, CRLF, CRLF_LEN);
	if (body) memapp(w, body->s, body->len);

#ifdef EXTRA_DEBUG
	assert(w-buf == *len);
#endif

	pkg_free(via.s);
	return buf;

 error:
	pkg_free(via.s);
	return 0;
}


int t_calc_branch(struct cell *t, 
	int b, char *branch, int *branch_len)
{
	return branch_builder( t->hash_index,
			0, t->md5,
			b, branch, branch_len );
}

/**
 * build CANCEL from UAC side
 */
char *build_uac_cancel(str *headers,str *body,struct cell *cancelledT,
		unsigned int branch, unsigned int *len, struct dest_info* dst)
{
	char *cancel_buf, *p;
	char branch_buf[MAX_BRANCH_PARAM_LEN];
	str branch_str;
	struct hostport hp;
	str content_length, via;

	LM_DBG("sing FROM=<%.*s>, TO=<%.*s>, CSEQ_N=<%.*s>\n",
		cancelledT->from.len, cancelledT->from.s, cancelledT->to.len,
		cancelledT->to.s, cancelledT->cseq_n.len, cancelledT->cseq_n.s);

	branch_str.s=branch_buf;
	if (!t_calc_branch(cancelledT,  branch, branch_str.s, &branch_str.len )){
		LM_ERR("failed to create branch !\n");
		goto error;
	}
	set_hostport(&hp,0);

	if (assemble_via(&via, cancelledT, dst, branch) < 0) {
		LOG(L_ERR, "build_uac_req(): Error while assembling Via\n");
		return 0;
	}

	/* method, separators, version  */
	*len=CANCEL_LEN + 2 /* spaces */ +SIP_VERSION_LEN + CRLF_LEN;
	*len+=cancelledT->uac[branch].uri.len;
	/*via*/
	*len+= via.len;
	/*From*/
	*len+=cancelledT->from.len;
	/*To*/
	*len+=cancelledT->to.len;
	/*CallId*/
	*len+=cancelledT->callid.len;
	/*CSeq*/
	*len+=cancelledT->cseq_n.len+1+CANCEL_LEN+CRLF_LEN;
	/* User Agent */
	if (server_signature) {
		*len += USER_AGENT_LEN + CRLF_LEN;
	}
	/* Content Length  */
	if (print_content_length(&content_length, body) < 0) {
		LM_ERR("failed to print content-length\n");
		return 0;
	}
	/* Content-Length */
	*len += (body ? (CONTENT_LENGTH_LEN + content_length.len + CRLF_LEN) : 0);
	/*Additional headers*/
	*len += (headers ? headers->len : 0);
	/*EoM*/
	*len+= CRLF_LEN;
	/* Message body */
	*len += (body ? body->len : 0);

	cancel_buf=shm_malloc( *len+1 );
	if (!cancel_buf)
	{
		LM_ERR("no more share memory\n");
		goto error01;
	}
	p = cancel_buf;

	memapp( p, CANCEL, CANCEL_LEN );

	*(p++) = ' ';
	memapp( p, cancelledT->uac[branch].uri.s,
		cancelledT->uac[branch].uri.len);
	memapp( p, " " SIP_VERSION CRLF, 1+SIP_VERSION_LEN+CRLF_LEN );

	/* insert our via */
	memapp(p,via.s,via.len);

	/*other headers*/
	memapp( p, cancelledT->from.s, cancelledT->from.len );
	memapp( p, cancelledT->callid.s, cancelledT->callid.len );
	memapp( p, cancelledT->to.s, cancelledT->to.len );

	memapp( p, cancelledT->cseq_n.s, cancelledT->cseq_n.len );
	*(p++) = ' ';
	memapp( p, CANCEL, CANCEL_LEN );
	memapp( p, CRLF, CRLF_LEN );

	/* User Agent header */
	if (server_signature) {
		memapp(p,USER_AGENT CRLF, USER_AGENT_LEN+CRLF_LEN );
	}
	/* Content Length*/
	if (body) {
		memapp(p, CONTENT_LENGTH, CONTENT_LENGTH_LEN);
		memapp(p, content_length.s, content_length.len);
		memapp(p, CRLF, CRLF_LEN);
	}
	if(headers && headers->len){
		memapp(p,headers->s,headers->len);
	}
	/*EoM*/
	memapp(p,CRLF,CRLF_LEN);
	if(body && body->len){
		memapp(p,body->s,body->len);
	}
	*p=0;
	pkg_free(via.s);
	return cancel_buf;
error01:
	pkg_free(via.s);
error:
	return NULL;
}

