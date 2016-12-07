/*
 * Accounting module
 *
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG FOKUS
 * Copyright (C) 2005 iptelorg GmbH
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _ATTRS_H
#define _ATTRS_H

#include <string.h>
#include "../../mem/mem.h"
#include "../../dprint.h"
#include "../../usr_avp.h"
#include "../../trim.h"
#include "../../str.h"
#include "../../ut.h"


/*
 * Parse the value of attrs parameter 
 */
static int parse_attrs(avp_ident_t** avps, int* avps_n, char* attrs)
{
	str token;

	token.s = strtok(attrs, ",");

	*avps = 0;
	*avps_n = 0;
	while(token.s) {
		token.len = strlen(token.s);
		trim(&token);
		
		if (token.len && token.s[0] == '$') {
			token.s++;
			token.len--;
		} else goto skip;

		*avps = pkg_realloc(*avps, sizeof(avp_ident_t) * (*avps_n + 1));
		if (!*avps) {
			ERR("No memory left\n");
			goto err;
		}

		if (parse_avp_ident(&token, &(*avps)[*avps_n]) < 0) {
			ERR("Error while parsing AVP id '%.*s'\n", token.len, ZSW(token.s));
			goto err;
		}
		DBG("Found attribute $%.*s\n", (*avps)[*avps_n].name.s.len, (*avps)[*avps_n].name.s.s);

		(*avps_n)++;
	skip:
		token.s = strtok(0, ",");
	}
	return 0;
 err:
	if (*avps) pkg_free(*avps);
	return -1;
}



#define attrs_append(dst, src)           \
do {                                     \
    if ((dst).len < (src).len) {         \
	ERR("Buffer too small\n");       \
	goto error;                      \
    }                                    \
    memcpy((dst).s, (src).s, (src).len); \
    (dst).s += (src).len;                \
    (dst).len -= (src).len;              \
} while(0)


/*
 * Escape delimiter characters
 */
#define attrs_append_esc(dst, src, esc_quote)               \
do {                                                        \
	int i;                                              \
	char* w;                                            \
	                                                    \
	if ((dst).len < ((src).len * 2)) {                  \
		ERR("Buffer too small\n");                  \
		goto error;                                 \
	}                                                   \
                                                            \
	w = (dst).s;                                        \
	for(i = 0; i < (src).len; i++) {                    \
		switch((src).s[i]) {                        \
		case '\n': *w++ = '\\'; *w++ = 'n';  break; \
		case '\r': *w++ = '\\'; *w++ = 'r';  break; \
		case '\t': *w++ = '\\'; *w++ = 't';  break; \
		case '\\': *w++ = '\\'; *w++ = '\\'; break; \
		case '\0': *w++ = '\\'; *w++ = '0';  break; \
                case '"':                                   \
                    if (esc_quote) {                        \
                        *w++ = '\\'; *w++ = 'q';            \
                    } else {                                \
                        *w++ = (src).s[i];                  \
                    }                                       \
                    break;                                  \
		case ':':  *w++ = '\\'; *w++ = 'o';  break; \
		case ',':  *w++ = '\\'; *w++ = 'c';  break; \
		default:   *w++ = (src).s[i];        break; \
		}                                           \
	}                                                   \
	(dst).len -= w - (dst).s;                           \
	(dst).s = w;                                        \
} while(0)


#define attrs_append_printf(dst, fmt, args...)              \
do {                                                        \
	int len = snprintf((dst).s, (dst).len, (fmt), ## args);	\
	if (len < 0 || len >= (dst).len) {                      \
		ERR("Buffer too small\n");                          \
		goto error;                                         \
	}                                                       \
	(dst).s += len;                                         \
	(dst).len -= len;                                       \
} while(0)



#define ATTRS_BUF_LEN 4096
static str* print_attrs(avp_ident_t* avps, int avps_n, int quote)
{
	static str quote_s = STR_STATIC_INIT("\"");
	static str attrs_name_delim = STR_STATIC_INIT(":");
	static str attrs_delim = STR_STATIC_INIT(",");
	static char buf[ATTRS_BUF_LEN];
	static str res;
	int i;
	struct search_state st;
	avp_value_t val;
	str p;

	p.s = buf;
	p.len = ATTRS_BUF_LEN - 1;

	if (quote && avps_n) {
		attrs_append(p, quote_s);
	}

	for(i = 0; i < avps_n; i++) {
		avp_t *this_avp = search_first_avp(avps[i].flags, avps[i].name, &val, &st);
		if (!this_avp) continue;
		attrs_append(p, avps[i].name.s);
		attrs_append(p, attrs_name_delim);
		if (this_avp->flags & AVP_VAL_STR)
			attrs_append_esc(p, val.s, quote);
		else
			attrs_append_printf(p, "%d", val.n);

		while(search_next_avp(&st, &val)) {
			attrs_append(p, attrs_delim);
			attrs_append(p, avps[i].name.s);
			attrs_append(p, attrs_name_delim);
			attrs_append_esc(p, val.s, quote);
		}
		if (i < (avps_n - 1)) attrs_append(p, attrs_delim); 
	}

	if (quote && avps_n) {
		attrs_append(p, quote_s);
	}

	*p.s = '\0';
	res.s = buf;
	res.len = p.s - buf;
	return &res;

 error:
	return 0;
}

#endif /* _ATTRS_H */
