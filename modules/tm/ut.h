/*
 * $Id$
 *
 * utilities
 *
 */

#ifndef _TM_UT_H
#define _TM_UT_H

#include "../../dprint.h"
#include "../../error.h"
#include "../../ut.h"
#include "../../str.h"
#include "../../parser/msg_parser.h"

inline static struct proxy_l *uri2proxy( str *uri )
{
	struct sip_uri  parsed_uri;
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
			goto error;
		}
	} else port=SIP_PORT;
	p=mk_proxy(parsed_uri.host.s, port);
	if (p==0) {
		LOG(L_ERR, "ERROR: t_relay: bad host name in URI <%.*s>\n",
			uri->len, uri->s);
		goto error;
	}
	free_uri( &parsed_uri );
	return p;

error:
	free_uri( &parsed_uri );
	return 0;
	
}

#endif
