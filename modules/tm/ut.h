/*
 * utilities
 *
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


#ifndef _TM_UT_H
#define _TM_UT_H


#include "../../proxy.h"
#include "../../str.h"
#include "../../parser/parse_uri.h"
#include "../../dprint.h"
#include "../../ut.h"
#include "../../ip_addr.h"
#include "../../error.h"
#include "../../forward.h"
#include "../../mem/mem.h"
#include "../../parser/msg_parser.h"
#include "../../resolve.h"
#ifdef USE_DNS_FAILOVER
#include "../../dns_cache.h"
#include "../../cfg_core.h" /* cfg_get(core, core_cfg, use_dns_failover) */
#endif


/*! Which header fields should be skipped */
#define tm_skip_hf(_hf) \
	(((_hf)->type == HDR_FROM_T)  || \
	((_hf)->type == HDR_TO_T)     || \
	((_hf)->type == HDR_CALLID_T) || \
	((_hf)->type == HDR_CSEQ_T))


/* a forced_proto takes precedence if != PROTO_NONE */
inline static enum sip_protos get_proto(enum sip_protos force_proto,
										enum sip_protos proto)
{
	/* calculate transport protocol */
	switch(force_proto) {
		case PROTO_NONE: /* no protocol has been forced -- look at proto */
			switch(proto) {
				case PROTO_NONE: /* leave it to dns */
						return PROTO_NONE;
				case PROTO_UDP:/* transport specified explicitly */
#ifdef USE_TCP
				case PROTO_TCP:
				case PROTO_WS:
#endif
#ifdef USE_TLS
				case PROTO_TLS:
#endif
#ifdef USE_SCTP
				case PROTO_SCTP:
#endif
						return proto;
				case PROTO_WSS:	/* should never see ;transport=wss */
				default:
						LOG(L_ERR, "ERROR: get_proto: unsupported transport:"
								" %d\n", proto );
						return PROTO_NONE;
			}
		case PROTO_UDP: /* some protocol has been forced -- take it */
#ifdef USE_TCP
		case PROTO_TCP:
		case PROTO_WS:
#endif
#ifdef USE_TLS
		case PROTO_TLS:
		case PROTO_WSS:
#endif
#ifdef USE_SCTP
		case PROTO_SCTP:
#endif
			return force_proto;
		default:
			LOG(L_ERR, "ERROR: get_proto: unsupported forced protocol: "
					"%d\n", force_proto);
			return PROTO_NONE;
	}
}



/*
 * Convert a URI into a proxy structure
 */
inline static struct proxy_l *uri2proxy( str *uri, int proto )
{
	struct sip_uri parsed_uri;
	struct proxy_l *p;
	enum sip_protos uri_proto;

	if (parse_uri(uri->s, uri->len, &parsed_uri) < 0) {
		LOG(L_ERR, "ERROR: uri2proxy: bad_uri: %.*s\n",
		    uri->len, uri->s );
		return 0;
	}
	
	if (parsed_uri.type==SIPS_URI_T){
		if (parsed_uri.proto==PROTO_UDP) {
			LOG(L_ERR, "ERROR: uri2proxy: bad transport for sips uri: %d\n",
					parsed_uri.proto);
			return 0;
		}else if (parsed_uri.proto != PROTO_WS)
			uri_proto=PROTO_TLS;
		else
			uri_proto=PROTO_WS;
	}else
		uri_proto=parsed_uri.proto;
#ifdef HONOR_MADDR
	if (parsed_uri.maddr_val.s && parsed_uri.maddr_val.len) {
		p = mk_proxy(&parsed_uri.maddr_val, 
					  parsed_uri.port_no, 
					  get_proto(proto, uri_proto));
		if (p == 0) {
			LOG(L_ERR, "ERROR: uri2proxy: bad maddr param in URI <%.*s>\n",
				uri->len, ZSW(uri->s));
			return 0;
		}
	} else
#endif
	p = mk_proxy(&parsed_uri.host, 
				  parsed_uri.port_no, 
				  get_proto(proto, uri_proto));
	if (p == 0) {
		LOG(L_ERR, "ERROR: uri2proxy: bad host name in URI <%.*s>\n",
		    uri->len, ZSW(uri->s));
		return 0;
	}
	
	return p;
}



/*
 * parse uri and return send related information
 * params: uri - uri in string form
 *         host - filled with the uri host part
 *         port - filled with the uri port
 *         proto - if != PROTO_NONE, this protocol will be forced over the
 *                 uri_proto, otherwise the uri proto will be used 
 *                 (value/return)
 *         comp - compression (if used)
 * returns 0 on success, < 0 on error
 */
inline static int get_uri_send_info(str* uri, str* host, unsigned short* port,
									char* proto, short* comp)
{
	struct sip_uri parsed_uri;
	enum sip_protos uri_proto;
	
	if (parse_uri(uri->s, uri->len, &parsed_uri) < 0) {
		LOG(L_ERR, "ERROR: get_uri_send_info: bad_uri: %.*s\n",
					uri->len, uri->s );
		return -1;
	}
	
	if (parsed_uri.type==SIPS_URI_T){
		if (parsed_uri.proto==PROTO_UDP) {
			LOG(L_ERR, "ERROR: get_uri_send_info: bad transport for"
						" sips uri: %d\n", parsed_uri.proto);
			return -1;
		}else if (parsed_uri.proto != PROTO_WS)
			uri_proto=PROTO_TLS;
		else
			uri_proto=PROTO_WS;
	}else
		uri_proto=parsed_uri.proto;
	
	*proto= get_proto(*proto, uri_proto);
#ifdef USE_COMP
	*comp=parsed_uri.comp;
#endif
#ifdef HONOR_MADDR
	if (parsed_uri.maddr_val.s && parsed_uri.maddr_val.len) {
		*host=parsed_uri.maddr_val;
		DBG("maddr dst: %.*s:%d\n", parsed_uri.maddr_val.len, 
				parsed_uri.maddr_val.s, parsed_uri.port_no);
	} else
#endif
		*host=parsed_uri.host;
	*port=parsed_uri.port_no;
	return 0;
}



/*
 * Convert a URI into a dest_info structure.
 * Same as uri2dst, but uses directly force_send_socket instead of msg.
 * If the uri host resolves to multiple ips and dns_h!=0 the first ip for 
 *  which a send socket is found will be used. If no send_socket are found,
 *  the first ip is selected.
 *
 * params: dns_h - pointer to a valid dns_srv_handle structure (intialized!) or
 *                 null. If null or use_dns_failover==0 normal dns lookup will
 *                 be performed (no failover).
 *         dst   - will be filled
 *         force_send_sock - if 0 dst->send_sock will be set to the default 
 *                 (see get_send_socket2()) 
 *         sflags - send flags
 *         uri   - uri in str form
 *         proto - if != PROTO_NONE, this protocol will be forced over the
 *                 uri_proto, otherwise the uri proto will be used if set or
 *                 the proto obtained from the dns lookup
 * returns 0 on error, dst on success
 */
#ifdef USE_DNS_FAILOVER
inline static struct dest_info *uri2dst2(struct dns_srv_handle* dns_h,
										struct dest_info* dst,
										struct socket_info *force_send_socket,
										snd_flags_t sflags,
										str *uri, int proto )
#else
inline static struct dest_info *uri2dst2(struct dest_info* dst,
										struct socket_info *force_send_socket,
										snd_flags_t sflags,
										str *uri, int proto )
#endif
{
	struct sip_uri parsed_uri;
	enum sip_protos uri_proto;
	str* host;
#ifdef USE_DNS_FAILOVER
	int ip_found;
	union sockaddr_union to;
	int err;
#endif

	if (parse_uri(uri->s, uri->len, &parsed_uri) < 0) {
		LOG(L_ERR, "ERROR: uri2dst: bad_uri: %.*s\n",
		    uri->len, uri->s );
		return 0;
	}
	
	if (parsed_uri.type==SIPS_URI_T){
		if (parsed_uri.proto==PROTO_UDP) {
			LOG(L_ERR, "ERROR: uri2dst: bad transport for sips uri: %d\n",
					parsed_uri.proto);
			return 0;
		}else if (parsed_uri.proto!=PROTO_WS)
			uri_proto=PROTO_TLS;
		else
			uri_proto=PROTO_WS;
	}else
		uri_proto=parsed_uri.proto;
	
	init_dest_info(dst);
	dst->proto= get_proto(proto, uri_proto);
#ifdef USE_COMP
	dst->comp=parsed_uri.comp;
#endif
	dst->send_flags=sflags;
#ifdef HONOR_MADDR
	if (parsed_uri.maddr_val.s && parsed_uri.maddr_val.len) {
		host=&parsed_uri.maddr_val;
		DBG("maddr dst: %.*s:%d\n", parsed_uri.maddr_val.len, 
								parsed_uri.maddr_val.s, parsed_uri.port_no);
	} else
#endif
		host=&parsed_uri.host;
#ifdef USE_DNS_FAILOVER
	if (cfg_get(core, core_cfg, use_dns_failover) && dns_h){
		ip_found=0;
		do{
			/* try all the ips until we find a good send socket */
			err=dns_sip_resolve2su(dns_h, &to, host,
								parsed_uri.port_no, &dst->proto, dns_flags);
			if (err!=0){
				if (ip_found==0){
					if (err!=-E_DNS_EOR)
						LOG(L_ERR, "ERROR: uri2dst: failed to resolve \"%.*s\" :"
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
		}while(dns_srv_handle_next(dns_h, err));
		ERR("no corresponding socket for \"%.*s\" af %d\n", host->len, 
				ZSW(host->s), dst->to.s.sa_family);
		/* try to continue */
		return dst;
	}
#endif
	if (sip_hostport2su(&dst->to, host, parsed_uri.port_no, &dst->proto)!=0){
		ERR("failed to resolve \"%.*s\"\n", host->len, ZSW(host->s));
		return 0;
	}
	dst->send_sock = get_send_socket2(force_send_socket, &dst->to,
										dst->proto, 0);
	if (dst->send_sock==0) {
		ERR("no corresponding socket found for \"%.*s\" af %d (%s:%s)\n",
			host->len, ZSW(host->s), dst->to.s.sa_family,
			proto2a(dst->proto), su2a(&dst->to, sizeof(dst->to)));
		/* ser_error = E_NO_SOCKET;*/
		/* try to continue */
	}
	return dst;
}



/*
 * Convert a URI into a dest_info structure
 * If the uri host resolves to multiple ips and dns_h!=0 the first ip for 
 *  which a send socket is found will be used. If no send_socket are found,
 *  the first ip is selected.
 *
 * params: dns_h - pointer to a valid dns_srv_handle structure (intialized!) or
 *                 null. If null or use_dns_failover==0 normal dns lookup will
 *                 be performed (no failover).
 *         dst   - will be filled
 *         msg   -  sip message used to set dst->send_sock and dst->send_flags,
 *                 if 0 dst->send_sock will be set to the default w/o using 
 *                  msg->force_send_socket (see get_send_socket()) and the 
 *                  send_flags will be set to 0.
 *         uri   - uri in str form
 *         proto - if != PROTO_NONE, this protocol will be forced over the
 *                 uri_proto, otherwise the uri proto will be used if set or
 *                 the proto obtained from the dns lookup
 * returns 0 on error, dst on success
 */
#ifdef USE_DNS_FAILOVER
inline static struct dest_info *uri2dst(struct dns_srv_handle* dns_h,
										struct dest_info* dst,
										struct sip_msg *msg, str *uri, 
											int proto )
{
	snd_flags_t sflags;
	if (msg)
		return uri2dst2(dns_h, dst, msg->force_send_socket,
							msg->fwd_send_flags, uri, proto);
	SND_FLAGS_INIT(&sflags);
	return uri2dst2(dns_h, dst, 0, sflags, uri, proto);
}
#else
inline static struct dest_info *uri2dst(struct dest_info* dst,
										struct sip_msg *msg, str *uri, 
											int proto )
{
	snd_flags_t sflags;
	if (msg)
		return uri2dst2(dst, msg->force_send_socket, msg->fwd_send_flags,
						uri, proto);
	SND_FLAGS_INIT(&sflags);
	return uri2dst2(dst, 0, sflags, uri, proto);
}
#endif /* USE_DNS_FAILOVER */

#endif /* _TM_UT_H */
