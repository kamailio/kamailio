/*
 * $Id$
 *
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
 * History
 * ------
 * 2003-02-14 un-escaping added (janakj)
 * 2006-09-19 AOR may be provided via an AVP instead of being fetched
 *            from URI (bogdan)
*/

/*!
 * \file
 * \brief SIP registrar module
 * \ingroup registrar
 */

#include <string.h> 
#include "../../dprint.h"
#include "../../parser/parse_uri.h"
#include "rerrno.h"
#include "reg_mod.h"
#include "common.h"
#include "config.h"

#define MAX_AOR_LEN 256

/*! \brief
 * Extract Address of Record
 */
int extract_aor(str* _uri, str* _a) {
	static char aor_buf[MAX_AOR_LEN];
	str tmp;
	struct sip_uri puri;
	int user_len;
	int_str avp_val;
	struct usr_avp *avp;
	str *uri;
	str realm_prefix;

	memset(aor_buf, 0, MAX_AOR_LEN);
	if (aor_avp_name.n != 0) {
		avp = search_first_avp(aor_avp_type, aor_avp_name, &avp_val, 0);
		if (avp && is_avp_str_val(avp)) {
			uri = &avp_val.s;
		} else {
			uri = _uri;
		}
	} else {
		uri = _uri;
	}

	if (parse_uri(uri->s, uri->len, &puri) < 0) {
		rerrno = R_AOR_PARSE;
		LM_ERR("failed to parse Address of Record\n");
		return -1;
	}

	if ((puri.user.len + puri.host.len + 1 + 4) > MAX_AOR_LEN
			|| puri.user.len > USERNAME_MAX_SIZE
			|| puri.host.len > DOMAIN_MAX_SIZE) {
		rerrno = R_AOR_LEN;
		LM_ERR("Address Of Record too long\n");
		return -2;
	}

	_a->s = aor_buf;
	_a->len = puri.user.len + 4;
	strncpy(_a->s, "sip:", 4);

	str tmps;
	tmps.s = _a->s + 4;
	tmps.len = puri.user.len;

	if (un_escape(&puri.user, &tmps) < 0) {
		rerrno = R_UNESCAPE;
		LM_ERR("failed to unescape username\n");
		return -3;
	}

	user_len = tmps.len + 4;//_a->len;

	if (user_len>4)
		aor_buf[_a->len++] = '@';
	/* strip prefix (if defined) */
	realm_prefix.s = cfg_get(registrar, registrar_cfg, realm_pref);
	realm_prefix.len = strlen(realm_prefix.s);
	if (realm_prefix.len && realm_prefix.len < puri.host.len
			&& (memcmp(realm_prefix.s, puri.host.s, realm_prefix.len) == 0)) {
		memcpy(aor_buf + _a->len, puri.host.s + realm_prefix.len,
				puri.host.len - realm_prefix.len);
		_a->len += puri.host.len - realm_prefix.len;
	} else {
		memcpy(aor_buf + _a->len, puri.host.s, puri.host.len);
		_a->len += puri.host.len;
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
