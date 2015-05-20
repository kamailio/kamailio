/*
 * Header file for TM MI functions
 *
 * Copyright (C) 2001-2003 FhG Fokus
 * Copyright (C) 2006 Voice Sistem SRL
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

/*! \file
 * \brief TMX :: MI functions
 *
 * \ingroup tm
 * - Module: \ref tm
 */

#include <stdlib.h>
#include "../../parser/parse_from.h"
#include "../../modules/tm/ut.h"
#include "../../lib/kmi/mi.h"
#include "../../str_list.h"
#include "tmx_mod.h"



/*!
 * \brief Convert a URI into socket address.
 *
 * Convert a URI into a socket address. Create a temporary proxy.
 * \param uri input URI
 * \param to_su target structure
 * \param proto protocol
 * \return choosen protocol
 */
static inline int uri2su(str *uri, union sockaddr_union *to_su, int proto)
{
	struct proxy_l *proxy;

	proxy = uri2proxy(uri, proto);
	if (!proxy) {
		ser_error = E_BAD_ADDRESS;
		LM_ERR("failed create a dst proxy\n");
		return -1;
	}

	hostent2su(to_su, &proxy->host, proxy->addr_idx, 
		(proxy->port) ? proxy->port : SIP_PORT);
	proto = proxy->proto;

	free_proxy(proxy);
	pkg_free(proxy);
	return proto;
}

/* should be replaced by tm's uri2dst instead */
static inline struct socket_info *uri2sock(struct sip_msg* msg, str *uri,
									union sockaddr_union *to_su, int proto)
{
	struct socket_info* send_sock;

	if ( (proto=uri2su(uri, to_su, proto))==-1 )
		return 0;

	send_sock = get_send_socket(msg, to_su, proto);
	if (!send_sock) {
		LM_ERR("no corresponding socket for af %d\n", to_su->s.sa_family);
		ser_error = E_NO_SOCKET;
	}

	return send_sock;
}


/************** Helper functions (from previous FIFO impl) *****************/

/*!
 * \brief Check if the request pushed via MI is correctly formed
 *
 * Check if the request pushed via MI is correctly formed. Test if
 * necessary SIP header fileds are included, could be parsed and the
 * CSEQ is correct.
 * \param msg SIP message
 * \param method SIP method
 * \param body SIP body
 * \param cseq SIP CSEQ value
 * \param callid SIP callid, optional
 * \return zero on success, or a mi_root with an error message included otherwise
 */
static inline struct mi_root* mi_check_msg(struct sip_msg* msg, str* method,
										str* body, int* cseq, str* callid)
{
	struct cseq_body *parsed_cseq;

	if (body && body->len && !msg->content_type)
		return init_mi_tree( 400, "Content-Type missing", 19);

	if (body && body->len && msg->content_length)
		return init_mi_tree( 400, "Content-Length disallowed", 24);

	if (!msg->to)
		return init_mi_tree( 400, "To missing", 10);

	if (!msg->from)
		return init_mi_tree( 400, "From missing", 12);

	/* we also need to know if there is from-tag and add it otherwise */
	if (parse_from_header(msg) < 0)
		return init_mi_tree( 400, "Error in From", 13);

	if (msg->cseq && (parsed_cseq = get_cseq(msg))) {
		if (str2int( &parsed_cseq->number, (unsigned int*)cseq)!=0)
			return init_mi_tree( 400, "Bad CSeq number", 15);

		if (parsed_cseq->method.len != method->len
		|| memcmp(parsed_cseq->method.s, method->s, method->len) !=0 )
			return init_mi_tree( 400, "CSeq method mismatch", 20);
	} else {
		*cseq = -1;
	}

	if (msg->callid) {
		callid->s = msg->callid->body.s;
		callid->len = msg->callid->body.len;
	} else {
		callid->s = 0;
		callid->len = 0;
	}

	return 0;
}


/*!
 * \brief Convert a header field block to char array
 *
 * Convert a header field block to char array, allocated in
 * pkg_mem.
 * \param uri SIP URI
 * \param hf header field
 * \param l
 * \param send_sock socket information
 * \return new allocated char array on success, zero otherwise
 */
static inline char *get_hfblock( str *uri, struct hdr_field *hf, int *l, struct socket_info** send_sock)
{
	struct str_list sl, *last, *new, *i, *foo;
	int hf_avail, frag_len, total_len;
	char *begin, *needle, *dst, *ret, *d;
	str *sock_name, *portname;
	union sockaddr_union to_su;

	ret=0; /* pessimist: assume failure */
	total_len=0;
	last=&sl;
	last->next=0;
	portname=sock_name=0;

	for (; hf; hf=hf->next) {
		if (tm_skip_hf(hf)) continue;

		begin=needle=hf->name.s; 
		hf_avail=hf->len;

		/* substitution loop */
		while(hf_avail) {
			d=memchr(needle, SUBST_CHAR, hf_avail);
			if (!d || d+1>=needle+hf_avail) { /* nothing to substitute */
				new=append_str_list(begin, hf_avail, &last, &total_len); 
				if (!new) goto error;
				break;
			} else {
				frag_len=d-begin;
				d++; /* d not at the second substitution char */
				switch(*d) {
					case SUBST_CHAR:	/* double SUBST_CHAR: IP */
						/* string before substitute */
						new=append_str_list(begin, frag_len, &last, &total_len); 
						if (!new) goto error;
						/* substitute */
						if (!sock_name) {
							if (*send_sock==0){
								*send_sock=uri2sock(0, uri, &to_su,PROTO_NONE);
								if (!*send_sock) {
									LM_ERR("send_sock failed\n");
									goto error;
								}
							}
							sock_name=&(*send_sock)->address_str;
							portname=&(*send_sock)->port_no_str;
						}
						new=append_str_list(sock_name->s, sock_name->len,
								&last, &total_len );
						if (!new) goto error;
						/* inefficient - FIXME --andrei*/
						new=append_str_list(":", 1, &last, &total_len);
						if (!new) goto error;
						new=append_str_list(portname->s, portname->len,
								&last, &total_len );
						if (!new) goto error;
						/* keep going ... */
						begin=needle=d+1;hf_avail-=frag_len+2;
						continue;
					default:
						/* no valid substitution char -- keep going */
						hf_avail-=frag_len+1;
						needle=d;
				}
			} /* possible substitute */
		} /* substitution loop */
		/* proceed to next header */
		/* new=append_str_list(CRLF, CRLF_LEN, &last, &total_len );
		if (!new) goto error; */
		LM_DBG("one more hf processed\n");
	} /* header loop */

	if(total_len==0) {
		LM_DBG("empty result\n");
		goto error;
	}

	/* construct a single header block now */
	ret=pkg_malloc(total_len);
	if (!ret) {
		LM_ERR("no pkg mem for hf block\n");
		goto error;
	}
	i=sl.next;
	dst=ret;
	while(i) {
		foo=i;
		i=i->next;
		memcpy(dst, foo->s.s, foo->s.len);
		dst+=foo->s.len;
		pkg_free(foo);
	}
	*l=total_len;
	return ret;

error:
	i=sl.next;
	while(i) {
		foo=i;
		i=i->next;
		pkg_free(foo);
	}
	*l=0;
	return 0;
}


/*!
 * \brief Print routes
 *
 * Print route to MI node, allocate temporary memory in pkg_mem.
 * \param node MI node
 * \param dlg route set
 */
static inline void mi_print_routes( struct mi_node *node, dlg_t* dlg)
{
#define MI_ROUTE_PREFIX_S       "Route: "
#define MI_ROUTE_PREFIX_LEN     (sizeof(MI_ROUTE_PREFIX_S)-1)
#define MI_ROUTE_SEPARATOR_S    ", "
#define MI_ROUTE_SEPARATOR_LEN  (sizeof(MI_ROUTE_SEPARATOR_S)-1)
	rr_t* ptr;
	int len;
	char *p, *s;

	ptr = dlg->hooks.first_route;

	if (ptr==NULL) {
		add_mi_node_child( node, 0, 0, 0, ".",1);
		return;
	}

	len = MI_ROUTE_PREFIX_LEN;
	for( ; ptr ; ptr=ptr->next)
		len += ptr->len + MI_ROUTE_SEPARATOR_LEN*(ptr->next!=NULL);
	if (dlg->hooks.last_route)
		len += dlg->hooks.last_route->len + 2;


	s = pkg_malloc( len );
	if (s==0) {
		LM_ERR("no more pkg mem\n");
		return;
	}


	p = s;
	memcpy( p, MI_ROUTE_PREFIX_S, MI_ROUTE_PREFIX_LEN);
	p += MI_ROUTE_PREFIX_LEN;

	for( ptr = dlg->hooks.first_route ; ptr ; ptr=ptr->next) {
		memcpy( p, ptr->nameaddr.name.s, ptr->len);
		p += ptr->len;
		if (ptr->next) {
			memcpy( p, MI_ROUTE_SEPARATOR_S, MI_ROUTE_SEPARATOR_LEN);
			p += MI_ROUTE_SEPARATOR_LEN;
		}
	}

	if (dlg->hooks.last_route) {
		*(p++) = '<';
		memcpy( p, dlg->hooks.last_route->s, dlg->hooks.last_route->len);
		p += dlg->hooks.last_route->len;
		*(p++) = '>';
	}

	add_mi_node_child( node, MI_DUP_VALUE, 0, 0, s, len);
	pkg_free(s);
}


/*!
 * \brief Print URIs
 *
 * Print URIs to MI node, allocate temporary memory in shm_mem.
 * \param node MI node
 * \param reply SIP reply
 * \return zero on success, -1 on errors
 */
static inline int mi_print_uris( struct mi_node *node, struct sip_msg* reply)
{
	dlg_t* dlg;

	if (reply==0)
		goto empty;

	dlg = (dlg_t*)shm_malloc(sizeof(dlg_t));
	if (!dlg) {
		LM_ERR("no shm memory left\n");
		return -1;
	}

	memset(dlg, 0, sizeof(dlg_t));
	if (_tmx_tmb.dlg_response_uac(dlg, reply, TARGET_REFRESH_UNKNOWN) < 0) {
		LM_ERR("failed to create dialog\n");
		_tmx_tmb.free_dlg(dlg);
		return -1;
	}

	if (dlg->state != DLG_CONFIRMED) {
		_tmx_tmb.free_dlg(dlg);
		goto empty;
	}

	if (dlg->hooks.request_uri->s) {
		add_mi_node_child( node, MI_DUP_VALUE, 0, 0,
			dlg->hooks.request_uri->s, dlg->hooks.request_uri->len);
	} else {
		add_mi_node_child( node, 0, 0, 0, ".",1);
	}
	if (dlg->hooks.next_hop->s) {
		add_mi_node_child( node, MI_DUP_VALUE, 0, 0,
			dlg->hooks.next_hop->s, dlg->hooks.next_hop->len);
	} else {
		add_mi_node_child( node, 0, 0, 0, ".",1);
	}

	mi_print_routes( node, dlg);

	_tmx_tmb.free_dlg(dlg);
	return 0;
empty:
	add_mi_node_child( node, 0, 0, 0, ".",1);
	add_mi_node_child( node, 0, 0, 0, ".",1);
	add_mi_node_child( node, 0, 0, 0, ".",1);
	return 0;
}


static void mi_uac_dlg_hdl( struct cell *t, int type, struct tmcb_params *ps )
{
	struct mi_handler *mi_hdl;
	struct mi_root *rpl_tree;
	str text;

	LM_DBG("MI UAC generated status %d\n", ps->code);
	if (!*ps->param)
		return;

	mi_hdl = (struct mi_handler *)(*ps->param);

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==0)
		goto done;

	if (ps->rpl==FAKED_REPLY) {
		get_reply_status( &text, ps->rpl, ps->code);
		if (text.s==0) {
			LM_ERR("get_reply_status failed\n");
			rpl_tree = 0;
			goto done;
		}
		add_mi_node_child( &rpl_tree->node, MI_DUP_VALUE, 0, 0,
			text.s, text.len);
		pkg_free(text.s);
		mi_print_uris( &rpl_tree->node, 0 );
		add_mi_node_child( &rpl_tree->node, 0, 0, 0, ".",1);
	} else { 
		addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "%d %.*s",
			ps->rpl->first_line.u.reply.statuscode,
			ps->rpl->first_line.u.reply.reason.len,
			ps->rpl->first_line.u.reply.reason.s);
		mi_print_uris( &rpl_tree->node, ps->rpl);
		add_mi_node_child( &rpl_tree->node, MI_DUP_VALUE, 0, 0,
			ps->rpl->headers->name.s,
			ps->rpl->len-(ps->rpl->headers->name.s - ps->rpl->buf));
	}

	LM_DBG("mi_callback successfully completed\n");
done:
	if (ps->code >= 200) {
		mi_hdl->handler_f( rpl_tree, mi_hdl, 1 /*done*/ );
		*ps->param = 0;
	} else {
		mi_hdl->handler_f( rpl_tree, mi_hdl, 0 );
	}
}



/**************************** MI functions ********************************/


/*
  Syntax of "t_uac_dlg" :
    method
    RURI
    NEXT_HOP
    socket
    headers
    [Body]
*/
struct mi_root*  mi_tm_uac_dlg(struct mi_root* cmd_tree, void* param)
{
	static char err_buf[MAX_REASON_LEN];
	static struct sip_msg tmp_msg;
	static dlg_t dlg;
	struct mi_root *rpl_tree;
	struct mi_node *node;
	struct sip_uri pruri;
	struct sip_uri pnexthop;
	struct socket_info* sock;
	str *method;
	str *ruri;
	str *nexthop;
	str *socket;
	str *hdrs;
	str *body;
	str s;
	str callid = {0,0};
	int sip_error;
	int proto;
	int port;
	int cseq;
	int n;
	uac_req_t uac_r;

	for( n=0,node = cmd_tree->node.kids; n<6 && node ; n++,node=node->next );
	if ( !(n==5 || n==6) || node!=0)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	cseq = -1;

	/* method name (param 1) */
	node = cmd_tree->node.kids;
	method = &node->value;

	/* RURI (param 2) */
	node = node->next;
	ruri = &node->value;
	if (parse_uri( ruri->s, ruri->len, &pruri) < 0 )
		return init_mi_tree( 400, "Invalid RURI", 12);

	/* nexthop RURI (param 3) */
	node = node->next;
	nexthop = &node->value;
	if (nexthop->len==1 && nexthop->s[0]=='.') {
		nexthop = 0;
	} else {
		if (parse_uri( nexthop->s, nexthop->len, &pnexthop) < 0 )
			return init_mi_tree( 400, "Invalid NEXTHOP", 15);
	}

	/* socket (param 4) */
	node = node->next;
	socket = &node->value;
	if (socket->len==1 && socket->s[0]=='.' ) {
		sock = 0;
	} else {
		if (parse_phostport( socket->s, &s.s, &s.len,
		&port,&proto)!=0)
			return init_mi_tree( 404, "Invalid local socket", 20);
		sock = grep_sock_info( &s, (unsigned short)port, proto);
		if (sock==0)
			return init_mi_tree( 404, "Local socket not found", 22);
	}

	/* new headers (param 5) */
	node = node->next;
	if (node->value.len==1 && node->value.s[0]=='.')
		hdrs = 0;
	else {
		hdrs = &node->value;
		/* use SIP parser to look at what is in the FIFO request */
		memset( &tmp_msg, 0, sizeof(struct sip_msg));
		tmp_msg.len = hdrs->len; 
		tmp_msg.buf = tmp_msg.unparsed = hdrs->s;
		if (parse_headers( &tmp_msg, HDR_EOH_F, 0) == -1 )
			return init_mi_tree( 400, "Bad headers", 11);
	}

	/* body (param 5 - optional) */
	node = node->next;
	if (node)
		body = &node->value;
	else
		body = 0;

	/* at this moment, we collected all the things we got, let's
	 * verify user has not forgotten something */
	rpl_tree = mi_check_msg( &tmp_msg, method, body, &cseq, &callid);
	if (rpl_tree) {
		if (tmp_msg.headers) free_hdr_field_lst(tmp_msg.headers);
		return rpl_tree;
	}

	s.s = get_hfblock( nexthop ? nexthop : ruri,
			tmp_msg.headers, &s.len, &sock);
	if (s.s==0) {
		if (tmp_msg.headers) free_hdr_field_lst(tmp_msg.headers);
		return 0;
	}

	memset( &dlg, 0, sizeof(dlg_t));
	/* Fill in Call-ID, use given Call-ID if
	 * present and generate it if not present */
	if (callid.s && callid.len)
		dlg.id.call_id = callid;
	else
		_tmx_tmb.generate_callid(&dlg.id.call_id);

	/* We will not fill in dlg->id.rem_tag because
	 * if present it will be printed within To HF */

	/* Generate fromtag if not present */
	if (!(get_from(&tmp_msg)->tag_value.len&&get_from(&tmp_msg)->tag_value.s))
		_tmx_tmb.generate_fromtag(&dlg.id.loc_tag, &dlg.id.call_id);

	/* Fill in CSeq */
	if (cseq!=-1)
		dlg.loc_seq.value = cseq;
	else
		dlg.loc_seq.value = DEFAULT_CSEQ;
	dlg.loc_seq.is_set = 1;

	dlg.loc_uri = tmp_msg.from->body;
	dlg.rem_uri = tmp_msg.to->body;
	dlg.rem_target = *ruri;
	if (nexthop)
		dlg.dst_uri = *nexthop;
	dlg.send_sock = sock;

	memset(&uac_r, 0, sizeof(uac_req_t));
	uac_r.method = method;
	uac_r.body = body;
	uac_r.headers = &s;
	uac_r.dialog = &dlg;
	if (cmd_tree->async_hdl!=NULL)
	{
		uac_r.cb = mi_uac_dlg_hdl;
		uac_r.cbp = (void*)cmd_tree->async_hdl;
		uac_r.cb_flags = TMCB_LOCAL_COMPLETED;
	}
	n = _tmx_tmb.t_uac(&uac_r);

	pkg_free(s.s);
	if (tmp_msg.headers) free_hdr_field_lst(tmp_msg.headers);

	if (n<=0) {
		/* error */
		rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
		if (rpl_tree==0)
			return 0;

		n = err2reason_phrase( n, &sip_error, err_buf, sizeof(err_buf),
			"MI/UAC") ;
		if (n > 0 )
			addf_mi_node_child( &rpl_tree->node, 0, 0, 0, "%d %.*s",
				sip_error, n, err_buf);
		else
			add_mi_node_child( &rpl_tree->node, 0, 0, 0,
				"500 MI/UAC failed", 17);

		return rpl_tree;
	} else {
		if (cmd_tree->async_hdl==NULL)
			return init_mi_tree( 202, "Accepted", 8);
		else
			return MI_ROOT_ASYNC_RPL;
	}
}


/*
  Syntax of "t_uac_cancel" :
    callid
    cseq
*/
struct mi_root* mi_tm_cancel(struct mi_root* cmd_tree, void* param)
{
	struct cancel_info cancel_data;
	struct mi_node *node;
	struct cell *trans;

	node =  cmd_tree->node.kids;
	if ( !node || !node->next || node->next->next)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	if( _tmx_tmb.t_lookup_callid( &trans, node->value, node->next->value) < 0 )
		return init_mi_tree( 481, "No such transaction", 19);

	/* cancel the call */
	LM_DBG("cancelling transaction %p\n",trans);

	init_cancel_info(&cancel_data);
	cancel_data.cancel_bitmap = ~0; /*all branches*/
	_tmx_tmb.cancel_uacs(trans, &cancel_data, 0);

	_tmx_tmb.unref_cell(trans);

	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
}


/*
  Syntax of "t_hash" :
    no nodes
*/
struct mi_root* mi_tm_hash(struct mi_root* cmd_tree, void* param)
{
#ifndef TM_HASH_STATS
	return init_mi_tree( 500, "No TM hash stats", 16);
#else
	struct mi_root* rpl_tree= NULL;
	struct mi_node* rpl;
	struct mi_node* node;
	struct mi_attr* attr;
	struct s_table* tm_t;
	char *p;
	int i;
	int len;

	rpl_tree = init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
	if (rpl_tree==0)
		return 0;
	rpl = &rpl_tree->node;
	tm_t = _tmx_tmb.get_table();

	for (i=0; i<TABLE_ENTRIES; i++) {
		if(tm_t->entries[i].cur_entries==0
				&& tm_t->entries[i].acc_entries==0)
			continue;

		p = int2str((unsigned long)i, &len );
		node = add_mi_node_child(rpl, MI_DUP_VALUE , 0, 0, p, len);
		if(node == NULL)
			goto error;

		p = int2str((unsigned long)tm_t->entries[i].cur_entries, &len );
		attr = add_mi_attr(node, MI_DUP_VALUE, "Current", 7, p, len );
		if(attr == NULL)
			goto error;

		p = int2str((unsigned long)tm_t->entries[i].acc_entries, &len );
		attr = add_mi_attr(node, MI_DUP_VALUE, "Total", 5, p, len );
		if(attr == NULL)
			goto error;
	}

	return rpl_tree;
error:
	free_mi_tree(rpl_tree);
	return init_mi_tree( 500, MI_INTERNAL_ERR_S, MI_INTERNAL_ERR_LEN);
#endif
}


/*
  Syntax of "t_reply" :
  code
  reason
  trans_id
  to_tag
  new headers
  [Body]
*/
struct mi_root* mi_tm_reply(struct mi_root* cmd_tree, void* param)
{
	struct mi_node* node;
	unsigned int hash_index;
	unsigned int hash_label;
	unsigned int rpl_code;
	struct cell *trans;
	str reason = {0, 0};
	str totag = {0, 0};
	str new_hdrs = {0, 0};
	str body = {0, 0};
	str tmp = {0, 0};
	char *p;
	int n;

	for( n=0,node = cmd_tree->node.kids; n<6 && node ; n++,node=node->next );
	if ( !(n==5 || n==6) || node!=0)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	/* get all info from the command */

	/* reply code (param 1) */
	node = cmd_tree->node.kids;
	if (str2int( &node->value, &rpl_code)!=0 || rpl_code>=700)
		return init_mi_tree( 400, "Invalid reply code", 18);

	/* reason text (param 2) */
	node = node->next;
	reason = node->value;

	/* trans_id (param 3) */
	node = node->next;
	tmp = node->value;
	p = memchr( tmp.s, ':', tmp.len);
	if(p==NULL)
		return init_mi_tree( 400, "Invalid trans_id", 16);

	tmp.len = p-tmp.s;
	if(str2int(&tmp, &hash_index)!=0)
		return init_mi_tree( 400, "Invalid index in trans_id", 25);

	tmp.s = p+1;
	tmp.len = (node->value.s+node->value.len) - tmp.s;
	if(str2int(&tmp, &hash_label)!=0)
		return init_mi_tree( 400, "Invalid label in trans_id", 25);

	if(_tmx_tmb.t_lookup_ident( &trans, hash_index, hash_label)<0)
		return init_mi_tree( 404, "Transaction not found", 21);

	/* to_tag (param 4) */
	node = node->next;
	totag = node->value;

	/* new headers (param 5) */
	node = node->next;
	if (!(node->value.len==1 && node->value.s[0]=='.'))
		new_hdrs = node->value;

	/* body (param 5 - optional) */
	node = node->next;
	if (node)
		body = node->value;

	/* it's refcounted now, t_reply_with body unrefs for me -- I can 
	 * continue but may not use T anymore  */
	n = _tmx_tmb.t_reply_with_body(trans, rpl_code, &reason, &body,
			&new_hdrs, &totag);

	if (n<0)
		return init_mi_tree( 500, "Reply failed", 12);

	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
}

/*
  Syntax of "t_reply_callid" :
  code
  reason
  callid
  cseq
  to_tag
  new headers
  [Body]
*/
struct mi_root* mi_tm_reply_callid(struct mi_root* cmd_tree, void* param)
{
	struct mi_node* node;
	unsigned int rpl_code;
	struct cell *trans;
	str reason = {0, 0};
	str totag = {0, 0};
	str new_hdrs = {0, 0};
	str body = {0, 0};
	str callid = {0, 0};
	str cseq = {0, 0};
	int n;

	for( n=0,node = cmd_tree->node.kids; n<7 && node ; n++,node=node->next );
	if ( !(n==6 || n==7) || node!=0)
		return init_mi_tree( 400, MI_MISSING_PARM_S, MI_MISSING_PARM_LEN);

	/* get all info from the command */

	/* reply code (param 1) */
	node = cmd_tree->node.kids;
	if (str2int( &node->value, &rpl_code)!=0 || rpl_code>=700)
		return init_mi_tree( 400, "Invalid reply code", 18);

	/* reason text (param 2) */
	node = node->next;
	reason = node->value;

	/* callid (param 3) */
	node = node->next;
	callid = node->value;

	/* cseq (param 4) */
	node = node->next;
	cseq = node->value;

	if(_tmx_tmb.t_lookup_callid( &trans, callid, cseq) < 0 )
		return init_mi_tree( 400, "Lookup failed - no transaction", 30);

	/* to_tag (param 5) */
	node = node->next;
	totag = node->value;

	/* new headers (param 6) */
	node = node->next;
	if (!(node->value.len==1 && node->value.s[0]=='.'))
		new_hdrs = node->value;

	/* body (param 7 - optional) */
	node = node->next;
	if (node)
		body = node->value;

	/* it's refcounted now, t_reply_with body unrefs for me -- I can
	 * continue but may not use T anymore  */
	n = _tmx_tmb.t_reply_with_body(trans, rpl_code, &reason, &body,
			&new_hdrs, &totag);

	if (n<0)
		return init_mi_tree( 500, "Reply failed", 12);

	return init_mi_tree( 200, MI_OK_S, MI_OK_LEN);
}

