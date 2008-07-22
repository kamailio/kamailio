/*
 * $Id$
 *
 * XMPP Module
 * This file is part of openser, a free SIP server.
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
 *
 * openser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * openser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andreea Spirea
 *
 */
/*! \file
 * \brief OpenSER XMPP module - utilities
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "xmpp.h"
#include "../../parser/parse_uri.h"

/*! \brief decode sip:user*domain1@domain2 -> user@domain1 
	\note In many kinds of gateway scenarios, the % sign is a common character used
		See the MSN XMPP transports for an example.
 */
char *decode_uri_sip_xmpp(char *uri)
{
	struct sip_uri puri;
	static char buf[512];
	char *p;

	if (!uri)
		return NULL;
	if (parse_uri(uri, strlen(uri), &puri) < 0) {
		LM_ERR("failed to parse URI\n");
		return NULL;
	}
	strncpy(buf, puri.user.s, sizeof(buf));
	buf[puri.user.len] = 0;
	
	/* replace domain separator */
	if ((p = strchr(buf, domain_separator)))
		*p = '@';

	return buf;
}

/*! \brief  encode sip:user@domain -> user*domain@xmpp_domain */
char *encode_uri_sip_xmpp(char *uri)
{
	struct sip_uri puri;
	static char buf[512];

	if (!uri)
		return NULL;
	if (parse_uri(uri, strlen(uri), &puri) < 0) {
		LM_ERR("failed to parse URI\n");
		return NULL;
	}
	snprintf(buf, sizeof(buf), "%.*s%c%.*s@%s",
		puri.user.len, puri.user.s,
		domain_separator,
		puri.host.len, puri.host.s,
		xmpp_domain);
	return buf;
}

/*! \brief  decode user*domain1@domain2 -> sip:user@domain1 */
char *decode_uri_xmpp_sip(char *jid)
{
	static char buf[512];
	char *p;

	if (!jid)
		return NULL;
	snprintf(buf, sizeof(buf), "sip:%s", jid);

	/* strip off resource */
	if ((p = strchr(buf, '/')))
		*p = 0;
	/* strip off domain */
	if ((p = strchr(buf, '@')))
		*p = 0;
	/* replace domain separator */
	if ((p = strchr(buf, domain_separator)))
		*p = '@';

	return buf;
}

/*! \brief  encode user@domain -> sip:user*domain@gateway_domain */
char *encode_uri_xmpp_sip(char *jid)
{
	static char buf[512];
	char *p;

	if (!jid)
		return NULL;
	/* TODO: maybe not modify jid? */
	if ((p = strchr(jid, '/')))
		*p = 0;
	if ((p = strchr(jid, '@')))
		*p = domain_separator;
	snprintf(buf, sizeof(buf), "sip:%s@%s", jid, gateway_domain);
	return buf;
}

char *extract_domain(char *jid)
{
	char *p;
	
	if ((p = strchr(jid, '/')))
		*p = 0;
	if ((p = strchr(jid, '@'))) {
		*p++ = 0;
		return p;
	}
	return p;
}

char *random_secret(void)
{
	static char secret[41];
	int i, r;

        for (i = 0; i < 40; i++) {
            r = (int) (36.0 * rand() / RAND_MAX);
            secret[i] = (r >= 0 && r <= 9) ? (r + 48) : (r + 87);
        }
        secret[40] = '\0';

	return secret;
}

char *db_key(char *secret, char *domain, char *id)
{
	char buf[1024];
	char *hash;
	
	snprintf(buf, sizeof(buf), "%s", secret);
	hash = shahash(buf);

	snprintf(buf, sizeof(buf), "%s%s", hash, domain);
	hash = shahash(buf);

	snprintf(buf, sizeof(buf), "%s%s", hash, id);
	hash = shahash(buf);
	return hash;
}

