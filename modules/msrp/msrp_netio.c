/**
 * Copyright (C) 2012 Daniel-Constantin Mierla (asipto.com)
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../../dprint.h"
#include "../../ut.h"
#include "../../cfg_core.h"
#include "../../tcp_server.h"
#include "../../forward.h"

#include "msrp_env.h"
#include "msrp_netio.h"

/**
 *
 */
int msrp_forward_frame(msrp_frame_t *mf, int flags)
{
#if 0
	if ((msrp_uri_to_dstinfo(0, &dst, uac_r->dialog->send_sock, snd_flags,
						uac_r->dialog->hooks.next_hop, PROTO_NONE)==0) ||
				(dst.send_sock==0)){
			LOG(L_ERR, "no send socket found\n");
			return -1;
		}
#endif
	return 0;
}

/**
 *
 */
int msrp_send_buffer(str *buf, str *addr, int flags)
{
	return 0;
}

/**
 *
 */
int msrp_relay(msrp_frame_t *mf)
{
	struct dest_info *dst;
	struct tcp_connection *con = NULL;
	char reqbuf[MSRP_MAX_FRAME_SIZE];
	msrp_hdr_t *tpath;
	msrp_hdr_t *fpath;
	msrp_env_t *env;
	str_array_t *sar;
	char *p;
	char *l;
	int port;

	if(mf->buf.len>=MSRP_MAX_FRAME_SIZE-1)
		return -1;

	tpath = msrp_get_hdr_by_id(mf, MSRP_HDR_TO_PATH);
	if(tpath==NULL)
	{
		LM_ERR("To-Path header not found\n");
		return -1;
	}
	fpath = msrp_get_hdr_by_id(mf, MSRP_HDR_FROM_PATH);
	if(fpath==NULL)
	{
		LM_ERR("From-Path header not found\n");
		return -1;
	}

	l = q_memchr(tpath->body.s, ' ', tpath->body.len);
	if(l==NULL)
	{
		LM_DBG("To-Path has only one URI -- nowehere to forward\n");
		return -1;
	}

	p = reqbuf;

	memcpy(p, mf->buf.s, tpath->body.s - mf->buf.s);
	p += tpath->body.s - mf->buf.s;

	memcpy(p, l + 1, fpath->body.s - l - 1);
	p += fpath->body.s - l - 1;

	memcpy(p, tpath->body.s, l + 1 - tpath->body.s);
	p += l + 1 - tpath->body.s;

	memcpy(p, fpath->name.s + 11, mf->buf.s + mf->buf.len - fpath->name.s - 11);
	p += mf->buf.s + mf->buf.len - fpath->name.s - 11;

	env = msrp_get_env();
	if(env->envflags&MSRP_ENV_DSTINFO)
	{
		dst = &env->dstinfo;
		goto done;
	}
	if(msrp_parse_hdr_to_path(mf)<0)
	{
		LM_ERR("error parsing To-Path header\n");
		return -1;
	}
	sar = (str_array_t*)tpath->parsed.data;
	if(sar==NULL || sar->size<2)
	{
		LM_DBG("To-Path has no next hop URI -- nowehere to forward\n");
		return -1;
	}
	if(msrp_env_set_dstinfo(mf, &sar->list[1], NULL, 0)<0)
	{
		LM_ERR("unable to set destination address\n");
		return -1;
	}
	dst = &env->dstinfo;
done:
	if (dst->send_flags.f & SND_F_FORCE_CON_REUSE)
	{
		port = su_getport(&dst->to);
		if (likely(port))
		{
			ticks_t con_lifetime;
			struct ip_addr ip;

			con_lifetime = cfg_get(tcp, tcp_cfg, con_lifetime);
			su2ip_addr(&ip, &dst->to);
			con = tcpconn_get(dst->id, &ip, port, NULL, con_lifetime);
		}
		else if (likely(dst->id))
		{
			con = tcpconn_get(dst->id, 0, 0, 0, 0);
		}

		if (con == NULL)
		{
			LM_WARN("TCP/TLS connection not found\n");
			return -1;
		}
	
		if (unlikely((con->rcv.proto == PROTO_WS || con->rcv.proto == PROTO_WSS)
				&& sr_event_enabled(SREV_TCP_WS_FRAME_OUT))) {
			ws_event_info_t wsev;

			memset(&wsev, 0, sizeof(ws_event_info_t));
			wsev.type = SREV_TCP_WS_FRAME_OUT;
			wsev.buf = reqbuf;
			wsev.len = p - reqbuf;
			wsev.id = con->id;
			return sr_event_exec(SREV_TCP_WS_FRAME_OUT, (void *) &wsev);
		}
		else if (tcp_send(dst, 0, reqbuf, p - reqbuf) < 0) {
			LM_ERR("forwarding frame failed\n");
			return -1;
		}
	}
	else if (tcp_send(dst, 0, reqbuf, p - reqbuf) < 0) {
			LM_ERR("forwarding frame failed\n");
			return -1;
	}

	return 0;
}

/**
 *
 */
int msrp_reply(msrp_frame_t *mf, str *code, str *text, str *xhdrs)
{
	char rplbuf[MSRP_MAX_FRAME_SIZE];
	msrp_hdr_t *hdr;
	msrp_env_t *env;
	char *p;
	char *l;

	/* no reply for a reply */
	if(mf->fline.msgtypeid==MSRP_REPLY)
		return 0;

	if(mf->fline.msgtypeid==MSRP_REQ_REPORT)
	{
		/* it does not take replies */
		return 0;
	}

	p = rplbuf;
	memcpy(p, mf->fline.protocol.s, mf->fline.protocol.len);
	p += mf->fline.protocol.len;
	*p = ' '; p++;
	memcpy(p, mf->fline.transaction.s, mf->fline.transaction.len);
	p += mf->fline.transaction.len;
	*p = ' '; p++;
	memcpy(p, code->s, code->len);
	p += code->len;
	*p = ' '; p++;
	memcpy(p, text->s, text->len);
	p += text->len;
	memcpy(p, "\r\n", 2);
	p += 2;
	memcpy(p, "To-Path: ", 9);
	p += 9;
	hdr = msrp_get_hdr_by_id(mf, MSRP_HDR_FROM_PATH);
	if(hdr==NULL)
	{
		LM_ERR("From-Path header not found\n");
		return -1;
	}
	if(mf->fline.msgtypeid==MSRP_REQ_SEND)
	{
		l = q_memchr(hdr->body.s, ' ', hdr->body.len);
		if(l==NULL) {
			memcpy(p, hdr->body.s, hdr->body.len + 2);
			p += hdr->body.len + 2;
		} else {
			memcpy(p, hdr->body.s, l - hdr->body.s);
			p += l - hdr->body.s;
			memcpy(p, "\r\n", 2);
			p += 2;
		}
	} else {
		memcpy(p, hdr->body.s, hdr->body.len + 2);
		p += hdr->body.len + 2;
	}
	hdr = msrp_get_hdr_by_id(mf, MSRP_HDR_TO_PATH);
	if(hdr==NULL)
	{
		LM_ERR("To-Path header not found\n");
		return -1;
	}
	memcpy(p, "From-Path: ", 11);
	p += 11;
	l = q_memchr(hdr->body.s, ' ', hdr->body.len);
	if(l==NULL) {
		memcpy(p, hdr->body.s, hdr->body.len + 2);
		p += hdr->body.len + 2;
	} else {
		memcpy(p, hdr->body.s, l - hdr->body.s);
		p += l - hdr->body.s;
		memcpy(p, "\r\n", 2);
		p += 2;
	}
	hdr = msrp_get_hdr_by_id(mf, MSRP_HDR_MESSAGE_ID);
	if(hdr!=NULL)
	{
		memcpy(p, hdr->buf.s, hdr->buf.len);
		p += hdr->buf.len;
	}

	if(xhdrs!=NULL && xhdrs->s!=NULL)
	{
		memcpy(p, xhdrs->s, xhdrs->len);
		p += xhdrs->len;
	}

	memcpy(p, mf->endline.s, mf->endline.len);
	p += mf->endline.len;
	*(p-3) = '$';

	env = msrp_get_env();

	if (unlikely((env->srcinfo.proto == PROTO_WS
			|| env->srcinfo.proto == PROTO_WSS)
			&& sr_event_enabled(SREV_TCP_WS_FRAME_OUT))) {
		struct tcp_connection *con = tcpconn_get(env->srcinfo.id, 0, 0,
								0, 0);
		ws_event_info_t wsev;

		if (con == NULL)
		{
			LM_WARN("TCP/TLS connection for WebSocket could not be"
				"found\n");
			return -1;
		}

		memset(&wsev, 0, sizeof(ws_event_info_t));
		wsev.type = SREV_TCP_WS_FRAME_OUT;
		wsev.buf = rplbuf;
		wsev.len = p - rplbuf;
		wsev.id = con->id;
		return sr_event_exec(SREV_TCP_WS_FRAME_OUT, (void *) &wsev);
	}
	else 
	if (tcp_send(&env->srcinfo, 0, rplbuf, p - rplbuf) < 0) {
		LM_ERR("sending reply failed\n");
		return -1;
	}

	return 0;
}


/**
 *
 */
struct dest_info *msrp_uri_to_dstinfo(struct dns_srv_handle* dns_h,
		struct dest_info* dst, struct socket_info *force_send_socket,
		snd_flags_t sflags, str *uri)
{
	msrp_uri_t parsed_uri;
	str* host;
	int port;
	int ip_found;
	union sockaddr_union to;
	int err;

	init_dest_info(dst);

	if (msrp_parse_uri(uri->s, uri->len, &parsed_uri) < 0) {
		LM_ERR("bad msrp uri: %.*s\n", uri->len, uri->s );
		return 0;
	}
	
	if (parsed_uri.scheme_no==MSRP_SCHEME_MSRPS){
		dst->proto = PROTO_TLS;
	} else {
		dst->proto = PROTO_TCP;
	}
	
	dst->send_flags=sflags;
	host=&parsed_uri.host;
	port = parsed_uri.port_no;

	if (dns_h && cfg_get(core, core_cfg, use_dns_failover)){
		ip_found=0;
		do{
			/* try all the ips until we find a good send socket */
			err=dns_sip_resolve2su(dns_h, &to, host,
								port, &dst->proto, dns_flags);
			if (err!=0){
				if (ip_found==0){
					if (err!=-E_DNS_EOR)
						LM_ERR("failed to resolve \"%.*s\" :"
								"%s (%d)\n", host->len, ZSW(host->s),
									dns_strerror(err), err);
					return 0; /* error, no ip found */
				}
				break;
			}
			if (ip_found==0){
				dst->to=to;
				ip_found=1;
			}
			dst->send_sock = get_send_socket2(force_send_socket, &to,
												dst->proto, 0);
			if (dst->send_sock){
				dst->to=to;
				return dst; /* found a good one */
			}
		} while(dns_srv_handle_next(dns_h, err));
		ERR("no corresponding socket for \"%.*s\" af %d\n", host->len, 
				ZSW(host->s), dst->to.s.sa_family);
		/* try to continue */
		return dst;
	}

	if (sip_hostport2su(&dst->to, host, port, &dst->proto)!=0){
		ERR("failed to resolve \"%.*s\"\n", host->len, ZSW(host->s));
		return 0;
	}
	dst->send_sock = get_send_socket2(force_send_socket, &dst->to,
			dst->proto, 0);
	if (dst->send_sock==0) {
		ERR("no corresponding socket for af %d\n", dst->to.s.sa_family);
		/* try to continue */
	}
	return dst;
}

struct socket_info *msrp_get_local_socket(str *sockaddr)
{
	int port, proto;
	str host;
	char backup;
	struct socket_info *si;

	backup = sockaddr->s[sockaddr->len];
	sockaddr->s[sockaddr->len] = '\0';
	if (parse_phostport(sockaddr->s, &host.s, &host.len, &port, &proto) < 0)
	{
		LM_ERR("invalid socket specification\n");
		sockaddr->s[sockaddr->len] = backup;
		return NULL;
	}
	sockaddr->s[sockaddr->len] = backup;
	si = grep_sock_info(&host, (unsigned short)port, (unsigned short)proto);
	return si;
}
