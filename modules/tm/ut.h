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
 */
/*
 * History:
 * -------
 *  2003-02-13  added proto to uri2proxy (andrei)
 *  2003-04-14  added get_proto to determine protocol from uri unless
 *              specified explicitely (jiri)
*/


#ifndef _TM_UT_H
#define _TM_UT_H

#include "defs.h"
#include "../../ip_addr.h"


#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../str.h"
#include "../../parser/msg_parser.h"


inline static enum sip_protos get_proto(enum sip_protos force_proto,
	struct sip_uri *u)
{
	/* calculate transport protocol */
	switch(force_proto) {
		case PROTO_NONE: 	/* no protocol has been forced -- look at uri */
			switch(u->proto) {
				case PROTO_NONE: /* uri default to UDP */
					return PROTO_UDP;
				case PROTO_UDP: /* transport specified explicitely */
#ifdef USE_TCP
				case PROTO_TCP:
#endif
					return u->proto;
				default:
					LOG(L_ERR, "ERROR: get_proto: unsupported transport: %d\n",
						u->proto );
					return PROTO_NONE;
			}
		case PROTO_UDP: /* some protocol has been forced -- take it */
#ifdef USE_TCP
		case PROTO_TCP:
#endif
			return force_proto;
		default:
			LOG(L_ERR, "ERROR: get_proto: unsupported forced protocol: "
				"%d\n", force_proto);
			return PROTO_NONE;
	}
}

inline static struct proxy_l *uri2proxy( str *uri, int proto )
{
	struct sip_uri parsed_uri;
	unsigned int  port; 
	struct proxy_l *p;
	int err;
	enum sip_protos out_proto;

	if (parse_uri(uri->s, uri->len, &parsed_uri)<0) {
		LOG(L_ERR, "ERROR: t_relay: bad_uri: %.*s\n",
			uri->len, uri->s );
		return 0;
	}
	if (parsed_uri.port.s){ 
		port=str2s(parsed_uri.port.s, parsed_uri.port.len, &err);
		if (err){
			LOG(L_ERR, "ERROR: t_relay: bad port in uri: <%.*s>\n",
				parsed_uri.port.len, parsed_uri.port.s);
			return 0;
		}
	/* fixed use of SRV resolver
	} else port=SIP_PORT; */
	} else port=0;

	out_proto=get_proto(proto,&parsed_uri);
	if (out_proto==PROTO_NONE) {
		LOG(L_ERR, "ERROR: uri2proxy: transport can't be determined "
			"for URI <%.*s>\n", uri->len, uri->s );
		return 0;
	}

	p=mk_proxy(&(parsed_uri.host), port, out_proto);
	if (p==0) {
		LOG(L_ERR, "ERROR: t_relay: bad host name in URI <%.*s>\n",
			uri->len, uri->s);
		return 0;
	}
	return p;
}

#endif
