/*
 * Copyright (C) 2026 toharishs@gmail.com
 *
 * The initial version of this code is written by Harish S
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
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

/* Matches an AOR/IMPU pair, including SIP/tel user-part comparison */

#include "impu_match.h"
#include <string.h>
#include "../../core/parser/parse_uri.h"

int pcscf_impu_matches_aor(str *aor, str *impu)
{
	struct sip_uri aor_uri, impu_uri;

	if(!aor || !aor->s || !impu || !impu->s)
		return 0;
	if(aor->len == impu->len && strncasecmp(aor->s, impu->s, aor->len) == 0)
		return 1;
	if(parse_uri(aor->s, aor->len, &aor_uri) != 0)
		return 0;
	if(parse_uri(impu->s, impu->len, &impu_uri) != 0)
		return 0;
	if(aor_uri.user.len && impu_uri.user.len
			&& aor_uri.user.len == impu_uri.user.len
			&& strncasecmp(aor_uri.user.s, impu_uri.user.s, aor_uri.user.len)
					   == 0)
		return 1;
	return 0;
}

int pcscf_contact_has_impu(pcontact_t *c, str *aor)
{
	ppublic_t *p;
	if(!c || !aor)
		return 0;
	if(pcscf_impu_matches_aor(aor, &c->aor))
		return 1;
	for(p = c->head; p; p = p->next)
		if(pcscf_impu_matches_aor(aor, &p->public_identity))
			return 1;
	return 0;
}

int is_impu_barred(pcontact_t *c, str *impu)
{
	ppublic_t *p;

	if(!c || !impu)
		return 0;
	for(p = c->head; p; p = p->next) {
		if(p->barred && pcscf_impu_matches_aor(impu, &p->public_identity))
			return 1;
	}
	return 0;
}
