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
*/


#ifndef _TM_UT_H
#define _TM_UT_H

#include "defs.h"


#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../str.h"
#include "../../parser/msg_parser.h"

inline static struct proxy_l *uri2proxy( str *uri, int proto )
{
	struct sip_uri parsed_uri;
	unsigned int  port; 
	struct proxy_l *p;
	int err;

	if (parse_uri(uri->s, uri->len, &parsed_uri)<0) {
		LOG(L_ERR, "ERROR: t_relay: bad_uri: %.*s\n",
			uri->len, uri->s );
		return 0;
	}
	if (parsed_uri.port.s){ 
		port=str2s((unsigned char*)parsed_uri.port.s, parsed_uri.port.len, &err);
		if (err){
			LOG(L_ERR, "ERROR: t_relay: bad port in uri: <%.*s>\n",
				parsed_uri.port.len, parsed_uri.port.s);
			return 0;
		}
	/* fixed use of SRV resolver
	} else port=SIP_PORT; */
	} else port=0;
	p=mk_proxy(&(parsed_uri.host), port, proto);
	if (p==0) {
		LOG(L_ERR, "ERROR: t_relay: bad host name in URI <%.*s>\n",
			uri->len, uri->s);
		return 0;
	}
	return p;
}

#endif
