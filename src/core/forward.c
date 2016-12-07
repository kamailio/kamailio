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
 *
 */

/*!
 * \file
 * \brief Kamailio core :: Message forwarding
 * \ingroup core
 * Module: \ref core
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
#include "char_msg_val.h"
#include "route.h"
#include "events.h"
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
#include "core_stats.h"

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif



/* return a socket_info_pointer to the sending socket; as opposed to
 * get_send_socket, which returns process's default socket, get_out_socket
 * attempts to determine the outbound interface which will be used;
 * it uses a temporary connected socket to determine it; it will
 * be very likely noticeably slower, but it can deal better with
 * multihomed hosts
 */

static int mhomed_sock_cache_disabled = 0;
static int sock_inet = -1;
static int sock_inet6 = -1;
static int _forward_set_send_info = 0;

void forward_set_send_info(int v)
{
	_forward_set_send_info = v;
}

static void apply_force_send_socket(struct dest_info* dst, struct sip_msg* msg);

struct socket_info* get_out_socket(union sockaddr_union* to, int proto)
{
	int* temp_sock;
	socklen_t len;
	union sockaddr_union from; 
	struct socket_info* si;
	struct ip_addr ip;
	union sockaddr_union uncon;

	memset(&uncon, 0, sizeof(union sockaddr_union));
	uncon.sin.sin_family = AF_UNSPEC;

	if (unlikely(proto!=PROTO_UDP)) {
		LM_CRIT("can only be called for UDP\n");
		return 0;
	}
retry:
	switch(to->s.sa_family){
	case AF_INET : {
		if(unlikely(sock_inet < 0)){
			sock_inet = socket(AF_INET, SOCK_DGRAM, 0);
			if (sock_inet==-1) {
				LM_ERR("socket() failed: %s\n", strerror(errno));
				return 0;
			}
		}
		temp_sock = &sock_inet;
		break;
	}
	case AF_INET6 : {
		if(unlikely(sock_inet6 < 0)){
			sock_inet6 = socket(AF_INET6, SOCK_DGRAM, 0);
			if (sock_inet6==-1) {
				LM_ERR("socket() failed: %s\n", strerror(errno));
				return 0;
			}
		}
		temp_sock = &sock_inet6;
		break;
	}
	default: {
		LM_ERR("Unknown protocol family \n");
		return 0;
	}
	}

	if( !mhomed_sock_cache_disabled ){
		/* some Linux kernel versions (all?) along with other UNIXes don't re-bound the sock if already bound */
		/* to un-bound a socket set sin_family to AF_UNSPEC and zero out the rest*/
		if (unlikely(connect(*temp_sock, &uncon.s, sockaddru_len(uncon)) < 0))
				mhomed_sock_cache_disabled = 1;
	}

	if (unlikely(connect(*temp_sock, &to->s, sockaddru_len(*to))==-1)) {
		if (unlikely(errno==EISCONN && !mhomed_sock_cache_disabled)){
			/*  no multiple connects support on the same socket */
			mhomed_sock_cache_disabled=1;
			if (sock_inet>=0){
				close(sock_inet);
				sock_inet=-1;
			}
			if (sock_inet6>=0){
				close(sock_inet6);
				sock_inet6=-1;
			}
			goto retry;
		}
		LM_ERR("connect failed: %s\n", strerror(errno));
		goto error;
	}
	len=sizeof(from);
	if (unlikely(getsockname(*temp_sock, &from.s, &len)==-1)) {
		LM_ERR("getsockname failed: %s\n", strerror(errno));
		goto error;
	}
	su2ip_addr(&ip, &from);
	si=find_si(&ip, 0, proto);
	if (si==0) goto error;
	LM_DBG("socket determined: %p\n", si );
	if (unlikely(mhomed_sock_cache_disabled)){
		close(*temp_sock);
		*temp_sock=-1;
	}
	return si;
error:
	LM_ERR("no socket found\n");
	ERR("no corresponding socket found for(%s:%s)\n",
			proto2a(proto), su2a(to, sizeof(*to)));
	if (unlikely(mhomed_sock_cache_disabled && *temp_sock >=0)){
		close(*temp_sock);
		*temp_sock=-1;
	}
	return 0;
}



/** get the sending socket for a corresponding destination.
 * @param force_send_socket - if !=0 and the protocol and af correspond
 *                            with the destination, it will be returned.
 *                            If the protocol or af check fail, a look-alike
 *                            socket will be searched for and mismatch will be
 *                            set. If no look-alike socket is found it will
 *                            fallback to normal resolution.
 * @param to - destination
 * @param proto - protocol
 * @param mismatch - result parameter, set if a force_send_socket was used, but
 *                   there was an error matching it exactly to the destination.
 *                   Possible values: 0 ok, SS_MISMATCH_PROTO,
 *                   SS_MISMATCH_ADDR, SS_MISMATCH_AF, SS_MISMATCH_MCAST.
 * @return a socket_info pointer to the sending socket on success (and possibly
 *         sets mismatch) or 0 on error.
 */
struct socket_info* get_send_socket2(struct socket_info* force_send_socket,
										union sockaddr_union* to, int proto,
										enum ss_mismatch* mismatch)
{
	struct socket_info* send_sock;
	struct socket_info* orig;
	
	if (likely(mismatch)) *mismatch=0;
	/* check if send interface is not forced */
	if (unlikely(force_send_socket)){
		orig=force_send_socket;
		/* Special case here as there is no ;transport=wss - so wss connections will
		   appear as ws ones and be sorted out in the WebSocket module */
		if (unlikely(orig->proto!=proto && !(orig->proto==PROTO_TLS && proto==PROTO_WS))){
			force_send_socket=find_si(&(force_send_socket->address),
											force_send_socket->port_no,
											proto);
			if (unlikely(force_send_socket == 0)){
				if (likely(mismatch)) *mismatch=SS_MISMATCH_ADDR;
				LM_WARN("protocol/port mismatch (forced %s:%s:%d, to %s:%s)\n",
						proto2a(orig->proto), ip_addr2a(&orig->address),
						orig->port_no,
						proto2a(proto), su2a(to, sizeof(*to)));
				goto not_forced;
			}
			if (likely(mismatch)) *mismatch=SS_MISMATCH_PROTO;
		}
		if (unlikely(force_send_socket->address.af!=to->s.sa_family)){
			LM_DBG("force_send_socket of different af"
					" (dst %d - %s:%s forced %d -%s:%s:%d)\n",
					to->s.sa_family, proto2a(proto), su2a(to, sizeof(*to)),
					force_send_socket->address.af,
					proto2a(force_send_socket->proto),
					ip_addr2a(&force_send_socket->address),
					force_send_socket->port_no);
			if (likely(mismatch)) *mismatch=SS_MISMATCH_AF;
			goto not_forced;
		}
		/* check if listening on the socket (the check does not work
		   for TCP and TLS, for them socket==-1 on all the processes
		   except tcp_main(), see close_extra_socks() */
		if (likely((force_send_socket->socket!=-1 ||
						force_send_socket->proto==PROTO_TCP ||
						force_send_socket->proto==PROTO_TLS ||
						force_send_socket->proto==PROTO_WS  ||
						force_send_socket->proto==PROTO_WSS) &&
					!(force_send_socket->flags & SI_IS_MCAST)))
				return force_send_socket;
		else{
			if (!(force_send_socket->flags & SI_IS_MCAST))
				LM_WARN("not listening on the requested socket (%s:%s:%d),"
							 " no fork mode?\n",
							proto2a(force_send_socket->proto),
							ip_addr2a(&force_send_socket->address),
							force_send_socket->port_no);
			else if (likely(mismatch)) *mismatch=SS_MISMATCH_MCAST;
		}
	};
not_forced:
	if (mhomed && proto==PROTO_UDP){
		send_sock=get_out_socket(to, proto);
		if ((send_sock==0) || (send_sock->socket!=-1))
			return send_sock; /* found or error*/
		else if (send_sock->socket==-1){
			LM_WARN("not listening on the"
					" requested socket (%s:%s:%d), no fork mode?\n",
					proto2a(send_sock->proto), ip_addr2a(&send_sock->address),
					send_sock->port_no);
			/* continue: try to use some socket */
		}
	}

	send_sock=0;
	/* check if we need to change the socket (different address families -
	 * eg: ipv4 -> ipv6 or ipv6 -> ipv4) */
	switch(proto){
#ifdef USE_TCP
		case PROTO_WS:
		case PROTO_TCP:
		/* on tcp just use the "main address", we don't really now the
		 * sending address (we can find it out, but we'll need also to see
		 * if we listen on it, and if yes on which port -> too complicated*/
			switch(to->s.sa_family){
				/* FIXME */
				case AF_INET:	send_sock=sendipv4_tcp;
								break;
				case AF_INET6:	send_sock=sendipv6_tcp;
								break;
				default:	LM_ERR("don't know how to forward to af %d\n",
									to->s.sa_family);
			}
			break;
#endif
#ifdef USE_TLS
		case PROTO_WSS:
		case PROTO_TLS:
			switch(to->s.sa_family){
				/* FIXME */
				case AF_INET:	send_sock=sendipv4_tls;
								break;
				case AF_INET6:	send_sock=sendipv6_tls;
								break;
				default:	LM_ERR("don't know how to forward to af %d\n",
									to->s.sa_family);
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
					case AF_INET6:	send_sock=sendipv6_sctp;
									break;
					default:	LM_ERR("don't know how to forward to af %d\n",
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
					case AF_INET6:	send_sock=sendipv6;
									break;
					default:	LM_ERR("don't know how to forward to af %d\n",
										to->s.sa_family);
				}
			}else send_sock=bind_address;
			break;
		default:
			LM_CRIT("unsupported proto %d (%s)\n", proto, proto2a(proto));
	}
	return send_sock;
}

static struct _check_self_func {
	check_self_f fself;
	struct _check_self_func *next;
} *_check_self_func_list = NULL;

/** check if _check_self_func_list is set
 * - return 1 if yes, 0 if no
 */
int is_check_self_func_list_set(void)
{
	return (_check_self_func_list)?1:0;
}

/** register a function to be called when matching for myself
 * - return 0 on success, -1 on error
 * - f must have same prototype as check_self() and return same kind of values
 */
int register_check_self_func(check_self_f f)
{
	struct _check_self_func *nf = 0;
	nf=(struct _check_self_func*)pkg_malloc(sizeof(struct _check_self_func));
	if(nf==0)
	{
		LM_ERR("no more pkg\n");
		return -1;
	}
	nf->fself = f;
	nf->next = _check_self_func_list;
	_check_self_func_list = nf;
	return 0;
}

/** run registered check self functions
 * returns 1 if true, 0 if false
 */
int run_check_self_func(str* host, unsigned short port, unsigned short proto)
{
	struct _check_self_func *sf = 0;

	if(_check_self_func_list==NULL)
		return 0;
	for(sf=_check_self_func_list; sf; sf=sf->next)
		if(sf->fself(host, port, proto)==1)
			return 1;
	return 0;
}

/** checks if the proto: host:port is one of the address we listen on;
 *
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
		LM_DBG("host != me\n");
		return (_check_self_func_list==NULL)?0:run_check_self_func(host,
														port, proto);
	}
found:
	return 1;
}

/** checks if the proto:port is one of the ports we listen on;
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



/** forwards a request to dst
 * parameters:
 *   msg       - sip msg
 *   dst       - destination name, if non-null it will be resolved and
 *               send_info updated with the ip/port. Even if dst is non
 *               null send_info must contain the protocol and if a non
 *               default port or non srv. lookup is desired, the port must
 *               be !=0 
 *   port      - used only if dst!=0 (else the port in send_info->to is used)
 *   send_info - value/result partially filled dest_info structure:
 *                 - send_info->proto and comp are used
 *                 - send_info->to will be filled (dns)
 *                 - send_info->send_flags is filled from the message
 *                 - if the send_socket member is null, a send_socket will be 
 *                   chosen automatically
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
	char proto;
	struct onsend_info onsnd_info = {0};
#ifdef USE_DNS_FAILOVER
	struct socket_info* prev_send_sock;
	int err;
	struct dns_srv_handle dns_srv_h;
	
	prev_send_sock=0;
	err=0;
#endif
	
	
	buf=0;
	orig_send_sock=send_info->send_sock;
	proto=send_info->proto;
	ret=0;

	if(dst){
#ifdef USE_DNS_FAILOVER
		if (cfg_get(core, core_cfg, use_dns_failover)){
			dns_srv_handle_init(&dns_srv_h);
			err=dns_sip_resolve2su(&dns_srv_h, &send_info->to, dst, port,
									&proto, dns_flags);
			if (err!=0){
				LM_ERR("resolving \"%.*s\" failed: %s [%d]\n",
						dst->len, ZSW(dst->s), dns_strerror(err), err);
				ret=E_BAD_ADDRESS;
				goto error;
			}
		}else
#endif
		if (sip_hostport2su(&send_info->to, dst, port, &proto)<0){
			LM_ERR("bad host name %.*s, dropping packet\n", dst->len, ZSW(dst->s));
			ret=E_BAD_ADDRESS;
			goto error;
		}
	}/* dst */
	send_info->send_flags=msg->fwd_send_flags;
	/* calculate branch for outbound request;
	   calculate is from transaction key, i.e., as an md5 of From/To/CallID/
	   CSeq exactly the same way as TM does; good for reboot -- than messages
	   belonging to transaction lost due to reboot will still be forwarded
	   with the same branch parameter and will be match-able downstream
	*/
	if (!char_msg_val( msg, md5 )) 	{ /* parses transaction key */
		LM_ERR("char_msg_val failed\n");
		ret=E_UNSPEC;
		goto error;
	}
	msg->hash_index=hash( msg->callid->body, get_cseq(msg)->number);
	if (!branch_builder( msg->hash_index, 0, md5, 0 /* 0-th branch */,
				msg->add_to_branch_s, &msg->add_to_branch_len )) {
		LM_ERR("branch_builder failed\n");
		ret=E_UNSPEC;
		goto error;
	}
	/* try to send the message until success or all the ips are exhausted
	 *  (if dns lookup is performed && the dns cache used ) */
#ifdef USE_DNS_FAILOVER
	do{
#endif
		if (orig_send_sock==0) /* no forced send_sock => find it **/
			send_info->send_sock=get_send_socket(msg, &send_info->to, proto);
		if (send_info->send_sock==0){
			LM_ERR("cannot forward to af %d, proto %d "
						"no corresponding listening socket\n",
						send_info->to.s.sa_family, proto);
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
			send_info->proto=proto;
			buf = build_req_buf_from_sip_req(msg, &len, send_info, 0);
			if (!buf){
				LM_ERR("building failed\n");
				ret=E_OUT_OF_MEM; /* most probable */
				goto error;
			}
#ifdef USE_DNS_FAILOVER
		}
#endif
		 /* send it! */
		LM_DBG("Sending:\n%.*s.\n", (int)len, buf);
		LM_DBG("orig. len=%d, new_len=%d, proto=%d\n",
				msg->len, len, send_info->proto );
	
		if (run_onsend(msg, send_info, buf, len)==0){
			su2ip_addr(&ip, &send_info->to);
			LM_INFO("request to %s:%d(%d) dropped (onsend_route)\n",
				ip_addr2a(&ip), su_getport(&send_info->to), send_info->proto);
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
				LM_DBG("blacklisted destination:%s:%d (%d)\n",
					ip_addr2a(&ip), su_getport(&send_info->to), send_info->proto);
				ret=ser_error=E_SEND;
#ifdef USE_DNS_FAILOVER
				continue; /* try another ip */
#else
				goto error;
#endif
			}
		}
#endif

		if(unlikely(_forward_set_send_info==1)) {
			onsnd_info.to=&send_info->to;
			onsnd_info.send_sock=send_info->send_sock;
			onsnd_info.buf=buf;
			onsnd_info.len=len;
			onsnd_info.msg=msg;
			p_onsend=&onsnd_info;
		}

		if (msg_send(send_info, buf, len)<0){
			p_onsend=0;
			ret=ser_error=E_SEND;
#ifdef USE_DST_BLACKLIST
			(void)dst_blacklist_add(BLST_ERR_SEND, send_info, msg);
#endif
#ifdef USE_DNS_FAILOVER
			continue; /* try another ip */
#else
			goto error;
#endif
		}else{
			p_onsend=0;
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
										&proto, dns_flags))==0));
	if ((err!=0) && (err!=-E_DNS_EOR)){
		LM_ERR("resolving %.*s host name in uri failed: %s [%d] (dropping packet)\n",
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
#if defined STATS_REQ_FWD_OK || defined STATS_REQ_FWD_DROP
	if(ret==0)
		STATS_REQ_FWD_OK();
	else
		STATS_REQ_FWD_DROP();
#endif /* STATS_REQ_FWD_* */
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
			LM_DBG("using 'rport'\n");
			port=str2s(via->rport->value.s, via->rport->value.len, &err);
			if (err){
				LM_ERR("bad rport value(%.*s)\n",
						via->rport->value.len, via->rport->value.s);
				port=0;
			}
		}
		if (via->received){
			LM_DBG("using 'received'\n");
			name=&(via->received->value);
			/* making sure that we won't do SRV lookup on "received"
			 * (possible if no DNS_IP_HACK is used)*/
			if (port==0) port=via->port?via->port:SIP_PORT; 
		}else{
			LM_DBG("using via host\n");
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
	LM_DBG("trying SRV lookup\n");
	proto=via->proto;
	he=sip_resolvehost(name, &port, &proto);
	
	if (he==0){
		LM_NOTICE("resolve_host(%.*s) failure\n", name->len, name->s);
		return -1;
	}
		
	hostent2su(to, he, 0, port);
	return 1;
}



/** removes first via & sends msg to the second
 * - mode param controls if modules sip response callbacks are executed */
static int do_forward_reply(struct sip_msg* msg, int mode)
{
	char* new_buf;
	struct dest_info dst;
	unsigned int new_len;
	int r;
	struct ip_addr ip;
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
			LM_ERR("host in first via!=me : %.*s:%d\n",
				msg->via1->host.len, msg->via1->host.s, msg->via1->port);
			/* send error msg back? */
			goto error;
		}
	}
	
	/* check modules response_f functions */
	if(likely(mode==0)) {
		for (r=0; r<mod_response_cbk_no; r++)
			if (mod_response_cbks[r](msg)==0) goto skip;
	}
	/* we have to forward the reply stateless, so we need second via -bogdan*/
	if (parse_headers( msg, HDR_VIA2_F, 0 )==-1 
		|| (msg->via2==0) || (msg->via2->error!=PARSE_OK))
	{
		/* no second via => error */
		LM_DBG("reply cannot be forwarded - no 2nd via\n");
		goto error;
	}

	new_buf = build_res_buf_from_sip_res( msg, &new_len);
	if (!new_buf){
		LM_ERR("building failed\n");
		goto error;
	}

	dst.proto=msg->via2->proto;
	SND_FLAGS_OR(&dst.send_flags, &msg->fwd_send_flags, &msg->rpl_send_flags);
	if (update_sock_struct_from_via( &dst.to, msg, msg->via2 )==-1) goto error;
#ifdef USE_COMP
	dst.comp=msg->via2->comp_no;
#endif

#if defined USE_TCP || defined USE_SCTP
	if (
#ifdef USE_TCP
			dst.proto==PROTO_TCP
			|| dst.proto==PROTO_WS
#ifdef USE_TLS
			|| dst.proto==PROTO_TLS
			|| dst.proto==PROTO_WSS
#endif
#ifdef USE_SCTP
			||
#endif /* USE_SCTP */
#endif /* USE_TCP */
#ifdef USE_SCTP
			dst.proto==PROTO_SCTP
#endif /* USE_SCTP */
			){
		/* find id in i param if it exists */
		if (msg->via1->i && msg->via1->i->value.s){
			s=msg->via1->i->value.s;
			len=msg->via1->i->value.len;
			LM_DBG("i=%.*s\n",len, ZSW(s));
			if (reverse_hex2int(s, len, (unsigned int*)&dst.id)<0){
				LM_ERR("bad via i param \"%.*s\"\n", len, ZSW(s));
				dst.id=0;
			}
		}		
				
	} 
#endif

	apply_force_send_socket(&dst, msg);

	/* call onsend_route */
	if(dst.send_sock == NULL) {
		dst.send_sock=get_send_socket(msg, &dst.to, dst.proto);
		if (dst.send_sock==0){
			LM_ERR("cannot forward reply\n");
			goto done;
		}
	}
	if (onsend_route_enabled(SIP_REPLY)){
		if (run_onsend(msg, &dst, new_buf, new_len)==0){
			su2ip_addr(&ip, &(dst.to));
			LOG(L_ERR, "forward_reply: reply to %s:%d(%d) dropped"
					" (onsend_route)\n", ip_addr2a(&ip),
						su_getport(&(dst.to)), dst.proto);
			goto error; /* error ? */
		}
	}

	if (msg_send(&dst, new_buf, new_len)<0)
	{
		STATS_RPL_FWD_DROP();
		goto error;
	}

	done:
#ifdef STATS
	STATS_TX_RESPONSE(  (msg->first_line.u.reply.statuscode/100) );
#endif

	LM_DBG("reply forwarded to %.*s:%d\n", 
			msg->via2->host.len, msg->via2->host.s,
			(unsigned short) msg->via2->port);

	STATS_RPL_FWD_OK();
	pkg_free(new_buf);
skip:
	return 0;
error:
	if (new_buf) pkg_free(new_buf);
	return -1;
}

/** removes first via & sends msg to the second */
int forward_reply(struct sip_msg* msg)
{
	return do_forward_reply(msg, 0);
}

/** removes first via & sends msg to the second - no module callbacks */
int forward_reply_nocb(struct sip_msg* msg)
{
	return do_forward_reply(msg, 1);
}

static void apply_force_send_socket(struct dest_info* dst, struct sip_msg* msg)
{
	if (msg->force_send_socket != 0) {
		dst->send_sock = get_send_socket(msg, &dst->to, dst->proto);
	}
}
