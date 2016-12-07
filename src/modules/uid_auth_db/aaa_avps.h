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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef _SER_AAA_AVPS_H_
#define _SER_AAA_AVPS_H_

#include "../../mem/mem.h"
#include "../../parser/parser_f.h"
#include "../../dprint.h"

#include <string.h>

/*
 * Parse list of tokens separated by some char and put each token
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


/*
 * Parse the list of AVP names separated by '|' into an array
 * of names, each element of the array is str string
 */
static int
aaa_avps_init(str *avp_list, str **parsed_avps, int *avps_n)
{
	int errcode, i;
	char *cp;

	if (!avp_list->s || !avp_list->len) {
		     /* AVPs disabled, nothing to do */
		*avps_n = 0;
		return 1;
	}

	cp = pkg_malloc(avp_list->len + 1);
	if (cp == NULL) {
		LOG(L_ERR, "aaa_avps::aaa_avps_init(): can't allocate memory\n");
		errcode = -1;
		goto bad;
	}
	memcpy(cp, avp_list->s, avp_list->len);
	*avps_n = parse_token_list(cp, cp + avp_list->len, '|', parsed_avps);
	if (*avps_n == -1) {
		LOG(L_ERR, "aaa_avps::aaa_avps_init(): can't parse avps_column_int "
		    "parameter\n");
		errcode = -2;
		pkg_free(cp);
		goto bad;
	}

	for (i = 0; i < *avps_n; i++) {
		(*parsed_avps)[i].s[(*parsed_avps)[i].len] = '\0';
	}

	return 0;
bad:
	if (*parsed_avps != NULL) {
		pkg_free((*parsed_avps)[0].s);
		pkg_free(*parsed_avps);
	}

	return errcode;
}

#endif
