/*
 * $Id$
 *
 * utilities
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
 *              specified explicitely (jiri)
 *  2003-07-07  get_proto takes now two protos as arguments (andrei)
 *              tls/sips support for get_proto & uri2proxy (andrei)
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
				case PROTO_UDP:/* transport specified explicitely */
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
	
	if (parsed_uri.secure){
		if ((parsed_uri.proto!=PROTO_TCP) && (parsed_uri.proto!=PROTO_NONE)){
			LOG(L_ERR, "ERROR: uri2proxy: bad transport  for sips uri: %d\n",
					parsed_uri.proto);
			return 0;
		}else
			uri_proto=PROTO_TLS;
	}else
		uri_proto=parsed_uri.proto;
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
 * Convert a URI into socket_info
 */
static inline struct socket_info *uri2sock(str *uri, union sockaddr_union *to_su, int proto)
{
	struct proxy_l *proxy;
	struct socket_info* send_sock;

	proxy = uri2proxy(uri, proto);
	if (!proxy) {
		ser_error = E_BAD_ADDRESS;
		LOG(L_ERR, "ERROR: uri2sock: Can't create a dst proxy\n");
		return 0;
	}
	
	hostent2su(to_su, &proxy->host, proxy->addr_idx, 
		   (proxy->port) ? proxy->port : SIP_PORT);
			/* we use proxy->proto since uri2proxy just set it correctly*/
	send_sock = get_send_socket(to_su, proxy->proto);
	if (!send_sock) {
		LOG(L_ERR, "ERROR: uri2sock: no corresponding socket for af %d\n", 
		    to_su->s.sa_family);
		ser_error = E_NO_SOCKET;
	}

	free_proxy(proxy);
	pkg_free(proxy);
	return send_sock;
}

#endif /* _TM_UT_H */
