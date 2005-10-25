/* 
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __SIMPLE_STR_H
#define __SIMPLE_STR_H

#ifdef __cplusplus
extern "C" {
#endif

/* If compiled for SER, use ser internal strings ! */
#ifdef SER

#include "str.h"	
typedef str str_t;

#else

typedef struct {
	char *s;
	int len;
} str_t;

#endif

#define FMT_STR(str)	(str).len,((str).s ? (str).s : "")	

/** transalate zero-terminated string to str_t (both uses the input buffer!)*/
str_t zt2str(char *str);

/** returns 1 if the string is empty */
int is_str_empty(const str_t *s);

/** duplicate string into given destination */
int str_dup(str_t* dst, const str_t* src);

/** duplicate string into newly allocated destination */
str_t *str_dup_new(const str_t* src);

/** duplicate zero-terminated string */
int str_dup_zt(str_t* dst, const char* src);

/** duplicate zero-terminated string to zero-terminated string */
char *zt_strdup(const char*src);

/** frees string content if allocated */
void str_free_content(str_t *s);

/** frees string content if allocated and then the string itself */
void str_free(str_t *s);

/** case sensitive comparation - returns 0 if equal, nonzero otherwise */
int str_case_equals(const str_t *a, const str_t *b);
/** case insensitive comparation - returns 0 if equal, nonzero otherwise */
int str_nocase_equals(const str_t *a, const str_t *b);

/** compare str_t and zero terminated string */
int str_cmp_zt(const str_t *a, const char *b); /* renamed sz_cmp */

/** is b prefix of a */
int str_prefix(const str_t *a, const str_t *b); /* ss_start */

/* #define ss_cmp(const str_t *a, const str_t *b) ((a->len == b->len)?sz_cmp(a, b->s):(-1)) */

void str_clear(str_t *s);

#ifdef __cplusplus
}
#endif

#endif
