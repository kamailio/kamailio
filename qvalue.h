/*
 * $Id$
 *
 * Handling of the q value
 *
 * Copyright (C) 2004 FhG FOKUS
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
 * 2004-04-25 created (janakj)
 */

#ifndef _QVALUE_H
#define _QVALUE_H 1

#include <string.h>

/*
 * The q value expresses the priority of a URI within a set of URIs 
 * (Contact header field in the same SIP message or dset array in 
 * ser. The higher is the q value of a URI the higher is the priority 
 * of the URI.
 *
 * The q value is usually expressed as a floating point number with 
 * limited number of decimal digits, for example 0.346. RFC3261 allows 
 * 0-3 decimal digits.
 *
 * To speed things up we represent the q value as integer number, it 
 * is then easier to handle/print the value. To convert float into 
 * integer we multiply the q value by 1000, i.e. 
 * (float)0.567 == (int)567. In the opposite direction, values 
 * higher or equal to 1000 are converted to 1.0 and values below or 
 * equal to 0 are converted to 0.
 *
 * Value Q_UNSPECIFIED (which is in fact -1) has a special meaning, it 
 * means that the q value is not known and the parameter should not be 
 * printed when printing Contacts, implementations will then use 
 * implementation specific pre-defined values.
 */

typedef int qvalue_t;

/*
 * Use this if the value of q is not specified
 */
#define Q_UNSPECIFIED ((qvalue_t)-1)


#define MAX_Q ((qvalue_t)1000)
#define MIN_Q ((qvalue_t)0)

#define MAX_Q_STR "1"
#define MAX_Q_STR_LEN (sizeof(MAX_Q_STR) - 1)

#define MIN_Q_STR "0"
#define MIN_Q_STR_LEN (sizeof(MIN_Q_STR) - 1)

#define Q_PREFIX "0."
#define Q_PREFIX_LEN (sizeof(Q_PREFIX) - 1)


/*
 * Calculate the length of printed q
 */
static inline size_t len_q(qvalue_t q)
{
	if (q == Q_UNSPECIFIED) {
		return 0;
	} else if (q >= MAX_Q) {
		return MAX_Q_STR_LEN;
	} else if (q <= MIN_Q) {
		return MIN_Q_STR_LEN;
	} else if (q % 100 == 0) {
		return Q_PREFIX_LEN + 1;
	} else if (q % 10 == 0) {
		return Q_PREFIX_LEN + 2;
	} else {
		return Q_PREFIX_LEN + 3;
	}
}


/*
 * Print the q parameter
 */
static inline size_t print_q(char* p, qvalue_t q)
{
	if (q == Q_UNSPECIFIED) {
		return 0;
	} else if (q >= MAX_Q) {
		memcpy(p, MAX_Q_STR, MAX_Q_STR_LEN);
		return MAX_Q_STR_LEN;
	} else if (q <= MIN_Q) {
		memcpy(p, MIN_Q_STR, MIN_Q_STR_LEN);
		return MIN_Q_STR_LEN;
	}
	
	memcpy(p, Q_PREFIX, Q_PREFIX_LEN);
	p += Q_PREFIX_LEN;

	*p++ = q / 100 + '0';
	q %= 100;
	if (!q) return Q_PREFIX_LEN + 1;
	*p++ = q / 10 + '0';
	q %= 10;
	if (!q) return Q_PREFIX_LEN + 2;
	*p = q + '0';
	return Q_PREFIX_LEN + 3;
}


/*
 * Convert string representation of q parameter in qvalue_t
 */
int str2q(qvalue_t* q, char* s, int len);


#endif /* _QVALUE_H */
