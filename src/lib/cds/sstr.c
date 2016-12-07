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

#include <cds/sstr.h>
#include <cds/memory.h>
#include <cds/dstring.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <cds/logger.h>

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

int str_dup_dbg(str_t* dst, const str_t* src, const char *file, int line)
{
	if (!dst) return -1;

	dst->len = 0;
	dst->s = NULL;
	if (!src) return 0;
	if ( (!src->s) || (src->len < 1)) return 0;

		/* ERROR_LOG("can't allocate memory (%d bytes)\n", src->len); */
	DEBUG_LOG("str_dup called from %s:%d\n", file, line);
	dst->s = cds_malloc(src->len);
	if (!dst->s) {
		/* ERROR_LOG("can't allocate memory (%d bytes)\n", src->len); */
		return -1;
	}
	memcpy(dst->s, src->s, src->len);
	dst->len = src->len;
	return 0;
}

int str_dup_impl(str_t* dst, const str_t* src)
{
	if (!dst) return -1;

	dst->len = 0;
	dst->s = NULL;
	if (!src) return 0;
	if ( (!src->s) || (src->len < 1)) return 0;

	dst->s = cds_malloc(src->len);
	if (!dst->s) {
		/* ERROR_LOG("can't allocate memory (%d bytes)\n", src->len); */
		return -1;
	}
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
}

void str_clear(str_t *s)
{
	if (s) {
		s->s = NULL;
		s->len = 0;
	}
} */

char *str_strchr(const str_t *s, char c)
{
	if (s) {
		int i;
		for (i = 0; i < s->len; i++)
			if (s->s[i] == c) return s->s + i;
	}
	return NULL;
}

char *str_str(const str_t *s, const str_t *search_for)
{
	int i, j;
	/* FIXME: reimplement using better algorithm */

	if (is_str_empty(search_for)) return s->s;
	if (is_str_empty(s)) return NULL;
	
	if (search_for->len > s->len) return NULL;

	j = 0;
	i = 0;
	while (i < s->len) {
		if (s->s[i] == search_for->s[j]) {
			j++;
			i++;
			if (j == search_for->len) return s->s + i - j;
		}
		else {
			i = i - j + 1;
			j = 0;
		}
	}
	return NULL;
}

/* creates new string as concatenation of a and b */
int str_concat(str_t *dst, str_t *a, str_t *b)
{
	int al;
	int bl;
	
	if (!dst) return -1;
	
	al = str_len(a);
	bl = str_len(b);
	
	dst->len = al + bl;
	if (dst->len > 0) {
		dst->s = (char *)cds_malloc(dst->len);
		if (!dst->s) {
			dst->len = 0;
			return -1;
		}
	}
	else {
		dst->s = NULL;
		dst->len = 0;
		return 0;
	}
	
	if (al) memcpy(dst->s, a->s, al);
	if (bl) memcpy(dst->s + al, b->s, bl);
	
	return 0;
}

int replace_str(const str_t *src, str_t *dst, const str_t *sample, const str_t *value)
{
	str_t s;
	char *c;
	dstring_t str;
	int res, len;
	
	/* if (!dst) return -1;
	 if (!src) {
		str_clear(dst);
		return -1; 
	} */

	if (is_str_empty(sample)) {
		str_clear(dst);
		return -1;
	}

	if (is_str_empty(src)) {
		str_clear(dst);
		return 0;
	}
	
	s = *src;
	dstr_init(&str, src->len + 32);
	do {
		c = str_str(&s, sample);
		if (c) {
			len = c - s.s;
			dstr_append(&str, s.s, len);
			dstr_append_str(&str, value);
			s.len = s.len - len - sample->len;
			s.s = c + sample->len;
			if (s.len <= 0) break;
		}
		else dstr_append_str(&str, &s);
	} while (c);
	
	res = dstr_get_str(&str, dst);
	dstr_destroy(&str);
	return res;
}
