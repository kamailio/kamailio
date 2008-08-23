/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
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
 * History:
 * -------
 *  2001-??-??  created by andrei
 *  ????-??-??  lots of changes by a lot of people
 *  2003-01-23  support for determination of outbound interface added :
 *               get_out_socket (jiri)
 *  2003-01-24  reply to rport support added, contributed by
 *               Maxim Sobolev <sobomax@FreeBSD.org> and modified by andrei
 *  2003-02-11  removed calls to upd_send & tcp_send & replaced them with
 *               calls to msg_send (andrei)
 *  2003-03-19  replaced all mallocs/frees w/ pkg_malloc/pkg_free (andrei)
 *  2003-04-02  fixed get_send_socket for tcp fwd to udp (andrei)
 *  2003-04-03  added su_setport (andrei)
 *  2003-04-04  update_sock_struct_from_via now differentiates between
 *               local replies  & "normal" replies (andrei)
 *  2003-04-12  update_sock_struct_from via uses also FL_FORCE_RPORT for
 *               local replies (andrei)
 *  2003-08-21  check_self properly handles ipv6 addresses & refs   (andrei)
 *  2003-10-21  check_self updated to handle proto (andrei)
 *  2003-10-24  converted to the new socket_info lists (andrei)
 *  2004-10-10  modified check_self to use grep_sock_info (andrei)
 *  2004-11-08  added force_send_socket support in get_send_socket (andrei)
 *  2005-12-11  onsend_router support; forward_request to no longer
 *              pkg_malloc'ed (andrei)
 *  2006-04-12  forward_{request,reply} use now struct dest_info (andrei)
 *  2006-04-21  basic comp via param support (andrei)
 *  2006-07-31  forward_request can resolve destination on its own, uses the 
 *              dns cache and falls back on send error to other ips (andrei)
 *  2007-10-08  get_send_socket() will ignore force_send_socket if the forced
 *               socket is multicast (andrei)
 */



#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "forward.h"
#include "hash_func.h"
#include "config.h"
#include "parser/msg_parser.h"
#include "route.h"
#include "dprint.h"
#include "globals.h"
#include "cfg_core.h"
#include "data_lump.h"
#include "ut.h"
#include "mem/mem.h"
#include "msg_translator.h"
#include "sr_module.h"
#include "ip_addr.h"
#include "resolve.h"
#include "name_alias.h"
#include "socket_info.h"
#include "onsend.h"
#include "resolve.h"
#ifdef USE_DNS_FAILOVER
#include "dns_cache.h"
#endif
#ifdef USE_DST_BLACKLIST
#include "dst_blacklist.h"
#endif
#include "compiler_opt.h"

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif



/* return a socket_info_pointer to the sending socket; as opposed to
 * get_send_socket, which returns process's default socket, get_out_socket
 * attempts to determine the outbound interface which will be used;
 * it creates a temporary connected socket to determine it; it will
 * be very likely noticeably slower, but it can deal better with
 * multihomed hosts
 */
struct socket_info* get_out_socket(union sockaddr_union* to, int proto)
{
	int temp_sock;
	socklen_t len;
	union sockaddr_union from; 
	struct socket_info* si;
	struct ip_addr ip;

	if (proto!=PROTO_UDP) {
		LOG(L_CRIT, "BUG: get_out_socket can only be called for UDP\n");
		return 0;
	}
	
	temp_sock=socket(to->s.sa_family, SOCK_DGRAM, 0 );
	if (temp_sock==-1) {
		LOG(L_ERR, "ERROR: get_out_socket: socket() failed: %s\n",
				strerror(errno));
		return 0;
	}
	if (connect(temp_sock, &to->s, sockaddru_len(*to))==-1) {
		LOG(L_ERR, "ERROR: get_out_socket: connect failed: %s\n",
				strerror(errno));
		goto error;
	}
	len=sizeof(from);
	if (getsockname(temp_sock, &from.s, &len)==-1) {
		LOG(L_ERR, "ERROR: get_out_socket: getsockname failed: %s\n",
				strerror(errno));
		goto error;
	}
	su2ip_addr(&ip, &from);
	si=find_si(&ip, 0, proto);
	if (si==0) goto error;
	close(temp_sock);
	DBG("DEBUG: get_out_socket: socket determined: %p\n", si );
	return si;
error:
	LOG(L_ERR, "ERROR: get_out_socket: no socket found\n");
	close(temp_sock);
	return 0;
}



/* returns a socket_info pointer to the sending socket or 0 on error
 * params: sip msg (can be null), destination socket_union pointer, protocol
 * if msg!=null and msg->force_send_socket, the force_send_socket will be
 * used
 */
struct socket_info* get_send_socket(struct sip_msg *msg, 
										union sockaddr_union* to, int proto)
{
	struct socket_info* send_sock;
	
	/* check if send interface is not forced */
	if (unlikely(msg && msg->force_send_socket)){
		if (unlikely(msg->force_send_socket->proto!=proto)){
			DBG("get_send_socket: force_send_socket of different proto"
					" (%d)!\n", proto);
			msg->force_send_socket=find_si(&(msg->force_send_socket->address),
											msg->force_send_socket->port_no,
											proto);
			if (unlikely(msg->force_send_socket == 0)){
				LOG(L_WARN, "WARNING: get_send_socket: "
						"protocol/port mismatch\n");
				goto not_forced;
			}
		}
		if (likely((msg->force_send_socket->socket!=-1) &&
					!(msg->force_send_socket->flags & SI_IS_MCAST)))
				return msg->force_send_socket;
		else{
			if (!(msg->force_send_socket->flags & SI_IS_MCAST))
				LOG(L_WARN, "WARNING: get_send_socket: not listening"
							 " on the requested socket, no fork mode?\n");
		}
	};
not_forced:
	if (mhomed && proto==PROTO_UDP){
		send_sock=get_out_socket(to, proto);
		if ((send_sock==0) || (send_sock->socket!=-1))
			return send_sock; /* found or error*/
		else if (send_sock->socket==-1){
			LOG(L_WARN, "WARNING: get_send_socket: not listening on the"
					" requested socket, no fork mode?\n");
			/* continue: try to use some socket */
		}
	}

	send_sock=0;
	/* check if we need to change the socket (different address families -
	 * eg: ipv4 -> ipv6 or ipv6 -> ipv4) */
	switch(proto){
#ifdef USE_TCP
		case PROTO_TCP:
		/* on tcp just use the "main address", we don't really now the
		 * sending address (we can find it out, but we'll need also to see
		 * if we listen on it, and if yes on which port -> too complicated*/
			switch(to->s.sa_family){
				/* FIXME */
				case AF_INET:	send_sock=sendipv4_tcp;
								break;
#ifdef USE_IPV6
				case AF_INET6:	send_sock=sendipv6_tcp;
								break;
#endif
				default:	LOG(L_ERR, "get_send_socket: BUG: don't know how"
									" to forward to af %d\n", to->s.sa_family);
			}
			break;
#endif
#ifdef USE_TLS
		case PROTO_TLS:
			switch(to->s.sa_family){
				/* FIXME */
				case AF_INET:	send_sock=sendipv4_tls;
								break;
#ifdef USE_IPV6
				case AF_INET6:	send_sock=sendipv6_tls;
								break;
#endif
				default:	LOG(L_ERR, "get_send_socket: BUG: don't know how"
									" to forward to af %d\n", to->s.sa_family);
			}
			break;
#endif /* USE_TLS */
#ifdef USE_SCTP
		case PROTO_SCTP:
			if ((bind_address==0) ||
					(to->s.sa_family!=bind_address->address.af) ||
					(bind_address->proto!=PROTO_SCTP)){
				switch(to->s.sa_family){
					case AF_INET:	send_sock=sendipv4_sctp;
									break;
#ifdef USE_IPV6
					case AF_INET6:	send_sock=sendipv6_sctp;
									break;
#endif
					default:	LOG(L_ERR, "get_send_socket: BUG: don't know"
										" how to forward to af %d\n",
										to->s.sa_family);
				}
			}else send_sock=bind_address;
			break;
#endif /* USE_SCTP */
		case PROTO_UDP:
			if ((bind_address==0) ||
					(to->s.sa_family!=bind_address->address.af) ||
					(bind_address->proto!=PROTO_UDP)){
				switch(to->s.sa_family){
					case AF_INET:	send_sock=sendipv4;
									break;
#ifdef USE_IPV6
					case AF_INET6:	send_sock=sendipv6;
									break;
#endif
					default:	LOG(L_ERR, "get_send_socket: BUG: don't know"
										" how to forward to af %d\n",
										to->s.sa_family);
				}
			}else send_sock=bind_address;
			break;
		default:
			LOG(L_CRIT, "BUG: get_send_socket: unknown proto %d\n", proto);
	}
	return send_sock;
}



/* checks if the proto: host:port is one of the address we listen on;
 * if port==0, the  port number is ignored
 * if proto==0 (PROTO_NONE) the protocol is ignored
 * returns 1 if true, 0 if false, -1 on error
 * WARNING: uses str2ip6 so it will overwrite any previous
 *  unsaved result of this function (static buffer)
 */
int check_self(str* host, unsigned short port, unsigned short proto)
{
	if (grep_sock_info(host, port, proto)) goto found;
	/* try to look into the aliases*/
	if (grep_aliases(host->s, host->len, port, proto)==0){
		DBG("check_self: host != me\n");
		return 0;
	}
found:
	return 1;
}

/* checks if the proto:port is one of the ports we listen on;
 * if proto==0 (PROTO_NONE) the protocol is ignored
 * returns 1 if true, 0 if false, -1 on error
 */
int check_self_port(unsigned short port, unsigned short proto)
{
	if (grep_sock_info_by_port(port, proto))
		/* as aliases do not contain different ports we can skip them */
		return 1;
	else
		return 0;
}



/* forwards a request to dst
 * parameters:
 *   msg       - sip msg
 *   dst       - destination name, if non-null it will be resolved and
 *               send_info updated with the ip/port. Even if dst is non
 *               null send_info must contain the protocol and if a non
 *               default port or non srv. lookup is desired, the port must
 *               be !=0 
 *   port      - used only if dst!=0 (else the port in send_info->to is used)
 *   send_info - filled dest_info structure:
 *               if the send_socket member is null, a send_socket will be 
 *               chosen automatically
 * WARNING: don't forget to zero-fill all the  unused members (a non-zero 
 * random id along with proto==PROTO_TCP can have bad consequences, same for
 *   a bogus send_socket value)
 */
int forward_request(struct sip_msg* msg, str* dst, unsigned short port,
							struct dest_info* send_info)
{
	unsigned int len;
	char* buf;
	char md5[MD5_LEN];
	struct socket_info* orig_send_sock; /* initial send_sock */
	int ret;
	struct ip_addr ip; /* debugging only */
#ifdef USE_DNS_FAILOVER
	struct socket_info* prev_send_sock;
	int err;
	struct dns_srv_handle dns_srv_h;
	
	prev_send_sock=0;
	err=0;
#endif
	
	
	buf=0;
	orig_send_sock=send_info->send_sock;
	ret=0;

	if(dst){
#ifdef USE_DNS_FAILOVER
		if (cfg_get(core, core_cfg, use_dns_failover)){
			dns_srv_handle_init(&dns_srv_h);
			err=dns_sip_resolve2su(&dns_srv_h, &send_info->to, dst, port,
									&send_info->proto, dns_flags);
			if (err!=0){
				LOG(L_ERR, "ERROR: forward_request: resolving \"%.*s\""
						" failed: %s [%d]\n", dst->len, ZSW(dst->s),
						dns_strerror(err), err);
				ret=E_BAD_ADDRESS;
				goto error;
			}
		}else
#endif
		if (sip_hostport2su(&send_info->to, dst, port, &send_info->proto)<0){
			LOG(L_ERR, "ERROR: forward_request: bad host name %.*s,"
						" dropping packet\n", dst->len, ZSW(dst->s));
			ret=E_BAD_ADDRESS;
			goto error;
		}
	}/* dst */
	/* calculate branch for outbound request;  if syn_branch is turned off,
	   calculate is from transaction key, i.e., as an md5 of From/To/CallID/
	   CSeq exactly the same way as TM does; good for reboot -- than messages
	   belonging to transaction lost due to reboot will still be forwarded
	   with the same branch parameter and will be match-able downstream

       if it is turned on, we don't care about reboot; we simply put a simple
	   value in there; better for performance
	*/
	if (syn_branch ) {
		*msg->add_to_branch_s='0';
		msg->add_to_branch_len=1;
	} else {
		if (!char_msg_val( msg, md5 )) 	{ /* parses transaction key */
			LOG(L_ERR, "ERROR: forward_request: char_msg_val failed\n");
			ret=E_UNSPEC;
			goto error;
		}
		msg->hash_index=hash( msg->callid->body, get_cseq(msg)->number);
		if (!branch_builder( msg->hash_index, 0, md5, 0 /* 0-th branch */,
					msg->add_to_branch_s, &msg->add_to_branch_len )) {
			LOG(L_ERR, "ERROR: forward_request: branch_builder failed\n");
			ret=E_UNSPEC;
			goto error;
		}
	}
	/* try to send the message until success or all the ips are exhausted
	 *  (if dns lookup is peformed && the dns cache used ) */
#ifdef USE_DNS_FAILOVER
	do{
#endif
		if (orig_send_sock==0) /* no forced send_sock => find it **/
			send_info->send_sock=get_send_socket(msg, &send_info->to,
												send_info->proto);
		if (send_info->send_sock==0){
			LOG(L_ERR, "forward_req: ERROR: cannot forward to af %d, proto %d "
						"no corresponding listening socket\n",
						send_info->to.s.sa_family, send_info->proto);
			ret=ser_error=E_NO_SOCKET;
#ifdef USE_DNS_FAILOVER
			/* continue, maybe we find a socket for some other ip */
			continue;
#else
			goto error;
#endif
		}
	
#ifdef USE_DNS_FAILOVER
		if (prev_send_sock!=send_info->send_sock){
			/* rebuild the message only if the send_sock changed */
			prev_send_sock=send_info->send_sock;
#endif
			if (buf) pkg_free(buf);
			buf = build_req_buf_from_sip_req(msg, &len, send_info);
			if (!buf){
				LOG(L_ERR, "ERROR: forward_request: building failed\n");
				ret=E_OUT_OF_MEM; /* most probable */
				goto error;
			}
#ifdef USE_DNS_FAILOVER
		}
#endif
		 /* send it! */
		DBG("Sending:\n%.*s.\n", (int)len, buf);
		DBG("orig. len=%d, new_len=%d, proto=%d\n",
				msg->len, len, send_info->proto );
	
		if (run_onsend(msg, send_info, buf, len)==0){
			su2ip_addr(&ip, &send_info->to);
			LOG(L_INFO, "forward_request: request to %s:%d(%d) dropped"
					" (onsend_route)\n", ip_addr2a(&ip),
						su_getport(&send_info->to), send_info->proto);
			ser_error=E_OK; /* no error */
			ret=E_ADM_PROHIBITED;
#ifdef USE_DNS_FAILOVER
			continue; /* try another ip */
#else
			goto error; /* error ? */
#endif
		}
#ifdef USE_DST_BLACKLIST
		if (cfg_get(core, core_cfg, use_dst_blacklist)){
			if (dst_is_blacklisted(send_info, msg)){
				su2ip_addr(&ip, &send_info->to);
				LOG(L_DBG, "DEBUG: blacklisted destination:%s:%d (%d)\n",
							ip_addr2a(&ip), su_getport(&send_info->to),
							send_info->proto);
				ret=ser_error=E_SEND;
#ifdef USE_DNS_FAILOVER
				continue; /* try another ip */
#else
				goto error;
#endif
			}
		}
#endif
		if (msg_send(send_info, buf, len)<0){
			ret=ser_error=E_SEND;
#ifdef USE_DST_BLACKLIST
			if (cfg_get(core, core_cfg, use_dst_blacklist))
				dst_blacklist_add(BLST_ERR_SEND, send_info, msg);
#endif
#ifdef USE_DNS_FAILOVER
			continue; /* try another ip */
#else
			goto error;
#endif
		}else{
			ret=ser_error=E_OK;
			/* sent requests stats */
			STATS_TX_REQUEST(  msg->first_line.u.request.method_value );
			/* exit succcesfully */
			goto end;
		}
#ifdef USE_DNS_FAILOVER
	}while(dst && cfg_get(core, core_cfg, use_dns_failover) &&
			dns_srv_handle_next(&dns_srv_h, err) && 
			((err=dns_sip_resolve2su(&dns_srv_h, &send_info->to, dst, port,
								  &send_info->proto, dns_flags))==0));
	if ((err!=0) && (err!=-E_DNS_EOR)){
		LOG(L_ERR, "ERROR:  resolving %.*s host name in uri"
							" failed: %s [%d] (dropping packet)\n",
									dst->len, ZSW(dst->s),
									dns_strerror(err), err);
		ret=ser_error=E_BAD_ADDRESS;
		goto error;
	}
#endif
	
error:
	STATS_TX_DROPS;
end:
#ifdef USE_DNS_FAILOVER
	if (dst && cfg_get(core, core_cfg, use_dns_failover)){
				dns_srv_handle_put(&dns_srv_h);
	}
#endif
	if (buf) pkg_free(buf);
	/* received_buf & line_buf will be freed in receive_msg by free_lump_list*/
	return ret;
}



int update_sock_struct_from_via( union sockaddr_union* to,
								 struct sip_msg* msg,
								 struct via_body* via )
{
	struct hostent* he;
	str* name;
	int err;
	unsigned short port;
	char proto;

	port=0;
	if(via==msg->via1){ 
		/* _local_ reply, we ignore any rport or received value
		 * (but we will send back to the original port if rport is
		 *  present) */
		if ((msg->msg_flags&FL_FORCE_RPORT)||(via->rport))
			port=msg->rcv.src_port;
		else port=via->port;
		name=&(via->host); /* received=ip in 1st via is ignored (it's
							  not added by us so it's bad) */
	}else{
		/* "normal" reply, we use rport's & received value if present */
		if (via->rport && via->rport->value.s){
			DBG("update_sock_struct_from_via: using 'rport'\n");
			port=str2s(via->rport->value.s, via->rport->value.len, &err);
			if (err){
				LOG(L_NOTICE, "ERROR: update_sock_struct_from_via: bad rport value(%.*s)\n",
						via->rport->value.len, via->rport->value.s);
				port=0;
			}
		}
		if (via->received){
			DBG("update_sock_struct_from_via: using 'received'\n");
			name=&(via->received->value);
			/* making sure that we won't do SRV lookup on "received"
			 * (possible if no DNS_IP_HACK is used)*/
			if (port==0) port=via->port?via->port:SIP_PORT; 
		}else{
			DBG("update_sock_struct_from_via: using via host\n");
			name=&(via->host);
			if (port==0) port=via->port;
		}
	}
	/* we do now a malloc/memcpy because gethostbyname loves \0-terminated 
	   strings; -jiri 
	   but only if host is not null terminated
	   (host.s[len] will always be ok for a via)
	    BTW: when is via->host.s non null terminated? tm copy? - andrei 
	    Yes -- it happened on generating a 408 by TM; -jiri
	    sip_resolvehost now accepts str -janakj
	*/
	DBG("update_sock_struct_from_via: trying SRV lookup\n");
	proto=via->proto;
	he=sip_resolvehost(name, &port, &proto);
	
	if (he==0){
		LOG(L_NOTICE, "ERROR:forward_reply:resolve_host(%.*s) failure\n",
				name->len, name->s);
		return -1;
	}
		
	hostent2su(to, he, 0, port);
	return 1;
}



/* removes first via & sends msg to the second */
int forward_reply(struct sip_msg* msg)
{
	char* new_buf;
	struct dest_info dst;
	unsigned int new_len;
	int r;
#ifdef USE_TCP
	char* s;
	int len;
#endif
	init_dest_info(&dst);
	new_buf=0;
	/*check if first via host = us */
	if (check_via){
		if (check_self(&msg->via1->host,
					msg->via1->port?msg->via1->port:SIP_PORT,
					msg->via1->proto)!=1){
			LOG(L_NOTICE, "ERROR: forward_reply: host in first via!=me :"
					" %.*s:%d\n", msg->via1->host.len, msg->via1->host.s,
									msg->via1->port);
			/* send error msg back? */
			goto error;
		}
	}
	
	/* check modules response_f functions */
	for (r=0; r<mod_response_cbk_no; r++)
		if (mod_response_cbks[r](msg)==0) goto skip;
	/* we have to forward the reply stateless, so we need second via -bogdan*/
	if (parse_headers( msg, HDR_VIA2_F, 0 )==-1 
		|| (msg->via2==0) || (msg->via2->error!=PARSE_OK))
	{
		/* no second via => error */
		LOG(L_ERR, "ERROR: forward_reply: no 2nd via found in reply\n");
		goto error;
	}

	new_buf = build_res_buf_from_sip_res( msg, &new_len);
	if (!new_buf){
		LOG(L_ERR, "ERROR: forward_reply: building failed\n");
		goto error;
	}

	dst.proto=msg->via2->proto;
	if (update_sock_struct_from_via( &dst.to, msg, msg->via2 )==-1) goto error;
#ifdef USE_COMP
	dst.comp=msg->via2->comp_no;
#endif

#ifdef USE_TCP
	if (dst.proto==PROTO_TCP
#ifdef USE_TLS
			|| dst.proto==PROTO_TLS
#endif
			){
		/* find id in i param if it exists */
		if (msg->via1->i && msg->via1->i->value.s){
			s=msg->via1->i->value.s;
			len=msg->via1->i->value.len;
			DBG("forward_reply: i=%.*s\n",len, ZSW(s));
			if (reverse_hex2int(s, len, (unsigned int*)&dst.id)<0){
				LOG(L_ERR, "ERROR: forward_reply: bad via i param \"%.*s\"\n",
						len, ZSW(s));
					dst.id=0;
			}
		}		
				
	} 
#endif
	if (msg_send(&dst, new_buf, new_len)<0) goto error;
#ifdef STATS
	STATS_TX_RESPONSE(  (msg->first_line.u.reply.statuscode/100) );
#endif

	DBG(" reply forwarded to %.*s:%d\n", 
			msg->via2->host.len, msg->via2->host.s,
			(unsigned short) msg->via2->port);

	pkg_free(new_buf);
skip:
	return 0;
error:
	if (new_buf) pkg_free(new_buf);
	return -1;
}
