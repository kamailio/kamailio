/*
 * $Id$
 *
 * utilities
 *
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
 *  2003-02-13  added proto to uri2proxy (andrei)
 *  2003-04-09  uri2sock moved from uac.c (janakj)
 *  2003-04-14  added get_proto to determine protocol from uri unless
 *              specified explicitly (jiri)
 *  2003-07-07  get_proto takes now two protos as arguments (andrei)
 *              tls/sips support for get_proto & uri2proxy (andrei)
 *  2006-04-13  added uri2dst(), simplified uri2sock() (andrei)
 *  2006-08-11  dns failover support: uri2dst uses the dns cache and tries to 
 *               get the first ip for which there is a send sock. (andrei)
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
#endif

/* a forced_proto takes precedence if != PROTO_NONE */
inline static enum sip_protos get_proto(enum sip_protos force_proto,
										enum sip_protos proto)
{
	/* calculate transport protocol */
	switch(force_proto) {
		case PROTO_NONE: /* no protocol has been forced -- look at proto */
			switch(proto) {
				case PROTO_NONE: /* uri default to UDP */
						return PROTO_UDP;
				case PROTO_UDP:/* transport specified explicitly */
#ifdef USE_TCP
				case PROTO_TCP:
#endif
#ifdef USE_TLS
				case PROTO_TLS:
#endif
						return proto;
				default:
						LOG(L_ERR, "ERROR: get_proto: unsupported transport:"
								" %d\n", proto );
						return PROTO_NONE;
			}
		case PROTO_UDP: /* some protocol has been forced -- take it */
#ifdef USE_TCP
		case PROTO_TCP:
#endif
#ifdef USE_TLS
		case PROTO_TLS:
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
		if ((parsed_uri.proto!=PROTO_TCP) && (parsed_uri.proto!=PROTO_NONE)){
			LOG(L_ERR, "ERROR: uri2proxy: bad transport  for sips uri: %d\n",
					parsed_uri.proto);
			return 0;
		}else
			uri_proto=PROTO_TLS;
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
									short* proto, short* comp)
{
	struct sip_uri parsed_uri;
	enum sip_protos uri_proto;
	
	if (parse_uri(uri->s, uri->len, &parsed_uri) < 0) {
		LOG(L_ERR, "ERROR: get_uri_send_info: bad_uri: %.*s\n",
					uri->len, uri->s );
		return -1;
	}
	
	if (parsed_uri.type==SIPS_URI_T){
		if ((parsed_uri.proto!=PROTO_TCP) && (parsed_uri.proto!=PROTO_NONE)){
			LOG(L_ERR, "ERROR: get_uri_send_info: bad transport  for"
						" sips uri: %d\n", parsed_uri.proto);
			return -1;
		}else
			uri_proto=PROTO_TLS;
	}else
		uri_proto=parsed_uri.proto;
	
	*proto= get_proto(*proto, uri_proto);
#ifdef USE_COMP
	*comp=parsed_uri.comp;
#endif
#ifdef HONOR_MADDR
	if (parsed_uri.maddr_val.s && parsed_uri.maddr_val.len) {
		*host=parsed_uri.maddr;
		DBG("maddr dst: %.*s:%d\n", parsed_uri.maddr_val.len, 
				parsed_uri.maddr_val.s, parsed_uri.port_no);
	} else
#endif
		*host=parsed_uri.host;
	*port=parsed_uri.port_no;
	return 0;
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
 *         msg   -  sip message used to set dst->send_sock, if 0 dst->send_sock
 *                 will be set to the default w/o using msg->force_send_socket 
 *                 (see get_send_socket()) 
 *         uri   - uri in str form
 *         proto - if != PROTO_NONE, this protocol will be forced over the
 *                 uri_proto, otherwise the uri proto will be used
 * returns 0 on error, dst on success
 */
#ifdef USE_DNS_FAILOVER
inline static struct dest_info *uri2dst(struct dns_srv_handle* dns_h,
										struct dest_info* dst,
										struct sip_msg *msg, str *uri, 
											int proto )
#else
inline static struct dest_info *uri2dst(struct dest_info* dst,
										struct sip_msg *msg, str *uri, 
											int proto )
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
		if ((parsed_uri.proto!=PROTO_TCP) && (parsed_uri.proto!=PROTO_NONE)){
			LOG(L_ERR, "ERROR: uri2dst: bad transport  for sips uri: %d\n",
					parsed_uri.proto);
			return 0;
		}else
			uri_proto=PROTO_TLS;
	}else
		uri_proto=parsed_uri.proto;
	
	init_dest_info(dst);
	dst->proto= get_proto(proto, uri_proto);
#ifdef USE_COMP
	dst->comp=parsed_uri.comp;
#endif
#ifdef HONOR_MADDR
	if (parsed_uri.maddr_val.s && parsed_uri.maddr_val.len) {
		host=&parsed_uri.maddr_val;
		DBG("maddr dst: %.*s:%d\n", parsed_uri.maddr_val.len, 
								parsed_uri.maddr_val.s, parsed_uri.port_no);
	} else
#endif
		host=&parsed_uri.host;
#ifdef USE_DNS_FAILOVER
	if (use_dns_failover && dns_h){
		ip_found=0;
		do{
			/* try all the ips until we find a good send socket */
			err=dns_sip_resolve2su(dns_h, &to, host,
									parsed_uri.port_no, dst->proto, dns_flags);
			if (err!=0){
				if (ip_found==0){
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
			dst->send_sock = get_send_socket(msg, &to, dst->proto);
			if (dst->send_sock){
				dst->to=to;
				return dst; /* found a good one */
			}
		}while(dns_srv_handle_next(dns_h, err));
		LOG(L_ERR, "ERROR: uri2sock: no corresponding socket for \"%.*s\" "
					"af %d\n", host->len, ZSW(host->s), dst->to.s.sa_family);
		/* try to continue */
		return dst;
	}
#endif
	if (sip_hostport2su(&dst->to, host, parsed_uri.port_no, dst->proto)!=0){
		LOG(L_ERR, "ERROR: uri2dst: failed to resolve \"%.*s\"\n",
					host->len, ZSW(host->s));
		return 0;
	}
	dst->send_sock = get_send_socket(msg, &dst->to, dst->proto);
	if (dst->send_sock==0) {
		LOG(L_ERR, "ERROR: uri2sock: no corresponding socket for af %d\n", 
					dst->to.s.sa_family);
		/* ser_error = E_NO_SOCKET;*/
		/* try to continue */
	}
	return dst;
}


#if 0
/*
 * Convert a URI into the corresponding sockaddr_union (address to send to) and
 *  send socket_info (socket/address from which to send)
 *  to_su is filled with the destination and the socket_info that will be 
 *  used for sending is returned.
 *  On error return 0.
 *
 *  NOTE: this function is deprecated, you should use uri2dst instead
 */
static inline struct socket_info *uri2sock(struct sip_msg* msg, str *uri,
									union sockaddr_union *to_su, int proto)
{
	struct dest_info dst;

	if (uri2dst(&dst, msg, uri, proto)==0){
		LOG(L_ERR, "ERROR: uri2sock: Can't create a dst proxy\n");
		ser_error=E_BAD_ADDRESS;
		return 0;
	}
	*to_su=dst.to; /* copy su */
	
	/* we use dst->send_socket since uri2dst just set it correctly*/
	if (dst.send_sock==0) {
		LOG(L_ERR, "ERROR: uri2sock: no corresponding socket for af %d\n", 
		    to_su->s.sa_family);
		ser_error = E_NO_SOCKET;
	}
	return dst.send_sock;
}
#endif

#endif /* _TM_UT_H */
