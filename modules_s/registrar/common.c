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
 */

#include <string.h> 
#include <ctype.h>
#include "../../dprint.h"
#include "../../ut.h"      /* q_memchr */
#include "../../parser/parse_uri.h"
#include "common.h"
#include "rerrno.h"
#include "reg_mod.h"


/*
 * Find a character occurence that is not quoted
 */
char* find_not_quoted(str* _s, char _c)
{
	int quoted = 0, i;
	
	for(i = 0; i < _s->len; i++) {
		if (!quoted) {
			if (_s->s[i] == '\"') quoted = 1;
			else if (_s->s[i] == _c) return _s->s + i;
		} else {
			if ((_s->s[i] == '\"') && (_s->s[i - 1] != '\\')) quoted = 0;
		}
	}
	return 0;
}


/*
 * Extract username part from URI
 */
int get_username(str* _s)
{
	char* at, *dcolon, *dc;
	dcolon = find_not_quoted(_s, ':');

	if (dcolon == 0) return -1;

	_s->s = dcolon + 1;
	_s->len -= dcolon - _s->s + 1;
	
	at = q_memchr(_s->s, '@', _s->len);
	dc = q_memchr(_s->s, ':', _s->len);
	if (at) {
		if ((dc) && (dc < at)) {
			_s->len = dc - _s->s;
			return 0;
		}
		
		_s->len = at - _s->s;
		return 0;
	} else return -2;
}


#define MAX_AOR_LEN 256
char aor_buf[MAX_AOR_LEN];


static inline void strlower(str* _s)
{
	int i;

	for(i = 0; i < _s->len; i++) {
		_s->s[i] = tolower(_s->s[i]);
	}
}


/*
 * Extract Address Of Record
 */
int extract_aor(struct sip_msg* _m, str* _a)
{
	str aor;
	struct sip_uri puri;

	aor = ((struct to_body*)_m->to->parsed)->uri;

	if (use_domain) {
		if (parse_uri(aor.s, aor.len, &puri) < 0) {
			rerrno = R_AOR_PARSE;
			LOG(L_ERR, "extract_aor(): Error while parsing AOR, sending 400\n");
			return -1;
		}

		_a->s = aor_buf;
		_a->len = puri.user.len + puri.host.len + 1;

		if (_a->len > MAX_AOR_LEN) {
			rerrno = R_AOR_LEN;
			LOG(L_ERR, "extract_aor(): Address Of Record too long, sending 500\n");
			return -2;
		}

		memcpy(aor_buf, puri.user.s, puri.user.len);
		aor_buf[puri.user.len] = '@';
		memcpy(aor_buf + puri.user.len + 1, puri.host.s, puri.host.len);

		if (case_sensitive) {
			aor.s = _a->s + puri.user.len + 1;
			aor.len = puri.host.len;
			strlower(&aor);
		} else {
			strlower(_a);
		}
	} else {
		if (get_username(&aor) < 0) {
			rerrno = R_TO_USER;
			LOG(L_ERR, "extract_aor(): Can't extract username part from To URI, sending 400\n");
			return -3;
		}
		_a->len = aor.len;

		if (case_sensitive) {
			_a->s = aor.s;
		} else {
			if (aor.len > MAX_AOR_LEN) {
				rerrno = R_AOR_LEN;
				LOG(L_ERR, "extract_aor(): Username too long, sending 500\n");
				return -4;
			}
			memcpy(aor_buf, aor.s, aor.len);
			_a->s = aor_buf;
			strlower(_a);
		}
	}

	return 0;
}
