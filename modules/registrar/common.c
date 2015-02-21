/*
 * Common stuff
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

/*!
 * \file
 * \brief SIP registrar module
 * \ingroup registrar
 */

#include <string.h> 
#include "../../dprint.h"
#include "rerrno.h"
#include "reg_mod.h"
#include "common.h"
#include "config.h"

#define MAX_AOR_LEN 256

/*! \brief
 * Extract Address of Record
 */
int extract_aor(str* _uri, str* _a, sip_uri_t *_pu)
{
	static char aor_buf[MAX_AOR_LEN];
	str tmp;
	sip_uri_t turi;
	sip_uri_t *puri;
	int user_len;
	str *uri;
	str realm_prefix = {0};
	
	memset(aor_buf, 0, MAX_AOR_LEN);
	uri=_uri;

	if(_pu!=NULL)
		puri = _pu;
	else
		puri = &turi;

	if (parse_uri(uri->s, uri->len, puri) < 0) {
		rerrno = R_AOR_PARSE;
		LM_ERR("failed to parse AoR [%.*s]\n", uri->len, uri->s);
		return -1;
	}
	
	if ( (puri->user.len + puri->host.len + 1) > MAX_AOR_LEN
	|| puri->user.len > USERNAME_MAX_SIZE
	||  puri->host.len > DOMAIN_MAX_SIZE ) {
		rerrno = R_AOR_LEN;
		LM_ERR("Address Of Record too long\n");
		return -2;
	}

	_a->s = aor_buf;
	_a->len = puri->user.len;

	if (un_escape(&puri->user, _a) < 0) {
		rerrno = R_UNESCAPE;
		LM_ERR("failed to unescape username\n");
		return -3;
	}

	user_len = _a->len;

	if (reg_use_domain) {
		if (user_len)
			aor_buf[_a->len++] = '@';
		/* strip prefix (if defined) */
 		realm_prefix.len = cfg_get(registrar, registrar_cfg, realm_pref).len;
		if(realm_prefix.len>0) {
			realm_prefix.s = cfg_get(registrar, registrar_cfg, realm_pref).s;
			LM_DBG("realm prefix is [%.*s]\n", realm_prefix.len,
					(realm_prefix.len>0)?realm_prefix.s:"");
		}
		if (realm_prefix.len>0
				&& realm_prefix.len<puri->host.len
				&& (memcmp(realm_prefix.s, puri->host.s, realm_prefix.len)==0))
		{
			memcpy(aor_buf + _a->len, puri->host.s + realm_prefix.len,
					puri->host.len - realm_prefix.len);
			_a->len += puri->host.len - realm_prefix.len;
		} else {
			memcpy(aor_buf + _a->len, puri->host.s, puri->host.len);
			_a->len += puri->host.len;
		}
	}

	if (cfg_get(registrar, registrar_cfg, case_sensitive) && user_len) {
		tmp.s = _a->s + user_len + 1;
		tmp.len = _a->s + _a->len - tmp.s;
		strlower(&tmp);
	} else {
		strlower(_a);
	}

	return 0;
}
