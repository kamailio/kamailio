/*
 * Copyright (C) 2006 Vaclav Kubart, vaclav dot kubart at iptel dot org
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
 */

/*! \file
 * \brief Parser :: Parse subscription-state in NOTIFY
 *
 * \ingroup parser
 */

#include "parse_subscription_state.h"
#include "../dprint.h"
#include "../trim.h"
#include "../mem/mem.h"
#include "../ut.h"
#include "parser_f.h"
#include "parse_param.h"
#include <string.h>

void free_subscription_state(subscription_state_t**ss)
{
	if (ss) {
		if (*ss) pkg_free(*ss);
		*ss = 0;
	}
}

static inline int str_cmp(const str *a, const str *b)
{
	int i;
	
	if (a->len != b->len) return 1;
	
	for (i = 0; i < a->len; i++) 
		if (a->s[i] != b->s[i]) return 1;
	return 0;
}

static int ss_parse(str *src, subscription_state_t *ss)
{
	static str active = STR_STATIC_INIT("active");
	static str pending = STR_STATIC_INIT("pending");
	static str terminated = STR_STATIC_INIT("terminated");
	
	int res = 0;
	param_hooks_t ph;
	param_t *params;
	str s = *src;
	str state;
	char *c;
	
	/* initialization */
	ss->expires_set = 0;
	ss->expires = 0;
	
	trim_leading(&s);
		
	state = s;
	
	c = find_not_quoted(&s, ';');
	if (c) {
		/* first parameter starts after c */
		state.len = c - state.s;
		s.len = s.len - (c - s.s) - 1;
		s.s = c + 1;
	}
	else {
		s.len = 0;
	}

	/* set state value */
	trim(&state);
	if (str_cmp(&state, &active) == 0) {
		ss->value = ss_active;
	}
	else if (str_cmp(&state, &pending) == 0) {
		ss->value = ss_pending;
	}
	else if (str_cmp(&state, &terminated) == 0) {
		ss->value = ss_terminated;
	}
	else { 
		/* INFO("unknown subscription-State value :%.*s\n",
					state.len, state.s); */
		ss->value = ss_extension;
	}

	/* explore parameters */
	
	trim_leading(&s);
	if (s.len > 0) {
		params = NULL;
		if (parse_params(&s, CLASS_CONTACT, &ph, &params) < 0) {
			ERR("can't parse params\n");
			res = -1;
		}
		else {
			if (ph.contact.expires) {
				ss->expires_set = 1;
				res = str2int(&ph.contact.expires->body, &ss->expires);
				if (res != 0) 
					ERR("invalid expires value: \'%.*s\'\n", 
						ph.contact.expires->body.len,
						ph.contact.expires->body.s);
			}
			if (params) free_params(params);
		}
	}
	/*
	ss->value = ss_active;
	ss->expires = 0;*/


	return res;
}

int parse_subscription_state(struct hdr_field *h)
{
	subscription_state_t *ss;
	if (h->parsed) return 0;

	ss = (subscription_state_t*)pkg_malloc(sizeof(*ss));
	if (!ss) {
		ERR("No memory left\n");
		return -1;
	}

	memset(ss, 0, sizeof(*ss));

	if (ss_parse(&h->body, ss) < 0) {
		ERR("Can't parse Subscription-State\n");
		pkg_free(ss);
		return -2;
	}

	h->parsed = (void*)ss;
	
	return 0;
}
