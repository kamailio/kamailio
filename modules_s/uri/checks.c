/*
 * $Id$
 *
 * Various URI checks
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
 * 2003-02-26: Created by janakj
 * 2004-03-20: has_totag introduced (jiri)
 */

#include <string.h>
#include "../../str.h"
#include "../../dprint.h"               /* Debugging */
#include "../../parser/digest/digest.h" /* get_authorized_cred */
#include "../../parser/parse_from.h"
#include "../../parser/parse_uri.h"
#include "../../ut.h"                   /* Handy utilities */
#include "../../db/db.h"                /* Database API */
#include "uri_mod.h"
#include "checks.h"


/*
 * Checks if From includes a To-tag -- good to identify
 * if a request creates a new dialog
 */
int has_totag(struct sip_msg* _m, char* _foo, char* _bar)
{
	str tag;

	if (!_m->to && parse_headers(_m, HDR_TO,0)==-1) {
		LOG(L_ERR, "ERROR: has_totag: To parsing failed\n");
		return -1;
	}
	if (!_m->to) {
		LOG(L_ERR, "ERROR: has_totag: no To\n");
		return -1;
	}
	tag=get_to(_m)->tag_value;
	if (tag.s==0 || tag.len==0) {
		DBG("DEBUG: has_totag: no totag\n");
		return -1;
	}
	DBG("DEBUG: has_totag: totag found\n");
	return 1;
}


/*
 * Check if the username matches the username in credentials
 */
int is_user(struct sip_msg* _m, char* _user, char* _str2)
{
	str* s;
	struct hdr_field* h;
	auth_body_t* c;

	s = (str*)_user;

	get_authorized_cred(_m->authorization, &h);
	if (!h) {
		get_authorized_cred(_m->proxy_auth, &h);
		if (!h) {
			LOG(L_ERR, "is_user(): No authorized credentials found (error in scripts)\n");
			LOG(L_ERR, "is_user(): Call {www,proxy}_authorize before calling is_user function !\n");
			return -1;
		}
	}

	c = (auth_body_t*)(h->parsed);

	if (!c->digest.username.user.len) {
		DBG("is_user(): Username not found in credentials\n");
		return -1;
	}

	if (s->len != c->digest.username.user.len) {
		DBG("is_user(): Username length does not match\n");
		return -1;
	}

	if (!memcmp(s->s, c->digest.username.user.s, s->len)) {
		DBG("is_user(): Username matches\n");
		return 1;
	} else {
		DBG("is_user(): Username differs\n");
		return -1;
	}
}
