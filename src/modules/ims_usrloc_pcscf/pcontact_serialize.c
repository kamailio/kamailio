/*
 * Copyright (C) 2026 toharishs@gmail.com
 *
 * The initial version of this code is written by Harish S
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

/* Serialize/parse the "<impu1><impu2>..." list format used for pcontact IMPU columns */

#include "pcontact_serialize.h"
#include <string.h>

int pcscf_serialize_impus(str *impus, int n, str *out, int out_size)
{
	int i;
	int needed = 0;

	if(!impus || !out)
		return -1;

	for(i = 0; i < n; i++) {
		needed += 2; /* '<' and '>' */
		needed += impus[i].len;
	}
	if(needed > out_size)
		return -1;

	/* write */
	out->len = 0;
	for(i = 0; i < n; i++) {
		if(out->len + 1 + impus[i].len + 1 > out_size)
			return -1;
		out->s[out->len++] = '<';
		if(impus[i].len > 0) {
			memcpy(out->s + out->len, impus[i].s, impus[i].len);
			out->len += impus[i].len;
		}
		out->s[out->len++] = '>';
	}
	return 0;
}

int pcscf_serialize_impus_barred(str *barred, int n, str *out, int out_size)
{
	/* same as serialize_impus but for barred list */
	return pcscf_serialize_impus(barred, n, out, out_size);
}

int pcscf_parse_impus(str *in, str *parsed, int max)
{
	int i = 0;
	int pos = 0;
	if(!in || !parsed || max <= 0)
		return 0;
	while(pos < in->len && i < max) {
		/* find '<' */
		while(pos < in->len && in->s[pos] != '<')
			pos++;
		if(pos >= in->len)
			break;
		int start = pos + 1;
		pos = start;
		/* find '>' */
		while(pos < in->len && in->s[pos] != '>')
			pos++;
		if(pos >= in->len)
			break;
		parsed[i].s = in->s + start;
		parsed[i].len = pos - start;
		i++;
		pos++; /* move past '>' */
	}
	return i;
}

static int str_case_eq(str *a, str *b)
{
	return (a->len == b->len) && (strncasecmp(a->s, b->s, a->len) == 0);
}

int pcscf_apply_barred_flags(
		str *impus, int n, str *barred, int n_barred, char *flags)
{
	int i, j;

	if(!impus || !flags)
		return -1;
	memset(flags, 0, n);
	if(!barred || n_barred <= 0)
		return 0;
	for(i = 0; i < n; i++)
		for(j = 0; j < n_barred; j++)
			if(str_case_eq(&impus[i], &barred[j]))
				flags[i] = 1;
	return 0;
}
