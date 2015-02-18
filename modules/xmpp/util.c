/*
 * XMPP Module
 * This file is part of Kamailio, a free SIP server.
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 * Author: Andreea Spirea
 *
 */
/*! \file
 * \brief Kamailio XMPP module - utilities
 *  \ingroup xmpp
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "xmpp.h"
#include "../../parser/parse_uri.h"
#include "../../parser/parse_param.h"

extern param_t *_xmpp_gwmap_list;

/*! \brief decode sip:user*domain1\@domain2 -> user\@domain1
 *     or based on gwmap sip:user\@sipdomain -> user\@xmppdomain
 *	\note In many kinds of gateway scenarios, the % sign is a common character used
 *		See the MSN XMPP transports for an example.
 */
char *decode_uri_sip_xmpp(char *uri)
{
	sip_uri_t puri;
	static char buf[512];
	char *p;
	param_t *it = NULL;

	if (!uri)
		return NULL;
	if (parse_uri(uri, strlen(uri), &puri) < 0) {
		LM_ERR("failed to parse URI\n");
		return NULL;
	}
	if(_xmpp_gwmap_list==0)
	{
		strncpy(buf, puri.user.s, sizeof(buf));
		buf[puri.user.len] = 0;
	
		/* replace domain separator */
		if ((p = strchr(buf, domain_separator)))
			*p = '@';
	} else {
		for(it=_xmpp_gwmap_list; it; it=it->next)
		{
			if(it->name.len==puri.host.len
					&& strncasecmp(it->name.s, puri.host.s, it->name.len)==0)
			{
				break;
			}
		}
		if(it && it->body.len>0)
		{
			snprintf(buf, 512, "%.*s@%.*s", puri.user.len, puri.user.s,
					it->body.len, it->body.s);
		} else {
			snprintf(buf, 512, "%.*s@%.*s", puri.user.len, puri.user.s,
					puri.host.len, puri.host.s);
		}
	}
	return buf;
}

/*! \brief  encode sip:user\@domain -> user*domain\@xmpp_domain
 *     or based on gwmap sip:user\@sipdomain -> user\@xmppdomain
 */
char *encode_uri_sip_xmpp(char *uri)
{
	sip_uri_t puri;
	static char buf[512];
	param_t *it = NULL;

	if (!uri)
		return NULL;
	if (parse_uri(uri, strlen(uri), &puri) < 0) {
		LM_ERR("failed to parse URI\n");
		return NULL;
	}
	if(_xmpp_gwmap_list==0)
	{
		snprintf(buf, sizeof(buf), "%.*s%c%.*s@%s",
			puri.user.len, puri.user.s,
			domain_separator,
			puri.host.len, puri.host.s,
			xmpp_domain);
	} else {
		for(it=_xmpp_gwmap_list; it; it=it->next)
		{
			if(it->name.len==puri.host.len
					&& strncasecmp(it->name.s, puri.host.s, it->name.len)==0)
			{
				break;
			}
		}
		if(it && it->body.len>0)
		{
			snprintf(buf, 512, "%.*s@%.*s", puri.user.len, puri.user.s,
					it->body.len, it->body.s);
		} else {
			snprintf(buf, 512, "%.*s@%.*s", puri.user.len, puri.user.s,
					puri.host.len, puri.host.s);
		}
	}
	return buf;
}

/*! \brief  decode user*domain1\@domain2 -> sip:user\@domain1
 *     or based on gwmap sip:user\@xmppdomain -> user\@sipdomain
 */
char *decode_uri_xmpp_sip(char *jid)
{
	static char buf[512];
	char *p;
	char tbuf[512];
	sip_uri_t puri;
	str sd;
	param_t *it = NULL;

	if (!jid)
		return NULL;

	if(_xmpp_gwmap_list==0)
	{
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
	} else {
		snprintf(tbuf, sizeof(tbuf), "sip:%s", jid);

		/* strip off resource */
		if ((p = strchr(tbuf, '/')))
			*p = 0;
		if (parse_uri(tbuf, strlen(tbuf), &puri) < 0) {
			LM_ERR("failed to parse URI\n");
			return NULL;
		}
		for(it=_xmpp_gwmap_list; it; it=it->next)
		{
			if(it->body.len>0)
			{
				sd = it->body;
			} else {
				sd = it->name;
			}
			if(sd.len==puri.host.len
					&& strncasecmp(sd.s, puri.host.s, sd.len)==0)
			{
				break;
			}
		}
		if(it)
		{
			snprintf(buf, 512, "sip:%.*s@%.*s", puri.user.len, puri.user.s,
					it->name.len, it->name.s);
		} else {
			snprintf(buf, 512, "sip:%.*s@%.*s", puri.user.len, puri.user.s,
					puri.host.len, puri.host.s);
		}

	}
	return buf;
}

/*! \brief  encode user\@domain -> sip:user*domain\@gateway_domain
 *     or based on gwmap sip:user\@xmppdomain -> user\@sipdomain
 */
char *encode_uri_xmpp_sip(char *jid)
{
	static char buf[512];
	char *p;
	char tbuf[512];
	sip_uri_t puri;
	str sd;
	param_t *it = NULL;

	if (!jid)
		return NULL;

	if(_xmpp_gwmap_list==0)
	{
		/* TODO: maybe not modify jid? */
		if ((p = strchr(jid, '/')))
			*p = 0;
		if ((p = strchr(jid, '@')))
			*p = domain_separator;
		snprintf(buf, sizeof(buf), "sip:%s@%s", jid, gateway_domain);
	} else {
		snprintf(tbuf, sizeof(tbuf), "sip:%s", jid);

		/* strip off resource */
		if ((p = strchr(tbuf, '/')))
			*p = 0;
		if (parse_uri(tbuf, strlen(tbuf), &puri) < 0) {
			LM_ERR("failed to parse URI\n");
			return NULL;
		}
		for(it=_xmpp_gwmap_list; it; it=it->next)
		{
			if(it->body.len>0)
			{
				sd = it->body;
			} else {
				sd = it->name;
			}
			if(sd.len==puri.host.len
					&& strncasecmp(sd.s, puri.host.s, sd.len)==0)
			{
				break;
			}
		}
		if(it)
		{
			snprintf(buf, 512, "sip:%.*s@%.*s", puri.user.len, puri.user.s,
					it->name.len, it->name.s);
		} else {
			snprintf(buf, 512, "sip:%.*s@%.*s", puri.user.len, puri.user.s,
					puri.host.len, puri.host.s);
		}

	}

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

