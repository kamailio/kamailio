/*
 * $Id$
 *
 * Common stuff
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
 * History
 * ------
 * 2003-02-14 un-escaping added (janakj)
 *       

*/

#include <string.h> 
#include "../../dprint.h"
#include "../../ut.h"      /* q_memchr */
#include "../../parser/parse_uri.h"
#include "rerrno.h"
#include "reg_mod.h"
#include "common.h"


#define MAX_AOR_LEN 256

/*
 * Extract Address of Record
 */
int extract_aor(str* _uri, str* _a)
{
	static char aor_buf[MAX_AOR_LEN];
	str tmp;
	struct sip_uri puri;
	int user_len;

	if (parse_uri(_uri->s, _uri->len, &puri) < 0) {
		rerrno = R_AOR_PARSE;
		LOG(L_ERR, "extract_aor(): Error while parsing Address of Record\n");
		return -1;
	}
	
	if ((puri.user.len + puri.host.len + 1) > MAX_AOR_LEN) {
		rerrno = R_AOR_LEN;
		LOG(L_ERR, "extract_aor(): Address Of Record too long\n");
		return -2;
	}

	_a->s = aor_buf;
	_a->len = puri.user.len;

	if (un_escape(&puri.user, _a) < 0) {
		rerrno = R_UNESCAPE;
		LOG(L_ERR, "extract_aor(): Error while unescaping username\n");
		return -3;
	}

	user_len = _a->len;

	aor_buf[_a->len] = '@';
	/* ** stripping patch ** -jiri
	memcpy(aor_buf + _a->len + 1, puri.host.s, puri.host.len);
	_a->len += 1 + puri.host.len;
	*/
	if (realm_prefix.len && realm_prefix.len < puri.host.len &&
			(memcmp(realm_prefix.s, puri.host.s, realm_prefix.len) == 0)) {
		memcpy(aor_buf + _a->len + 1, puri.host.s + realm_prefix.len, puri.host.len - realm_prefix.len);
		_a->len += 1 + puri.host.len - realm_prefix.len;
	} else {
		 memcpy(aor_buf + _a->len + 1, puri.host.s, puri.host.len);
		 _a->len += 1 + puri.host.len;
	}
	/* end of stripptig patch */

	if (case_sensitive) {
		tmp.s = _a->s + user_len + 1;
		tmp.len = puri.host.len;
		strlower(&tmp);
	} else {
		strlower(_a);
	}

	return 0;
}
