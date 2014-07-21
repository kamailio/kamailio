/**
 * $Id$
 *
 * Copyright (C) 2011 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "../../dset.h"
#include "../../forward.h"
#include "../../parser/parse_uri.h"
#include "../../resolve.h"

#include "corex_lib.h"

/**
 * append new branches with generic parameters
 */
int corex_append_branch(sip_msg_t *msg, gparam_t *pu, gparam_t *pq)
{
	str uri = {0};
	str qv = {0};
	int ret = 0;

	qvalue_t q = Q_UNSPECIFIED;
	flag_t branch_flags = 0;

	if (pu!=NULL)
	{
		if(fixup_get_svalue(msg, pu, &uri)!=0)
		{
			LM_ERR("cannot get the URI parameter\n");
			return -1;
		}
	}

	if (pq!=NULL)
	{
		if(fixup_get_svalue(msg, pq, &qv)!=0)
		{
			LM_ERR("cannot get the Q parameter\n");
			return -1;
		}
		if(qv.len>0 && str2q(&q, qv.s, qv.len)<0)
		{
			LM_ERR("cannot parse the Q parameter\n");
			return -1;
		}
	}


	getbflagsval(0, &branch_flags);
	ret = append_branch(msg, (uri.len>0)?&uri:0, &msg->dst_uri,
			    &msg->path_vec, q, branch_flags,
			    msg->force_send_socket, 0, 0, 0, 0);


	if(uri.len<=0)
	{
		/* reset all branch attributes if r-uri was shifted to branch */
		reset_force_socket(msg);
		setbflagsval(0, 0);
		if(msg->dst_uri.s!=0)
			pkg_free(msg->dst_uri.s);
		msg->dst_uri.s = 0;
		msg->dst_uri.len = 0;

		/* if this is a cloned message, don't free the path vector as it was copied into shm memory and will be freed as contiguous block*/
		if (!(msg->msg_flags&FL_SHM_CLONE)) {
			if(msg->path_vec.s!=0)
				pkg_free(msg->path_vec.s);
			msg->path_vec.s = 0;
			msg->path_vec.len = 0;
		}
	}

	return ret;
}

typedef struct corex_alias {
	str alias;
	unsigned short port;
	unsigned short proto;
	int flags;
	struct corex_alias* next;
} corex_alias_t;

static corex_alias_t *_corex_alias_list = NULL;

int corex_add_alias_subdomains(char* aliasval)
{
	char *p = NULL;
	corex_alias_t ta;
	corex_alias_t *na;

	memset(&ta, 0, sizeof(corex_alias_t));

	p = strchr(aliasval, ':');
	if(p==NULL) {
		/* only hostname */
		ta.alias.s = aliasval;
		ta.alias.len = strlen(aliasval);
		goto done;
	}
	if((p-aliasval)==3 || (p-aliasval)==4) {
		/* check if it is protocol */
		if((p-aliasval)==3 && strncasecmp(aliasval, "udp", 3)==0) {
			ta.proto = PROTO_UDP;
		} else if((p-aliasval)==3 && strncasecmp(aliasval, "tcp", 3)==0) {
			ta.proto = PROTO_TCP;
		} else if((p-aliasval)==3 && strncasecmp(aliasval, "tls", 3)==0) {
			ta.proto = PROTO_TLS;
		} else if((p-aliasval)==4 && strncasecmp(aliasval, "sctp", 4)==0) {
			ta.proto = PROTO_SCTP;
		} else {
			/* use hostname */
			ta.alias.s = aliasval;
			ta.alias.len = p - aliasval;
		}
	}
	if(ta.alias.len==0) {
		p++;
		if(p>=aliasval+strlen(aliasval))
			goto error;
		ta.alias.s = p;
		p = strchr(ta.alias.s, ':');
		if(p==NULL) {
			ta.alias.len = strlen(ta.alias.s);
			goto done;
		}
	}
	/* port */
	p++;
	if(p>=aliasval+strlen(aliasval))
		goto error;
	ta.port = str2s(p, strlen(p), NULL);

done:
	if(ta.alias.len==0)
		goto error;

	na = (corex_alias_t*)pkg_malloc(sizeof(corex_alias_t));
	if(na==NULL) {
		LM_ERR("no memory for adding alias subdomains: %s\n", aliasval);
		return -1;
	}
	memcpy(na, &ta, sizeof(corex_alias_t));
	na->next = _corex_alias_list;
	_corex_alias_list = na;

	return 0;

error:
	LM_ERR("error adding alias subdomains: %s\n", aliasval);
	return -1;
}


int corex_check_self(str* host, unsigned short port, unsigned short proto)
{
	corex_alias_t *ta;

	for(ta=_corex_alias_list; ta; ta=ta->next) {
		if(host->len<ta->alias.len)
			continue;
		if(ta->port!=0 && port!=0 && ta->port!=port)
			continue;
		if(ta->proto!=0 && proto!=0 && ta->proto!=proto)
			continue;
		if(host->len==ta->alias.len
				&& strncasecmp(host->s, ta->alias.s, host->len)==0) {
			/* match domain */
			LM_DBG("check self domain match: %d:%.*s:%d\n", (int)ta->port,
					ta->alias.len, ta->alias.s, (int)ta->proto);
			return 1;
		}
		if(strncasecmp(ta->alias.s, host->s + host->len - ta->alias.len,
					ta->alias.len)==0) {
			if(host->s[host->len - ta->alias.len - 1]=='.') {
				/* match sub-domain */
				LM_DBG("check self sub-domain match: %d:%.*s:%d\n", (int)ta->port,
					ta->alias.len, ta->alias.s, (int)ta->proto);
				return 1;
			}
		}
	}

	return 0; /* no match */
}

int corex_register_check_self(void)
{
	if(_corex_alias_list==NULL)
		return 0;
	if (register_check_self_func(corex_check_self) <0 ) {
	    LM_ERR("failed to register check self function\n");
	    return -1;
	}
	return 0;
}

int corex_send(sip_msg_t *msg, gparam_t *pu, enum sip_protos proto)
{
	str dest = {0};
	int ret = 0;
	struct sip_uri next_hop, *u;
	struct dest_info dst;
	char *p;

	if (pu)
	{
		if (fixup_get_svalue(msg, pu, &dest))
		{
			LM_ERR("cannot get the destination parameter\n");
			return -1;
		}
	}

	init_dest_info(&dst);

	if (dest.len <= 0)
	{
		/*get next hop uri uri*/
		if (msg->dst_uri.len) {
			ret = parse_uri(msg->dst_uri.s, msg->dst_uri.len,
							&next_hop);
			u = &next_hop;
		} else {
			ret = parse_sip_msg_uri(msg);
			u = &msg->parsed_uri;
		}

		if (ret<0) {
			LM_ERR("send() - bad_uri dropping packet\n");
			ret=E_BUG;
			goto error;
		}
	}
	else
	{
		u = &next_hop;
		u->port_no = 5060;
		u->host = dest;
		p = memchr(dest.s, ':', dest.len);
		if (p)
		{
			u->host.len = p - dest.s;
			p++;
			u->port_no = str2s(p, dest.len - (p - dest.s), NULL);
		}
	}

	ret = sip_hostport2su(&dst.to, &u->host, u->port_no,
				&dst.proto);
	if(ret!=0) {
		LM_ERR("failed to resolve [%.*s]\n", u->host.len,
			ZSW(u->host.s));
		ret=E_BUG;
		goto error;
	}

	dst.proto = proto;
	if (proto == PROTO_UDP)
	{
		dst.send_sock=get_send_socket(msg, &dst.to, PROTO_UDP);
		if (dst.send_sock!=0){
			ret=udp_send(&dst, msg->buf, msg->len);
		}else{
			ret=-1;
		}
	}
#ifdef USE_TCP
	else{
		/*tcp*/
		dst.id=0;
		ret=tcp_send(&dst, 0, msg->buf, msg->len);
	}
#endif

	if (ret>=0) ret=1;


error:
	return ret;
}

/**
 *
 */
int corex_send_data(str *puri, str *pdata)
{
	struct dest_info dst;
	sip_uri_t next_hop;
	int ret = 0;
	char proto;

	if(parse_uri(puri->s, puri->len, &next_hop)<0)
	{
		LM_ERR("bad dst sip uri <%.*s>\n", puri->len, puri->s);
		return -1;
	}

	init_dest_info(&dst);
	LM_DBG("sending data to sip uri <%.*s>\n", puri->len, puri->s);
	proto = next_hop.proto;
	if(sip_hostport2su(&dst.to, &next_hop.host, next_hop.port_no,
				&proto)!=0) {
		LM_ERR("failed to resolve [%.*s]\n", next_hop.host.len,
			ZSW(next_hop.host.s));
		return -1;
	}
	dst.proto = proto;
	if(dst.proto==PROTO_NONE) dst.proto = PROTO_UDP;

	if (dst.proto == PROTO_UDP)
	{
		dst.send_sock=get_send_socket(0, &dst.to, PROTO_UDP);
		if (dst.send_sock!=0) {
			ret=udp_send(&dst, pdata->s, pdata->len);
		} else {
			LM_ERR("no socket for dst sip uri <%.*s>\n", puri->len, puri->s);
			ret=-1;
		}
	}
#ifdef USE_TCP
	else if(dst.proto == PROTO_TCP) {
		/*tcp*/
		dst.id=0;
		ret=tcp_send(&dst, 0, pdata->s, pdata->len);
	}
#endif
#ifdef USE_TLS
	else if(dst.proto == PROTO_TLS) {
		/*tls*/
		dst.id=0;
		ret=tcp_send(&dst, 0, pdata->s, pdata->len);
	}
#endif
#ifdef USE_SCTP
	else if(dst.proto == PROTO_SCTP) {
		/*sctp*/
		dst.send_sock=get_send_socket(0, &dst.to, PROTO_SCTP);
		if (dst.send_sock!=0) {
			ret=sctp_core_msg_send(&dst, pdata->s, pdata->len);
		} else {
			LM_ERR("no socket for dst sip uri <%.*s>\n", puri->len, puri->s);
			ret=-1;
		}
	}
#endif
	else {
		LM_ERR("unknown proto [%d] for dst sip uri <%.*s>\n",
				dst.proto, puri->len, puri->s);
		ret=-1;
	}

	if (ret>=0) ret=1;

	return ret;
}
