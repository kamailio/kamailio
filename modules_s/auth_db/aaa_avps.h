/*
 * $Id$
 *
 * Common functions for Digest Authentication and Accounting Modules
 *
 * Copyright (C) 2001-2004 FhG Fokus
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
 */

#ifndef _SER_AAA_AVPS_H_
#define _SER_AAA_AVPS_H_

#include "../../mem/mem.h"
#include "../../parser/parser_f.h"
#include "../../dprint.h"

#include <string.h>

/*
 * Parse list of tokens separated by some char and put each tocken
 * into result array. Caller frees result array!
 */
static inline int
parse_token_list(char *p, char *pend, char separator, str **result)
{
	int i;

	i = 0;
	*result = NULL;
	while ((pend - p) > 0) {
		*result = pkg_realloc(*result, sizeof(**result) * (i + 1));
		if (*result == NULL)
			return -1;
		(*result)[i].s = p;
		p = eat_token2_end(p, pend, separator) + 1;
		(*result)[i].len = p - (*result)[i].s - 1;
		i++;
	}
	return i;
}

static inline int
aaa_avps_init(str *avps_column_int, str *avps_column_str,
    str **avps_int, str **avps_str, int *avps_int_n, int *avps_str_n)
{
	int errcode, i;
	char *cp;

	avps_column_int->len = strlen(avps_column_int->s);
	avps_column_str->len = strlen(avps_column_str->s);

	cp = pkg_malloc(avps_column_int->len + 1);
	if (cp == NULL) {
		LOG(L_ERR, "aaa_avps::aaa_avps_init(): can't allocate memory\n");
		errcode = -1;
		goto bad;
	}
	memcpy(cp, avps_column_int->s, avps_column_int->len);
	*avps_int_n = parse_token_list(cp, cp + avps_column_int->len, '|', avps_int);
	if (*avps_int_n == -1) {
		LOG(L_ERR, "aaa_avps::aaa_avps_init(): can't parse avps_column_int "
		    "parameter\n");
		errcode = -2;
		pkg_free(cp);
		goto bad;
	}
	cp = pkg_malloc(avps_column_str->len + 1);
	if (cp == NULL) {
		LOG(L_ERR, "aaa_avps::aaa_avps_init(): can't allocate memory\n");
		errcode = -3;
		goto bad;
	}
	memcpy(cp, avps_column_str->s, avps_column_str->len);
	*avps_str_n = parse_token_list(cp, cp + avps_column_str->len, '|', avps_str);
	if (*avps_str_n == -1) {
		LOG(L_ERR, "aaa_avps::aaa_avps_init(): can't parse avps_column_str "
		    "parameter\n");
		errcode = -4;
		pkg_free(cp);
		goto bad;
	}
	for (i = 0; i < *avps_int_n; i++)
		(*avps_int)[i].s[(*avps_int)[i].len] = '\0';
	for (i = 0; i < *avps_str_n; i++)
		(*avps_str)[i].s[(*avps_str)[i].len] = '\0';

	return 0;
bad:
	if (*avps_int != NULL) {
		pkg_free((*avps_int)[0].s);
		pkg_free(*avps_int);
	}
	if (*avps_str != NULL) {
		pkg_free((*avps_str)[0].s);
		pkg_free(*avps_str);
	}
	return errcode;
}

#endif
