/*
 * $Id$
 *
 * Remote-Party-ID related functions
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
 * --------
 * 2003-04-28 rpid contributed by Juha Heinanen added (janakj)
 */

#include <string.h>
#include "../../str.h"
#include "../../data_lump.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../parser/parse_to.h"
#include "../../parser/parse_uri.h"
#include "../../ut.h"
#include "auth_mod.h"
#include "api.h"
#include "rpid.h"


#define RPID_HF_NAME "Remote-Party-ID: "
#define RPID_HF_NAME_LEN (sizeof(RPID_HF_NAME)-1)


static str rpid;                /* rpid, stored in a backend authentication module */
static int rpid_is_e164;        /* 1 - yes, 0 - unknown, -1 - no */


/*
 * Copy of is_e164 from enum module
 */
static inline int is_e164(str* _user)
{
	int i;
	char c;
	
	if ((_user->len > 2) && (_user->len < 17) && ((_user->s)[0] == '+')) {
		for (i = 1; i <= _user->len; i++) {
			c = (_user->s)[i];
			if (c < '0' && c > '9') return -1;
		}
		return 1;
	}
	return -1;
}


/* 
 * Copy of append_hf_helper from textops.
 */
static inline int append_rpid_helper(struct sip_msg* _m, str *_s)
{
	struct lump* anchor;
	char *s;
	
	if (parse_headers(_m, HDR_EOH, 0) == -1) {
		LOG(L_ERR, "append_rpid(): Error while parsing message\n");
		return -1;
	}
	
	anchor = anchor_lump(&_m->add_rm, _m->unparsed - _m->buf, 0, 0);
	if (!anchor) {
		LOG(L_ERR, "append_rpid(): Can't get anchor\n");
		return -2;
	}
	
	s = pkg_malloc(_s->len);
	if (!s) {
		LOG(L_ERR, "append_rpid(): No memory left\n");
	}
	
	memcpy(s, _s->s, _s->len);
	if (!insert_new_lump_before(anchor, s, _s->len, 0)) {
		LOG(L_ERR, "append_rpid(): Can't insert lump\n");
		pkg_free(s);
		return -3;
	}

	return 0;
}


/*
 * Append RPID header field to the message
 */
int append_rpid_hf(struct sip_msg* _m, char* _s1, char* _s2)
{
	str rpid_hf;
	char *at;

	     /* No remote party ID, just return */
	if (!rpid.len) {
		DBG("append_rpid_hf(): rpid is empty, nothing to append\n");
		return 1;
	}
	
	rpid_hf.len = RPID_HF_NAME_LEN + rpid_prefix.len + rpid.len + rpid_suffix.len + CRLF_LEN;
	rpid_hf.s = pkg_malloc(rpid_hf.len);
	if (!rpid_hf.s) {
		LOG(L_ERR, "append_rpid_hf(): No memory left\n");
		return -1;
	}

	at = rpid_hf.s;
	memcpy(at, RPID_HF_NAME, RPID_HF_NAME_LEN);
	at += RPID_HF_NAME_LEN;

	memcpy(at, rpid_prefix.s, rpid_prefix.len);
	at += rpid_prefix.len;

	memcpy(at, rpid.s, rpid.len);
	at += rpid.len;

	memcpy(at, rpid_suffix.s, rpid_suffix.len);
	at += rpid_suffix.len;

	memcpy(at, CRLF, CRLF_LEN);

	append_rpid_helper(_m, &rpid_hf);
	pkg_free(rpid_hf.s);
	return 1;
}


/*
 * Check if SIP URI in rpid contains an e164 user part
 */
int is_rpid_user_e164(struct sip_msg* _m, char* _s1, char* _s2)
{
	struct to_body parsed;
	struct sip_uri uri;

	if (rpid_is_e164) return rpid_is_e164;

	if (!rpid.len) {
		DBG("is_rpid_user_e164(): Empty rpid\n");
		rpid_is_e164 = -1;
		return -1;
	}

	memset(&parsed, 0, sizeof(struct to_body));
	parse_to(rpid.s, rpid.s + rpid.len + 1, &parsed);
	
	if (parsed.error == PARSE_ERROR) {
		LOG(L_ERR, "is_rpid_user_e164(): Bad RPID header\n");
		rpid_is_e164 = -1;
		return -1;
	}
	
	if (parse_uri(parsed.uri.s, parsed.uri.len, &uri) < 0) {
		LOG(L_ERR, "is_rpid_user_e164(): Error while parsing RPID URI\n");
		rpid_is_e164 = -1;
		return -1;
	}

	rpid_is_e164 = (is_e164(&uri.user) ? 1 : -1);
	return rpid_is_e164;
}

	
/*
 * Process rpid
 * Will be alway called upon an authentication attempt
 */
void save_rpid(str* _rpid)
{
	rpid.s = 0;
	rpid.len = rpid_is_e164 = 0;

	if (!_rpid) {
		return;
	}
	
	rpid.s = _rpid->s;
	rpid.len = _rpid->len;
	DBG("save_rpid(): rpid value is '%.*s'\n", _rpid->len, ZSW(_rpid->s));
}
