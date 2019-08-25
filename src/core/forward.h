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
 */
/*!
 * \file
 * \brief Kamailio core :: Message forwarding
 * \author andrei
 * \ingroup core
 * Module: \ref core
 */

#ifndef forward_h
#define forward_h

#include "globals.h"
#include "parser/msg_parser.h"
#include "route.h"
#include "proxy.h"
#include "ip_addr.h"

#include "udp_server.h"
#ifdef USE_TCP
#include "tcp_server.h"
#include "tcp_conn.h"
#endif
#ifdef USE_SCTP
#include "sctp_core.h"
#endif

#include "compiler_opt.h"
#include "events.h"


enum ss_mismatch {
	SS_MISMATCH_OK=0,
	SS_MISMATCH_PROTO, /* proto mismatch, but found same addr:port */
	SS_MISMATCH_ADDR,  /* proto and addr:port mismatch */
	SS_MISMATCH_AF,    /* af mismatch */
	SS_MISMATCH_MCAST  /* mcast forced send socket */
};


struct socket_info* get_send_socket2(struct socket_info* force_send_socket,
		union sockaddr_union* su, int proto,
		enum ss_mismatch* mismatch);


inline static struct socket_info* get_send_socket(struct sip_msg* msg,
		union sockaddr_union* su, int proto)
{
	return get_send_socket2(msg?msg->force_send_socket:0, su, proto, 0);
}


#define GET_URI_PORT(uri) ((uri)->port_no?(uri)->port_no:(((uri)->proto==PROTO_TLS)?SIPS_PORT:SIP_PORT))

struct socket_info* get_out_socket(union sockaddr_union* to, int proto);
typedef int (*check_self_f)(str* host, unsigned short port,
		unsigned short proto);
int register_check_self_func(check_self_f f);
int check_self(str* host, unsigned short port, unsigned short proto);
int check_self_port(unsigned short port, unsigned short proto);
int forward_request( struct sip_msg* msg, str* dst,  unsigned short port,
		struct dest_info* send_info);
int update_sock_struct_from_via( union sockaddr_union* to,
		struct sip_msg* msg,
		struct via_body* via );

/* use src_ip, port=src_port if rport, via port if via port, 5060 otherwise */
#define update_sock_struct_from_ip(  to, msg ) \
	init_su((to), &(msg)->rcv.src_ip, \
			(((msg)->via1->rport)|| \
				(((msg)->msg_flags|global_req_flags)&FL_FORCE_RPORT))? \
				(msg)->rcv.src_port: \
				((msg)->via1->port)?(msg)->via1->port: SIP_PORT )

int forward_reply( struct sip_msg* msg);
int forward_reply_nocb( struct sip_msg* msg);

void forward_set_send_info(int v);

int is_check_self_func_list_set(void);

#define msg_send(_dst, _buf, _len) msg_send_buffer((_dst), (_buf), (_len), 0)

/* params:
 * dst = struct dest_info containing:
 *    send_sock = 0 if not known (e.g. for udp in some cases), non-0 otherwise;
 *                if 0 or mcast a new send_sock will be automatically choosen
 *    proto = TCP|UDP
 *    to = destination (sockaddr_union)
 *    id = only used on tcp, it will force sending on connection "id" if id!=0
 *         and the connection exists, else it will send to "to"
 *        (useful for sending replies on  the same connection as the request
 *         that generated them; use 0 if you don't want this)
 * buf, len = buffer
 * flags = control internal behavior
 *    * 1 - skip executing event SREV_NET_DATA_OUT
 * returns: 0 if ok, -1 on error*/

static inline int msg_send_buffer(struct dest_info* dst, char* buf, int len,
		int flags)
{
	struct dest_info new_dst;
	str outb;
	sr_net_info_t netinfo;
	sr_event_param_t evp = {0};

#ifdef USE_TCP
	int port;
	struct ip_addr ip;
	union sockaddr_union* from = NULL;
	union sockaddr_union local_addr;
	struct tcp_connection *con = NULL;
	struct ws_event_info wsev;
	int ret;
#endif

	outb.s = buf;
	outb.len = len;
	if(!(flags&1)) {
		if(sr_event_enabled(SREV_NET_DATA_OUT)) {
			outb.s = (char*)pkg_malloc(len + 1);
			if(outb.s==NULL) {
				LM_ERR("failed to clone outgoing buffer\n");
				return -1;
			}
			memcpy(outb.s, buf, len);
			outb.s[len] = '\0';
			evp.data = (void*)&outb;
			evp.dst = dst;
			sr_event_exec(SREV_NET_DATA_OUT, &evp);
		}
	}

	if(outb.s==NULL) {
		LM_ERR("failed to update outgoing buffer\n");
		return -1;
	}

#ifdef USE_TCP
	if (unlikely((dst->proto == PROTO_WS
#ifdef USE_TLS
					|| dst->proto == PROTO_WSS
#endif
				) && sr_event_enabled(SREV_TCP_WS_FRAME_OUT))) {
		if (unlikely(dst->send_flags.f & SND_F_FORCE_SOCKET
					&& dst->send_sock)) {

			local_addr = dst->send_sock->su;
#ifdef SO_REUSEPORT
			if (cfg_get(tcp, tcp_cfg, reuse_port)) {
				LM_DBG("sending to: %s, force_socket=%d, send_sock=%p\n",
						su2a(&dst->to,sizeof(struct sockaddr_in)),
						(dst->send_flags.f & SND_F_FORCE_SOCKET),
						dst->send_sock);

				su_setport(&local_addr, dst->send_sock->port_no);
			}
			else
				su_setport(&local_addr, 0); /* any local port will do */
#else
			su_setport(&local_addr, 0); /* any local port will do */
#endif
			from = &local_addr;
		}

		port = su_getport(&dst->to);
		if (likely(port)) {
			su2ip_addr(&ip, &dst->to);
			if(tcp_connection_match==TCPCONN_MATCH_STRICT) {
				con = tcpconn_lookup(dst->id, &ip, port, from,
						(dst->send_sock)?dst->send_sock->port_no:0, 0);
			} else {
				con = tcpconn_get(dst->id, &ip, port, from, 0);
			}
		}
		else if (likely(dst->id))
			con = tcpconn_get(dst->id, 0, 0, 0, 0);
		else {
			LM_CRIT("null_id & to\n");
			goto error;
		}

		if (con == NULL)
		{
			LM_WARN("TCP/TLS connection for WebSocket could not be found\n");
			goto error;
		}

		memset(&wsev, 0, sizeof(ws_event_info_t));
		wsev.type = SREV_TCP_WS_FRAME_OUT;
		wsev.buf = outb.s;
		wsev.len = outb.len;
		wsev.id = con->id;
		evp.data = (void *)&wsev;
		ret = sr_event_exec(SREV_TCP_WS_FRAME_OUT, &evp);
		tcpconn_put(con);
		goto done;
	}
#endif

	if (likely(dst->proto==PROTO_UDP)){
		if (unlikely((dst->send_sock==0) ||
					(dst->send_sock->flags & SI_IS_MCAST))){
			new_dst=*dst;
			new_dst.send_sock=get_send_socket(0, &dst->to, dst->proto);
			if (unlikely(new_dst.send_sock==0)){
				LM_ERR("no sending socket found\n");
				goto error;
			}
			dst=&new_dst;
		}
		if (unlikely(udp_send(dst, outb.s, outb.len)==-1)){
			LOG(cfg_get(core, core_cfg, corelog), "udp_send failed\n");
			goto error;
		}
	}
#ifdef USE_TCP
	else if (dst->proto==PROTO_TCP){
		if (unlikely(tcp_disable)){
			LM_WARN("attempt to send on tcp and tcp support is disabled\n");
			goto error;
		}else{
			if (unlikely((dst->send_flags.f & SND_F_FORCE_SOCKET) &&
						dst->send_sock)) {
				local_addr=dst->send_sock->su;
#ifdef SO_REUSEPORT
				if (cfg_get(tcp, tcp_cfg, reuse_port)) {
					LM_DBG("sending to: %s, force_socket=%d, send_sock=%p\n",
							su2a(&dst->to,sizeof(struct sockaddr_in)),
							(dst->send_flags.f & SND_F_FORCE_SOCKET),
							dst->send_sock);

					su_setport(&local_addr, dst->send_sock->port_no);
				}
				else
					su_setport(&local_addr, 0); /* any local port will do */
#else
				su_setport(&local_addr, 0); /* any local port will do */
#endif
				from=&local_addr;
			}
			if (unlikely(tcp_send(dst, from, outb.s, outb.len)<0)){
				LOG(cfg_get(core, core_cfg, corelog), "tcp_send failed\n");
				goto error;
			}
		}
	}
#ifdef USE_TLS
	else if (dst->proto==PROTO_TLS){
		if (unlikely(tls_disable)){
			LM_WARN("attempt to send on tls and tls support is disabled\n");
			goto error;
		}else{
			if (unlikely((dst->send_flags.f & SND_F_FORCE_SOCKET) &&
						dst->send_sock)) {
				local_addr=dst->send_sock->su;
#ifdef SO_REUSEPORT
				if (cfg_get(tcp, tcp_cfg, reuse_port)) {
					LM_DBG("sending to: %s, force_socket=%d, send_sock=%p\n",
							su2a(&dst->to,sizeof(struct sockaddr_in)),
							(dst->send_flags.f & SND_F_FORCE_SOCKET),
							dst->send_sock);

					su_setport(&local_addr, dst->send_sock->port_no);
				}
				else
					su_setport(&local_addr, 0); /* any local port will do */
#else
				su_setport(&local_addr, 0); /* any local port will do */
#endif
				from=&local_addr;
			}
			if (unlikely(tcp_send(dst, from, outb.s, outb.len)<0)){
				LOG(cfg_get(core, core_cfg, corelog), "tcp_send failed\n");
				goto error;
			}
		}
	}
#endif /* USE_TLS */
#endif /* USE_TCP */
#ifdef USE_SCTP
	else if (dst->proto==PROTO_SCTP){
		if (unlikely(sctp_disable)){
			LM_WARN("attempt to send on sctp and sctp support is disabled\n");
			goto error;
		}else{
			if (unlikely(dst->send_sock==0)){
				new_dst=*dst;
				new_dst.send_sock=get_send_socket(0, &dst->to, dst->proto);
				if (unlikely(new_dst.send_sock==0)){
					LM_ERR("no sending SCTP socket found\n");
					goto error;
				}
				dst=&new_dst;
			}
			if (unlikely(sctp_core_msg_send(dst, outb.s, outb.len)<0)){
				LOG(cfg_get(core, core_cfg, corelog), "sctp_msg_send failed\n");
				goto error;
			}
		}
	}
#endif /* USE_SCTP */
	else{
		LM_CRIT("unknown proto %d\n", dst->proto);
		goto error;
	}
	ret = 0;
done:

	if(!(flags&1)) {
		memset(&netinfo, 0, sizeof(sr_net_info_t));
		netinfo.data.s = outb.s;
		netinfo.data.len = outb.len;
		netinfo.dst = dst;
		evp.data = (void*)&netinfo;
		sr_event_exec(SREV_NET_DATA_SEND, &evp);
	}

	if(outb.s != buf)
		pkg_free(outb.s);
	return ret;
error:
	if(outb.s != buf)
		pkg_free(outb.s);
	return -1;
}

#endif
