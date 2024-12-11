/*
 * pv_headers
 *
 * Copyright (C)
 * 2020-2023 Victor Seva <vseva@sipwise.com>
 * 2018 Kirill Solomko <ksolomko@sipwise.com>
 *
 * This file is part of Kamailio, a free SIP server.
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

#include "pvh_str.h"

int pvh_str_new(str *s, int size)
{
	s->s = (char *)pkg_malloc(size);
	if(s->s == NULL) {
		PKG_MEM_ERROR;
		return -1;
	}
	memset(s->s, 0, size);
	s->len = 0;

	return 1;
}

int pvh_str_free(str *s)
{
	if(s->s)
		pkg_free(s->s);
	s->s = NULL;
	return 1;
}

int pvh_str_copy(str *dst, str *src, unsigned int max_size)
{
	unsigned int src_len = src->len + 1 >= max_size ? max_size - 1 : src->len;

	if(src == NULL || dst == NULL || src->len <= 0)
		return -1;

	memset(dst->s, 0, dst->len);
	memcpy(dst->s, src->s, src_len);
	dst->s[src_len] = '\0';
	dst->len = src_len;

	return 1;
}

int pvh_extract_display_uri(char *suri, str *display, str *duri)
{
	char *ptr_a = NULL;
	char *ptr_b = NULL;
	int display_len = 0;
	int uri_len = 0;

	if(suri == NULL || strlen(suri) == 0)
		return -1;

	ptr_a = strchr(suri, '<');
	ptr_b = strchr(suri, '>');

	if(ptr_a == NULL && ptr_b == NULL) {
		ptr_a = suri;
		uri_len = strlen(suri);
	} else if(ptr_a == NULL || ptr_b == NULL) {
		return -1;
	} else {
		display_len = ptr_a - suri;
		ptr_a++;
		uri_len = ptr_b - ptr_a;
	}

	if(uri_len <= 0)
		return -1;

	if(display_len > 0) {
		memcpy(display->s, suri, display_len);
		display->len = strlen(display->s);
		display->s[display->len] = '\0';
	} else {
		display->len = 0;
	}

	memcpy(duri->s, ptr_a, uri_len);
	duri->len = strlen(duri->s);
	duri->s[duri->len] = '\0';

	return 1;
}

char *pvh_detect_split_char(char *val)
{
	char *quote_a = NULL, *quote_b = NULL;
	char *split = NULL;

	if(val == NULL)
		return NULL;

	split = strchr(val, ',');
	if(split == NULL) {
		LM_DBG("no split marker detected\n");
		return NULL;
	}

	quote_a = strchr(val, '"');
	if(quote_a == NULL || split < quote_a) {
		LM_DBG("split marker detected[%ld], not between quotes\n", split - val);
		return split;
	}

	quote_b = strchr(val + (split - quote_a + 1), '"');
	if(quote_b == NULL) {
		LM_DBG("split marker detected[%ld], quote occurrence unbalanced[%ld]\n",
				split - val, quote_b - val);
		return split;
	}
	return pvh_detect_split_char(val + (quote_b - val + 1));
}

int pvh_split_values(str *s, char d[][_pvh_params.hdr_value_size], int *d_size,
		int keep_spaces, char *marker)
{
	char *p = NULL;
	int idx = 0, c_idx = 0;

	*d_size = -1;

	if(s == NULL || s->len == 0 || d == NULL) {
		*d_size = 0;
		return 1;
	}
	if(!marker)
		marker = pvh_detect_split_char(s->s);
	while(idx < s->len) {
		p = s->s + idx++;
		if(keep_spaces == 0 && strncmp(p, " ", 1) == 0)
			continue;
		if(p == marker) {
			if(marker && idx < s->len) {
				LM_DBG("search next split marker[%d]\n", idx);
				marker = pvh_detect_split_char(p + 1);
			}
			if(c_idx == 0)
				continue;
			if(c_idx + 1 < _pvh_params.hdr_value_size)
				c_idx++;
			d[*d_size][c_idx] = '\0';
			c_idx = 0;
			continue;
		}
		if(c_idx == 0)
			(*d_size)++;
		strncpy(&d[*d_size][c_idx++], p, 1);
	}

	if(c_idx > 0) {
		if(c_idx >= _pvh_params.hdr_value_size)
			c_idx--;
		d[*d_size][c_idx] = '\0';
	}

	(*d_size)++;

	return 1;
}
