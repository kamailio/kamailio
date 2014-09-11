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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __SIMPLE_STR_H
#define __SIMPLE_STR_H

#include <cds/memory.h>

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

#define STR_STATIC_INIT(v) {(v), sizeof(v) - 1}

#endif

#define FMT_STR(str)	(str).len,((str).s ? (str).s : "")	

#define str_len(ptr)	((ptr)?(ptr)->len:0)

/** transalate zero-terminated string to str_t (both uses the input buffer!)*/
str_t zt2str(char *str);

/** returns 1 if the string is empty */
int is_str_empty(const str_t *s);

/** duplicate string into given destination (data region is newly allocated) */
int str_dup_impl(str_t* dst, const str_t* src);
int str_dup_dbg(str_t* dst, const str_t* src, const char *file, int line);
/*#define str_dup(dst,src)	str_dup_dbg(dst,src,__FILE__,__LINE__)*/
#define str_dup(dst,src)	str_dup_impl(dst,src)


/** duplicate string into newly allocated destination (data and str structure are newly allocated) */
str_t *str_dup_new(const str_t* src);

/** duplicate zero-terminated string */
int str_dup_zt(str_t* dst, const char* src);

/** duplicate zero-terminated string to zero-terminated string */
char *zt_strdup(const char*src);

/** frees string content if allocated */
/* void str_free_content(str_t *s); */
#define str_free_content(str)	do { if (str != NULL) { \
		if (((str)->len > 0) && ((str)->s)) cds_free((str)->s);\
		(str)->len = 0; \
		(str)->s = 0; \
	} } while (0)

/** frees string content if allocated and then the string itself */
/* void str_free(str_t *s); */
#define str_free(str)	do { if (str != NULL) { \
		if (((str)->len > 0) && ((str)->s)) cds_free((str)->s);\
		cds_free(str); \
	} } while (0)

/* clears string content */
#define str_clear(str)	do { if (str != NULL) { \
		(str)->len = 0; \
		(str)->s = 0; \
	} } while (0)


/** case sensitive comparation - returns 0 if equal, nonzero otherwise */
int str_case_equals(const str_t *a, const str_t *b);
/** case insensitive comparation - returns 0 if equal, nonzero otherwise */
int str_nocase_equals(const str_t *a, const str_t *b);

/** compare str_t and zero terminated string */
int str_cmp_zt(const str_t *a, const char *b); /* renamed sz_cmp */

/** is b prefix of a */
int str_prefix(const str_t *a, const str_t *b); /* ss_start */

/* #define ss_cmp(const str_t *a, const str_t *b) ((a->len == b->len)?sz_cmp(a, b->s):(-1)) */

/* void str_clear(str_t *s); */

/** locate character in string */
char *str_strchr(const str_t *s, char c);

/** locate string in string */
char *str_str(const str_t *s, const str_t *search_for);

/* creates new string as concatenation of a and b */
int str_concat(str_t *dst, str_t *a, str_t *b);

int replace_str(const str_t *src, str_t *dst, const str_t *sample, const str_t *value);

/** Copies string into another one. The destination string buffer 
 * MUST be allocated in needed size! */
#define str_cpy(dst, src) do { \
	memcpy((dst)->s, (src)->s, (src)->len); \
	(dst)->len = (src)->len; \
	} while (0)

/* pointer after given string - often used when strings
 * allocated together with data structure holding them */
#define after_str_ptr(ss)	((ss)->s + (ss)->len)

/*
 * Append a string app with length app_len
 * to the end of string str which is a str* pointer
 * the buffer must be large enough
 */
#define str_append(str, app, app_len)                    \
    do {                                                 \
        memcpy((str)->s + (str)->len, (app), (app_len)); \
        (str)->len += (app_len);                         \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif
