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

#include <cds/sstr.h>
#include <cds/memory.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/** returns 1 if the string is empty */
int is_str_empty(const str_t *s)
{
	if (!s) return 1;
	if ((!s->s) || (s->len < 1)) return 1;
	return 0;
}

int str_cmp_zt(const str_t *a, const char *b)
{
	int i;
	
	if (!a) {
		if (b) return 1;
		else return 0;
	}
	
	for (i = 0; (i < a->len) && (b[i]); i++) {
		if (a->s[i] < b[i]) return -1;
		if (a->s[i] > b[i]) return 1;
	}
	if (i < a->len) return 1;
	return 0;
}

int str_prefix(const str_t *a, const str_t *b)
{
	int i;
	if (!b) return 0;
	if (!a) return -1;

	if (b->len > a->len) return -1;
	for (i = 0; i < b->len; i++) {
		if (a->s[i] != b->s[i]) return -1;
	}
	return 0;
}

str_t zt2str(char *str)
{
	str_t s;

	s.s = str;
	if (str) s.len = strlen(str);
	else s.len = 0;
	return s;
}

int str_dup(str_t* dst, const str_t* src)
{
	if (!dst) return -1;

	dst->len = 0;
	dst->s = NULL;
	if (!src) return 0;
	if ( (!src->s) || (src->len < 1)) return 0;

	dst->s = cds_malloc(src->len);
	if (!dst->s) return -1;
	memcpy(dst->s, src->s, src->len);
	dst->len = src->len;
	return 0;
}

str_t *str_dup_new(const str_t* src)
{
	str_t *dst = cds_malloc(sizeof(str_t));
	if (dst) str_dup(dst, src);
	return dst;
}

int str_dup_zt(str_t* dst, const char* src)
{
	int len;
	if (!dst) return -1;

	dst->len = 0;
	dst->s = NULL;
	if (!src) return 0;
	len = strlen(src);
	if (len < 1) return 0;

	dst->s = cds_malloc(len);
	if (!dst->s) return -1;
	memcpy(dst->s, src, len);
	dst->len = len;
	return 0;
}

char *zt_strdup(const char* src)
{
	int len;
	char *dst;

	len = strlen(src);
	if (len < 0) return NULL;

	dst = cds_malloc(len + 1);
	if (dst) memcpy(dst, src, len + 1);
	return dst;
}

int str_nocase_equals(const str_t *a, const str_t *b)
{
	int i;
	
	if (!a) {
		if (!b) return 0;
		else return (b->len == 0) ? 0 : 1;
	}
	if (!b) return (a->len == 0) ? 0 : 1;
	if (a->len != b->len) return 1;
	
	for (i = 0; i < a->len; i++) 
		if (tolower(a->s[i]) != tolower(b->s[i])) return 1;
	return 0;
}

int str_case_equals(const str_t *a, const str_t *b)
{
	int i;
	
	if (!a) {
		if (!b) return 0;
		else return (b->len == 0) ? 0 : 1;
	}
	if (!b) return (a->len == 0) ? 0 : 1;
	if (a->len != b->len) return 1;
	
	for (i = 0; i < a->len; i++) 
		if (a->s[i] != b->s[i]) return 1;
	return 0;
}

/* void str_free_content(str_t *s)
{
	if (!s) return;
	if ((s->len > 0) && (s->s)) cds_free(s->s);
	s->len = 0;
	s->s = NULL;
} 

void str_free(str_t *s)
{
	if (s) {
		str_free_content(s);
		cds_free(s);
	}
}*/

void str_clear(str_t *s)
{
	if (s) {
		s->s = NULL;
		s->len = 0;
	}
}

char *str_strchr(const str_t *s, char c)
{
	if (s) {
		int i;
		for (i = 0; i < s->len; i++)
			if (s->s[i] == c) return s->s + i;
	}
	return NULL;
}

